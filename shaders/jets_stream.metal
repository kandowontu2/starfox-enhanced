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

constant spvUnsafeArray<int2, 8> _2236 = spvUnsafeArray<int2, 8>({ int2(500, 0), int2(-500, 0), int2(0, 500), int2(0, -500), int2(612), int2(-612, 612), int2(612, -612), int2(-612) });
constant spvUnsafeArray<float, 9> _2288 = spvUnsafeArray<float, 9>({ 1.0, -0.16666667163372039794921875, 0.008333333767950534820556640625, -0.00019841270113829523324966430664063, 2.7557318844628753140568733215332e-06, -2.5052107943679402524139732122421e-08, 1.6059044372074282591711380518973e-10, -7.6471636098127127034729255683487e-13, 2.8114573589663703623298118827734e-15 });

static inline __attribute__((always_inline))
bool feature_valid(thread const float3& f, thread const uint& kind)
{
    bool3 _3012 = isnan(f);
    bool3 _3013 = isinf(f);
    if (!all(not(bool3(_3012.x || _3013.x, _3012.y || _3013.y, _3012.z || _3013.z))))
    {
        return false;
    }
    if (kind == 1u)
    {
        bool _3027;
        if (f.z == 0.0)
        {
            _3027 = all(f.xy >= float2(0.0));
        }
        else
        {
            _3027 = false;
        }
        bool _3033;
        if (_3027)
        {
            _3033 = spvFAdd(f.x, f.y) <= 1.0;
        }
        else
        {
            _3033 = false;
        }
        return _3033;
    }
    bool _3038;
    if (kind != 2u)
    {
        _3038 = kind == 3u;
    }
    else
    {
        _3038 = true;
    }
    bool _3043;
    if (_3038)
    {
        _3043 = abs(spvFSub(dot(f, f), 1.0)) < 0.00010099999781232327222824096679688;
    }
    else
    {
        _3043 = false;
    }
    return _3043;
}

static inline __attribute__((always_inline))
bool reflection_specular_plane_valid(thread const ReflectionSpecularPlane& plane)
{
    bool _3054;
    if (plane.a.w == 2.0)
    {
        _3054 = plane.b.w == 2.0;
    }
    else
    {
        _3054 = false;
    }
    bool _3059;
    if (_3054)
    {
        _3059 = plane.c.w == 2.0;
    }
    else
    {
        _3059 = false;
    }
    bool _3076;
    if (!_3059)
    {
        bool _3069;
        if ((isunordered(plane.a.w, 1.0) || plane.a.w == 1.0))
        {
            _3069 = plane.b.w != 1.0;
        }
        else
        {
            _3069 = true;
        }
        bool _3075;
        if (!_3069)
        {
            _3075 = plane.c.w != 1.0;
        }
        else
        {
            _3075 = true;
        }
        _3076 = _3075;
    }
    else
    {
        _3076 = false;
    }
    bool _3086;
    if (!_3076)
    {
        bool4 _3080 = isnan(plane.a);
        bool4 _3081 = isinf(plane.a);
        _3086 = !all(not(bool4(_3080.x || _3081.x, _3080.y || _3081.y, _3080.z || _3081.z, _3080.w || _3081.w)));
    }
    else
    {
        _3086 = true;
    }
    bool _3096;
    if (!_3086)
    {
        bool4 _3090 = isnan(plane.b);
        bool4 _3091 = isinf(plane.b);
        _3096 = !all(not(bool4(_3090.x || _3091.x, _3090.y || _3091.y, _3090.z || _3091.z, _3090.w || _3091.w)));
    }
    else
    {
        _3096 = true;
    }
    bool _3106;
    if (!_3096)
    {
        bool4 _3100 = isnan(plane.c);
        bool4 _3101 = isinf(plane.c);
        _3106 = !all(not(bool4(_3100.x || _3101.x, _3100.y || _3101.y, _3100.z || _3101.z, _3100.w || _3101.w)));
    }
    else
    {
        _3106 = true;
    }
    bool _3114;
    if (!_3106)
    {
        _3114 = any(abs(plane.a.xyz) > float3(999999995904.0));
    }
    else
    {
        _3114 = true;
    }
    bool _3122;
    if (!_3114)
    {
        _3122 = any(abs(plane.b.xyz) > float3(999999995904.0));
    }
    else
    {
        _3122 = true;
    }
    bool _3130;
    if (!_3122)
    {
        _3130 = any(abs(plane.c.xyz) > float3(999999995904.0));
    }
    else
    {
        _3130 = true;
    }
    if (_3130)
    {
        return false;
    }
    bool _3136;
    if (_3059)
    {
        _3136 = any(plane.c.xyz != float3(0.0));
    }
    else
    {
        _3136 = false;
    }
    if (_3136)
    {
        return false;
    }
    float3 _3153;
    if (_3059)
    {
        _3153 = plane.b.xyz;
    }
    else
    {
        _3153 = cross(spvFSub(plane.b.xyz, plane.a.xyz), spvFSub(plane.c.xyz, plane.a.xyz));
    }
    float _1612 = dot(_3153, _3153);
    bool3 _3154 = isnan(_3153);
    bool3 _3155 = isinf(_3153);
    bool _3163;
    if (all(not(bool3(_3154.x || _3155.x, _3154.y || _3155.y, _3154.z || _3155.z))))
    {
        _3163 = !(isnan(_1612) || isinf(_1612));
    }
    else
    {
        _3163 = false;
    }
    bool _3165;
    if (_3163)
    {
        _3165 = _1612 > 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3165 = false;
    }
    return _3165;
}

static inline __attribute__((always_inline))
bool reflection_rough_frame_valid(thread const ReflectionRoughFrame& old)
{
    bool _3176;
    if (old.a.w == 2.0)
    {
        _3176 = old.b.w == 2.0;
    }
    else
    {
        _3176 = false;
    }
    bool _3181;
    if (_3176)
    {
        _3181 = old.c.w == 2.0;
    }
    else
    {
        _3181 = false;
    }
    bool _3198;
    if (!_3181)
    {
        bool _3191;
        if ((isunordered(old.a.w, 1.0) || old.a.w == 1.0))
        {
            _3191 = old.b.w != 1.0;
        }
        else
        {
            _3191 = true;
        }
        bool _3197;
        if (!_3191)
        {
            _3197 = old.c.w != 1.0;
        }
        else
        {
            _3197 = true;
        }
        _3198 = _3197;
    }
    else
    {
        _3198 = false;
    }
    bool _3208;
    if (!_3198)
    {
        bool4 _3202 = isnan(old.a);
        bool4 _3203 = isinf(old.a);
        _3208 = !all(not(bool4(_3202.x || _3203.x, _3202.y || _3203.y, _3202.z || _3203.z, _3202.w || _3203.w)));
    }
    else
    {
        _3208 = true;
    }
    bool _3218;
    if (!_3208)
    {
        bool4 _3212 = isnan(old.b);
        bool4 _3213 = isinf(old.b);
        _3218 = !all(not(bool4(_3212.x || _3213.x, _3212.y || _3213.y, _3212.z || _3213.z, _3212.w || _3213.w)));
    }
    else
    {
        _3218 = true;
    }
    bool _3228;
    if (!_3218)
    {
        bool4 _3222 = isnan(old.c);
        bool4 _3223 = isinf(old.c);
        _3228 = !all(not(bool4(_3222.x || _3223.x, _3222.y || _3223.y, _3222.z || _3223.z, _3222.w || _3223.w)));
    }
    else
    {
        _3228 = true;
    }
    bool _3238;
    if (!_3228)
    {
        bool4 _3232 = isnan(old.projection);
        bool4 _3233 = isinf(old.projection);
        _3238 = !all(not(bool4(_3232.x || _3233.x, _3232.y || _3233.y, _3232.z || _3233.z, _3232.w || _3233.w)));
    }
    else
    {
        _3238 = true;
    }
    bool _3248;
    if (!_3238)
    {
        bool4 _3242 = isnan(old.extentClip);
        bool4 _3243 = isinf(old.extentClip);
        _3248 = !all(not(bool4(_3242.x || _3243.x, _3242.y || _3243.y, _3242.z || _3243.z, _3242.w || _3243.w)));
    }
    else
    {
        _3248 = true;
    }
    bool _3258;
    if (!_3248)
    {
        bool4 _3252 = isnan(old.settings);
        bool4 _3253 = isinf(old.settings);
        _3258 = !all(not(bool4(_3252.x || _3253.x, _3252.y || _3253.y, _3252.z || _3253.z, _3252.w || _3253.w)));
    }
    else
    {
        _3258 = true;
    }
    bool _3266;
    if (!_3258)
    {
        _3266 = any(abs(old.a.xyz) > float3(999999995904.0));
    }
    else
    {
        _3266 = true;
    }
    bool _3274;
    if (!_3266)
    {
        _3274 = any(abs(old.b.xyz) > float3(999999995904.0));
    }
    else
    {
        _3274 = true;
    }
    bool _3282;
    if (!_3274)
    {
        _3282 = any(abs(old.c.xyz) > float3(999999995904.0));
    }
    else
    {
        _3282 = true;
    }
    bool _3289;
    if (!_3282)
    {
        _3289 = any(old.projection.xy <= float2(0.0));
    }
    else
    {
        _3289 = true;
    }
    bool _3296;
    if (!_3289)
    {
        _3296 = any(abs(old.projection) > float4(999999995904.0));
    }
    else
    {
        _3296 = true;
    }
    bool _3303;
    if (!_3296)
    {
        _3303 = any(old.extentClip.xy < float2(1.0));
    }
    else
    {
        _3303 = true;
    }
    bool _3310;
    if (!_3303)
    {
        _3310 = any(old.extentClip.xy > float2(16384.0));
    }
    else
    {
        _3310 = true;
    }
    bool _3316;
    if (!_3310)
    {
        _3316 = old.extentClip.z <= 0.0;
    }
    else
    {
        _3316 = true;
    }
    bool _3325;
    if (!_3316)
    {
        _3325 = old.extentClip.w <= old.extentClip.z;
    }
    else
    {
        _3325 = true;
    }
    bool _3331;
    if (!_3325)
    {
        _3331 = old.extentClip.w > 999999995904.0;
    }
    else
    {
        _3331 = true;
    }
    bool _3337;
    if (!_3331)
    {
        _3337 = old.settings.x < 0.0;
    }
    else
    {
        _3337 = true;
    }
    bool _3343;
    if (!_3337)
    {
        _3343 = old.settings.x > 1.0;
    }
    else
    {
        _3343 = true;
    }
    bool _3349;
    if (!_3343)
    {
        _3349 = old.settings.y < 0.0;
    }
    else
    {
        _3349 = true;
    }
    bool _3355;
    if (!_3349)
    {
        _3355 = old.settings.y > 7.0;
    }
    else
    {
        _3355 = true;
    }
    bool _3365;
    if (!_3355)
    {
        _3365 = floor(old.settings.y) != old.settings.y;
    }
    else
    {
        _3365 = true;
    }
    bool _3372;
    if (!_3365)
    {
        _3372 = any(old.settings.zw != float2(0.0));
    }
    else
    {
        _3372 = true;
    }
    if (_3372)
    {
        return false;
    }
    bool _3378;
    if (_3181)
    {
        _3378 = any(old.c.xyz != float3(0.0));
    }
    else
    {
        _3378 = false;
    }
    if (_3378)
    {
        return false;
    }
    float3 _3395;
    if (_3181)
    {
        _3395 = old.b.xyz;
    }
    else
    {
        _3395 = cross(spvFSub(old.b.xyz, old.a.xyz), spvFSub(old.c.xyz, old.a.xyz));
    }
    float _1615 = dot(_3395, _3395);
    bool3 _3396 = isnan(_3395);
    bool3 _3397 = isinf(_3395);
    bool _3405;
    if (all(not(bool3(_3396.x || _3397.x, _3396.y || _3397.y, _3396.z || _3397.z))))
    {
        _3405 = !(isnan(_1615) || isinf(_1615));
    }
    else
    {
        _3405 = false;
    }
    bool _3407;
    if (_3405)
    {
        _3407 = _1615 > 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3407 = false;
    }
    return _3407;
}

static inline __attribute__((always_inline))
bool reflection_liquid_frame_valid(thread const ReflectionLiquidFrame& old)
{
    float _1616 = dot(old.planeNormal.xyz, old.planeNormal.xyz);
    bool4 _3416 = isnan(old.planePoint);
    bool4 _3417 = isinf(old.planePoint);
    bool _3429;
    if (all(not(bool4(_3416.x || _3417.x, _3416.y || _3417.y, _3416.z || _3417.z, _3416.w || _3417.w))))
    {
        bool4 _3423 = isnan(old.planeNormal);
        bool4 _3424 = isinf(old.planeNormal);
        _3429 = !all(not(bool4(_3423.x || _3424.x, _3423.y || _3424.y, _3423.z || _3424.z, _3423.w || _3424.w)));
    }
    else
    {
        _3429 = true;
    }
    bool _3439;
    if (!_3429)
    {
        bool4 _3433 = isnan(old.rotation0);
        bool4 _3434 = isinf(old.rotation0);
        _3439 = !all(not(bool4(_3433.x || _3434.x, _3433.y || _3434.y, _3433.z || _3434.z, _3433.w || _3434.w)));
    }
    else
    {
        _3439 = true;
    }
    bool _3449;
    if (!_3439)
    {
        bool4 _3443 = isnan(old.rotation1);
        bool4 _3444 = isinf(old.rotation1);
        _3449 = !all(not(bool4(_3443.x || _3444.x, _3443.y || _3444.y, _3443.z || _3444.z, _3443.w || _3444.w)));
    }
    else
    {
        _3449 = true;
    }
    bool _3459;
    if (!_3449)
    {
        bool4 _3453 = isnan(old.rotation2);
        bool4 _3454 = isinf(old.rotation2);
        _3459 = !all(not(bool4(_3453.x || _3454.x, _3453.y || _3454.y, _3453.z || _3454.z, _3453.w || _3454.w)));
    }
    else
    {
        _3459 = true;
    }
    bool _3469;
    if (!_3459)
    {
        bool4 _3463 = isnan(old.projection);
        bool4 _3464 = isinf(old.projection);
        _3469 = !all(not(bool4(_3463.x || _3464.x, _3463.y || _3464.y, _3463.z || _3464.z, _3463.w || _3464.w)));
    }
    else
    {
        _3469 = true;
    }
    bool _3479;
    if (!_3469)
    {
        bool4 _3473 = isnan(old.extentClip);
        bool4 _3474 = isinf(old.extentClip);
        _3479 = !all(not(bool4(_3473.x || _3474.x, _3473.y || _3474.y, _3473.z || _3474.z, _3473.w || _3474.w)));
    }
    else
    {
        _3479 = true;
    }
    bool _3489;
    if (!_3479)
    {
        bool4 _3483 = isnan(old.settings);
        bool4 _3484 = isinf(old.settings);
        _3489 = !all(not(bool4(_3483.x || _3484.x, _3483.y || _3484.y, _3483.z || _3484.z, _3483.w || _3484.w)));
    }
    else
    {
        _3489 = true;
    }
    bool _3497;
    if (!_3489)
    {
        _3497 = any(abs(old.planePoint.xyz) > float3(999999995904.0));
    }
    else
    {
        _3497 = true;
    }
    bool _3502;
    if (!_3497)
    {
        _3502 = isnan(_1616) || isinf(_1616);
    }
    else
    {
        _3502 = true;
    }
    bool _3505;
    if (!_3502)
    {
        _3505 = _1616 <= 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3505 = true;
    }
    bool _3512;
    if (!_3505)
    {
        _3512 = any(abs(old.rotation0) > float4(999999995904.0));
    }
    else
    {
        _3512 = true;
    }
    bool _3519;
    if (!_3512)
    {
        _3519 = any(abs(old.rotation1) > float4(999999995904.0));
    }
    else
    {
        _3519 = true;
    }
    bool _3526;
    if (!_3519)
    {
        _3526 = any(abs(old.rotation2) > float4(999999995904.0));
    }
    else
    {
        _3526 = true;
    }
    bool _3533;
    if (!_3526)
    {
        _3533 = any(old.projection.xy <= float2(0.0));
    }
    else
    {
        _3533 = true;
    }
    bool _3540;
    if (!_3533)
    {
        _3540 = any(old.projection.xy > float2(999999995904.0));
    }
    else
    {
        _3540 = true;
    }
    bool _3547;
    if (!_3540)
    {
        _3547 = any(old.extentClip.xy < float2(1.0));
    }
    else
    {
        _3547 = true;
    }
    bool _3554;
    if (!_3547)
    {
        _3554 = any(old.extentClip.xy > float2(16384.0));
    }
    else
    {
        _3554 = true;
    }
    bool _3560;
    if (!_3554)
    {
        _3560 = old.extentClip.z <= 0.0;
    }
    else
    {
        _3560 = true;
    }
    bool _3569;
    if (!_3560)
    {
        _3569 = old.extentClip.w <= old.extentClip.z;
    }
    else
    {
        _3569 = true;
    }
    bool _3575;
    if (!_3569)
    {
        _3575 = old.extentClip.w > 999999995904.0;
    }
    else
    {
        _3575 = true;
    }
    bool _3581;
    if (!_3575)
    {
        _3581 = old.settings.x < 0.0;
    }
    else
    {
        _3581 = true;
    }
    bool _3587;
    if (!_3581)
    {
        _3587 = old.settings.x > 999999995904.0;
    }
    else
    {
        _3587 = true;
    }
    bool _3598;
    if (!_3587)
    {
        bool _3597;
        if (old.settings.y != 0.0)
        {
            _3597 = old.settings.y != 3.0;
        }
        else
        {
            _3597 = false;
        }
        _3598 = _3597;
    }
    else
    {
        _3598 = true;
    }
    if (_3598)
    {
        return false;
    }
    float _1617 = dot(old.rotation0.xyz, cross(old.rotation1.xyz, old.rotation2.xyz));
    bool _3615;
    if (!(isnan(_1617) || isinf(_1617)))
    {
        _3615 = abs(_1617) > 9.9999999600419720025001879548654e-13;
    }
    else
    {
        _3615 = false;
    }
    return _3615;
}

static inline __attribute__((always_inline))
bool interval_exact_point(thread const Interval& a, thread const float& x)
{
    if (x == 0.0)
    {
        return ((as_type<uint>(a.lo) | as_type<uint>(a.hi)) & 2147483647u) == 0u;
    }
    bool _11382;
    if (as_type<uint>(a.lo) == as_type<uint>(x))
    {
        _11382 = as_type<uint>(a.hi) == as_type<uint>(x);
    }
    else
    {
        _11382 = false;
    }
    return _11382;
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
    uint _15908;
    if (x > 0.0)
    {
        _15908 = 1u;
    }
    else
    {
        _15908 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _15908);
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
    uint _15922;
    if (x < 0.0)
    {
        _15922 = 1u;
    }
    else
    {
        _15922 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _15922);
}

static inline __attribute__((always_inline))
float optical_add_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    uint _11385 = as_type<uint>(a) & 2147483647u;
    uint _11388 = as_type<uint>(b) & 2147483647u;
    bool _11391;
    if (_11385 < 2139095040u)
    {
        _11391 = _11388 < 2139095040u;
    }
    else
    {
        _11391 = false;
    }
    if (_11391)
    {
        if (_11385 == 0u)
        {
            return b;
        }
        if (_11388 == 0u)
        {
            return a;
        }
        bool _11398;
        if (_11385 >= 8388608u)
        {
            _11398 = _11388 < 8388608u;
        }
        else
        {
            _11398 = true;
        }
        if (_11398)
        {
            intervalFailed = true;
        }
        bool _11401;
        if (_11385 >= 562036736u)
        {
            _11401 = _11385 <= 1568669696u;
        }
        else
        {
            _11401 = false;
        }
        bool _11403;
        if (_11401)
        {
            _11403 = _11388 >= 562036736u;
        }
        else
        {
            _11403 = false;
        }
        bool _11405;
        if (_11403)
        {
            _11405 = _11388 <= 1568669696u;
        }
        else
        {
            _11405 = false;
        }
        if (_11405)
        {
            float _11409;
            if (_11385 >= _11388)
            {
                _11409 = a;
            }
            else
            {
                _11409 = b;
            }
            float _11413;
            if (_11385 >= _11388)
            {
                _11413 = b;
            }
            else
            {
                _11413 = a;
            }
            float _1698 = spvFAdd(_11409, _11413);
            float _1700 = spvFSub(_11413, spvFSub(_1698, _11409));
            if (upper)
            {
                float _11417;
                if (_1700 > 0.0)
                {
                    float param_var_x = _1698;
                    float _11416 = interval_up(param_var_x, intervalFailed);
                    _11417 = _11416;
                }
                else
                {
                    _11417 = _1698;
                }
                return _11417;
            }
            float _11420;
            if (_1700 < 0.0)
            {
                float param_var_x_1 = _1698;
                float _11419 = interval_down(param_var_x_1, intervalFailed);
                _11420 = _11419;
            }
            else
            {
                _11420 = _1698;
            }
            return _11420;
        }
    }
    float _1701 = spvFAdd(a, b);
    float _11426;
    if (upper)
    {
        float param_var_x_2 = _1701;
        float _11425 = interval_up(param_var_x_2, intervalFailed);
        _11426 = _11425;
    }
    else
    {
        float param_var_x_3 = _1701;
        float _11424 = interval_down(param_var_x_3, intervalFailed);
        _11426 = _11424;
    }
    return _11426;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _4479;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _4479 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _4479 = false;
    }
    bool _4483;
    if (_4479)
    {
        _4483 = a.lo <= a.hi;
    }
    else
    {
        _4483 = false;
    }
    bool _4502;
    if (_4483)
    {
        bool _4497;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _4497 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _4497 = false;
        }
        bool _4501;
        if (_4497)
        {
            _4501 = b.lo <= b.hi;
        }
        else
        {
            _4501 = false;
        }
        _4502 = _4501;
    }
    else
    {
        _4502 = false;
    }
    if (_4502)
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
    float _4513 = optical_add_bound(param_var_a_2, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_3 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _4518 = optical_add_bound(param_var_a_3, param_var_b_1, param_var_upper_1, intervalFailed);
    return Interval{ _4513, _4518 };
}

static inline __attribute__((always_inline))
float optical_product_bounds(thread const float& a, thread const float& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    uint _15925 = as_type<uint>(a);
    uint _15926 = _15925 & 2147483647u;
    uint _15928 = as_type<uint>(b);
    uint _15929 = _15928 & 2147483647u;
    bool _15932;
    if (_15926 < 2139095040u)
    {
        _15932 = _15929 < 2139095040u;
    }
    else
    {
        _15932 = false;
    }
    if (_15932)
    {
        bool _15935;
        if (_15926 != 0u)
        {
            _15935 = _15929 == 0u;
        }
        else
        {
            _15935 = true;
        }
        if (_15935)
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
            float _15952 = as_type<float>(as_type<uint>(b) ^ 2147483648u);
            optical_product_upper = _15952;
            return _15952;
        }
        if (as_type<uint>(b) == 3212836864u)
        {
            float _15959 = as_type<float>(as_type<uint>(a) ^ 2147483648u);
            optical_product_upper = _15959;
            return _15959;
        }
        bool _15962;
        if (_15926 >= 8388608u)
        {
            _15962 = _15929 < 8388608u;
        }
        else
        {
            _15962 = true;
        }
        if (_15962)
        {
            intervalFailed = true;
        }
        bool _15965;
        if (_15926 >= 813694976u)
        {
            _15965 = _15926 <= 1317011456u;
        }
        else
        {
            _15965 = false;
        }
        bool _15967;
        if (_15965)
        {
            _15967 = _15929 >= 813694976u;
        }
        else
        {
            _15967 = false;
        }
        bool _15969;
        if (_15967)
        {
            _15969 = _15929 <= 1317011456u;
        }
        else
        {
            _15969 = false;
        }
        if (_15969)
        {
            float _1707 = spvFMul(a, b);
            uint _15976 = _15925 & 65535u;
            uint _15977 = ((_15925 & 8388607u) | 8388608u) >> 16u;
            uint _15978 = _15928 & 65535u;
            uint _15979 = ((_15928 & 8388607u) | 8388608u) >> 16u;
            uint _1708 = _15976 * _15978;
            uint _1711 = (_15976 * _15979) + (_15977 * _15978);
            uint _1712 = _1708 + (_1711 << 16u);
            uint _15983;
            if (_1712 < _1708)
            {
                _15983 = 1u;
            }
            else
            {
                _15983 = 0u;
            }
            uint _1715 = ((_15977 * _15979) + (_1711 >> 16u)) + _15983;
            uint _15984 = as_type<uint>(_1707);
            uint _1717 = (((_15984 & 2147483647u) >> 23u) - (_15926 >> 23u)) - (_15929 >> 23u);
            uint _1718 = _1717 + 150u;
            bool _15991;
            if (_1718 >= 23u)
            {
                _15991 = _1718 > 24u;
            }
            else
            {
                _15991 = true;
            }
            if (_15991)
            {
                intervalFailed = true;
                float param_var_x = _1707;
                float _15992 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _15992;
                float param_var_x_1 = _1707;
                float _15993 = interval_down(param_var_x_1, intervalFailed);
                return _15993;
            }
            uint _15995 = (_15984 & 8388607u) | 8388608u;
            uint _15997 = _15995 << (_1718 & 31u);
            uint _15999 = _15995 >> ((4294967178u - _1717) & 31u);
            bool _16004;
            if (_1715 <= _15999)
            {
                bool _16003;
                if (_1715 == _15999)
                {
                    _16003 = _1712 > _15997;
                }
                else
                {
                    _16003 = false;
                }
                _16004 = _16003;
            }
            else
            {
                _16004 = true;
            }
            bool _16009;
            if (_1715 >= _15999)
            {
                bool _16008;
                if (_1715 == _15999)
                {
                    _16008 = _1712 < _15997;
                }
                else
                {
                    _16008 = false;
                }
                _16009 = _16008;
            }
            else
            {
                _16009 = true;
            }
            bool _16016 = ((as_type<uint>(a) ^ as_type<uint>(b)) & 2147483648u) != 0u;
            bool _16017;
            if (_16016)
            {
                _16017 = _16009;
            }
            else
            {
                _16017 = _16004;
            }
            float _16019;
            if (_16017)
            {
                float param_var_x_2 = _1707;
                float _16018 = interval_up(param_var_x_2, intervalFailed);
                _16019 = _16018;
            }
            else
            {
                _16019 = _1707;
            }
            optical_product_upper = _16019;
            bool _16020;
            if (_16016)
            {
                _16020 = _16004;
            }
            else
            {
                _16020 = _16009;
            }
            float _16022;
            if (_16020)
            {
                float param_var_x_3 = _1707;
                float _16021 = interval_down(param_var_x_3, intervalFailed);
                _16022 = _16021;
            }
            else
            {
                _16022 = _1707;
            }
            return _16022;
        }
    }
    float _1720 = spvFMul(a, b);
    float param_var_x_4 = _1720;
    float _16025 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _16025;
    float param_var_x_5 = _1720;
    float _16026 = interval_down(param_var_x_5, intervalFailed);
    return _16026;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _11440;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11440 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11440 = false;
    }
    bool _11444;
    if (_11440)
    {
        _11444 = a.lo <= a.hi;
    }
    else
    {
        _11444 = false;
    }
    bool _11463;
    if (_11444)
    {
        bool _11458;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11458 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11458 = false;
        }
        bool _11462;
        if (_11458)
        {
            _11462 = b.lo <= b.hi;
        }
        else
        {
            _11462 = false;
        }
        _11463 = _11462;
    }
    else
    {
        _11463 = false;
    }
    if (_11463)
    {
        Interval param_var_a = a;
        float param_var_x = 0.0;
        bool _11469;
        if (!interval_exact_point(param_var_a, param_var_x))
        {
            Interval param_var_a_1 = b;
            float param_var_x_1 = 0.0;
            _11469 = interval_exact_point(param_var_a_1, param_var_x_1);
        }
        else
        {
            _11469 = true;
        }
        if (_11469)
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
    bool _11513;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11513 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11513 = false;
    }
    bool _11517;
    if (_11513)
    {
        _11517 = a.lo <= a.hi;
    }
    else
    {
        _11517 = false;
    }
    bool _11536;
    if (_11517)
    {
        bool _11531;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11531 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11531 = false;
        }
        bool _11535;
        if (_11531)
        {
            _11535 = b.lo <= b.hi;
        }
        else
        {
            _11535 = false;
        }
        _11536 = _11535;
    }
    else
    {
        _11536 = false;
    }
    bool _11544;
    if (_11536)
    {
        _11544 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    else
    {
        _11544 = false;
    }
    bool _11552;
    if (_11536)
    {
        _11552 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    else
    {
        _11552 = false;
    }
    float param_var_a_6 = a.lo;
    float param_var_b = b.lo;
    float _11557 = optical_product_bounds(param_var_a_6, param_var_b, intervalFailed, optical_product_upper);
    float _11566;
    float _11567;
    if (!_11552)
    {
        float param_var_a_7 = a.lo;
        float param_var_b_1 = b.hi;
        float _11564 = optical_product_bounds(param_var_a_7, param_var_b_1, intervalFailed, optical_product_upper);
        _11566 = optical_product_upper;
        _11567 = _11564;
    }
    else
    {
        _11566 = optical_product_upper;
        _11567 = _11557;
    }
    float _11575;
    float _11576;
    if (!_11544)
    {
        float param_var_a_8 = a.hi;
        float param_var_b_2 = b.lo;
        float _11573 = optical_product_bounds(param_var_a_8, param_var_b_2, intervalFailed, optical_product_upper);
        _11575 = optical_product_upper;
        _11576 = _11573;
    }
    else
    {
        _11575 = optical_product_upper;
        _11576 = _11557;
    }
    float _11586;
    float _11587;
    if (!_11544)
    {
        float _11584;
        float _11585;
        if (_11552)
        {
            _11584 = _11575;
            _11585 = _11576;
        }
        else
        {
            float param_var_a_9 = a.hi;
            float param_var_b_3 = b.hi;
            float _11582 = optical_product_bounds(param_var_a_9, param_var_b_3, intervalFailed, optical_product_upper);
            _11584 = optical_product_upper;
            _11585 = _11582;
        }
        _11586 = _11584;
        _11587 = _11585;
    }
    else
    {
        _11586 = _11566;
        _11587 = _11567;
    }
    return Interval{ precise::min(precise::min(_11557, _11567), precise::min(_11576, _11587)), precise::max(precise::max(optical_product_upper, _11566), precise::max(_11575, _11586)) };
}

static inline __attribute__((always_inline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float _21463;
    _21463 = precise::sqrt(a);
    float _21464;
    for (uint _21465 = 0u; _21465 < 8u; _21463 = _21464, _21465++)
    {
        float _1808 = spvFMul(_21463, _21463);
        float _1809 = spvFMul(_21463, 4097.0);
        float _1811 = spvFSub(_1809, spvFSub(_1809, _21463));
        float _1812 = spvFSub(_21463, _1811);
        float _1813 = spvFMul(_21463, 4097.0);
        float _1815 = spvFSub(_1813, spvFSub(_1813, _21463));
        float _1816 = spvFSub(_21463, _1815);
        float _1830 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1811, _1815), _1808), spvFMul(_1811, _1816)), spvFMul(_1812, _1815)), spvFMul(_1812, _1816)), spvFMul(_21463, 0.0)), spvFMul(0.0, _21463)), spvFMul(0.0, 0.0));
        float _1831 = spvFAdd(_1808, _1830);
        float _1833 = spvFSub(_1830, spvFSub(_1831, _1808));
        bool _21474;
        if (!(isnan(_1831) || isinf(_1831)))
        {
            _21474 = isnan(_1833) || isinf(_1833);
        }
        else
        {
            _21474 = true;
        }
        if (_21474)
        {
            intervalFailed = true;
            return _21463;
        }
        bool _21481;
        if ((isunordered(_1831, a) || _1831 >= a))
        {
            bool _21480;
            if (_1831 == a)
            {
                _21480 = _1833 < 0.0;
            }
            else
            {
                _21480 = false;
            }
            _21481 = _21480;
        }
        else
        {
            _21481 = true;
        }
        bool _21488;
        if ((isunordered(a, _1831) || a >= _1831))
        {
            bool _21487;
            if (a == _1831)
            {
                _21487 = 0.0 < _1833;
            }
            else
            {
                _21487 = false;
            }
            _21488 = _21487;
        }
        else
        {
            _21488 = true;
        }
        bool _21492;
        if (upper)
        {
            _21492 = !_21481;
        }
        else
        {
            _21492 = !_21488;
        }
        if (_21492)
        {
            return _21463;
        }
        if (upper)
        {
            float param_var_x = _21463;
            float _21496 = interval_up(param_var_x, intervalFailed);
            _21464 = _21496;
        }
        else
        {
            float param_var_x_1 = _21463;
            float _21494 = interval_down(param_var_x_1, intervalFailed);
            _21464 = precise::max(0.0, _21494);
        }
    }
    intervalFailed = true;
    return _21463;
}

static inline __attribute__((always_inline))
Interval isqrt(thread const Interval& a, thread bool& intervalFailed)
{
    bool _16033;
    if ((isunordered(a.hi, 0.0) || a.hi >= 0.0))
    {
        _16033 = a.hi > 1000000015047466219876688855040.0;
    }
    else
    {
        _16033 = true;
    }
    if (_16033)
    {
        intervalFailed = true;
        return Interval{ 0.0, 1000000015047466219876688855040.0 };
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _16037 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _16037;
    float _16038 = interval_down(param_var_x, intervalFailed);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _16043 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _16043;
    float _16044 = interval_up(param_var_x_1, intervalFailed);
    return Interval{ precise::max(0.0, _16038), _16044 };
}

static inline __attribute__((always_inline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float _21499;
    _21499 = a / b;
    float _21500;
    for (uint _21501 = 0u; _21501 < 8u; _21499 = _21500, _21501++)
    {
        bool _21509;
        if (!(isnan(_21499) || isinf(_21499)))
        {
            _21509 = abs(_21499) > 1000000015047466219876688855040.0;
        }
        else
        {
            _21509 = true;
        }
        if (_21509)
        {
            intervalFailed = true;
            return _21499;
        }
        float _1858 = spvFMul(_21499, b);
        float _1859 = spvFMul(_21499, 4097.0);
        float _1861 = spvFSub(_1859, spvFSub(_1859, _21499));
        float _1862 = spvFSub(_21499, _1861);
        float _1863 = spvFMul(b, 4097.0);
        float _1865 = spvFSub(_1863, spvFSub(_1863, b));
        float _1866 = spvFSub(b, _1865);
        float _1880 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1861, _1865), _1858), spvFMul(_1861, _1866)), spvFMul(_1862, _1865)), spvFMul(_1862, _1866)), spvFMul(_21499, 0.0)), spvFMul(0.0, b)), spvFMul(0.0, 0.0));
        float _1881 = spvFAdd(_1858, _1880);
        float _1883 = spvFSub(_1880, spvFSub(_1881, _1858));
        bool _21518;
        if (!(isnan(_1881) || isinf(_1881)))
        {
            _21518 = isnan(_1883) || isinf(_1883);
        }
        else
        {
            _21518 = true;
        }
        if (_21518)
        {
            intervalFailed = true;
            return _21499;
        }
        bool _21525;
        if ((isunordered(_1881, a) || _1881 >= a))
        {
            bool _21524;
            if (_1881 == a)
            {
                _21524 = _1883 < 0.0;
            }
            else
            {
                _21524 = false;
            }
            _21525 = _21524;
        }
        else
        {
            _21525 = true;
        }
        bool _21532;
        if ((isunordered(a, _1881) || a >= _1881))
        {
            bool _21531;
            if (a == _1881)
            {
                _21531 = 0.0 < _1883;
            }
            else
            {
                _21531 = false;
            }
            _21532 = _21531;
        }
        else
        {
            _21532 = true;
        }
        if (b < 0.0)
        {
            bool _21542;
            if (upper)
            {
                _21542 = !_21532;
            }
            else
            {
                _21542 = !_21525;
            }
            if (_21542)
            {
                return _21499;
            }
        }
        else
        {
            bool _21538;
            if (upper)
            {
                _21538 = !_21525;
            }
            else
            {
                _21538 = !_21532;
            }
            if (_21538)
            {
                return _21499;
            }
        }
        if (upper)
        {
            float param_var_x = _21499;
            float _21545 = interval_up(param_var_x, intervalFailed);
            _21500 = _21545;
        }
        else
        {
            float param_var_x_1 = _21499;
            float _21544 = interval_down(param_var_x_1, intervalFailed);
            _21500 = _21544;
        }
    }
    intervalFailed = true;
    return _21499;
}

static inline __attribute__((always_inline))
float interval_divide_pair(thread const float& alo, thread const float& ahi, thread const float& blo, thread const float& bhi, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _16058;
    if (!(isnan(alo) || isinf(alo)))
    {
        _16058 = !(isnan(ahi) || isinf(ahi));
    }
    else
    {
        _16058 = false;
    }
    bool _16062;
    if (_16058)
    {
        _16062 = alo <= ahi;
    }
    else
    {
        _16062 = false;
    }
    bool _16080;
    if (_16062)
    {
        bool _16075;
        if (!(isnan(blo) || isinf(blo)))
        {
            _16075 = !(isnan(bhi) || isinf(bhi));
        }
        else
        {
            _16075 = false;
        }
        bool _16079;
        if (_16075)
        {
            _16079 = blo <= bhi;
        }
        else
        {
            _16079 = false;
        }
        _16080 = _16079;
    }
    else
    {
        _16080 = false;
    }
    bool _16086;
    if (_16080)
    {
        _16086 = as_type<uint>(alo) == as_type<uint>(ahi);
    }
    else
    {
        _16086 = false;
    }
    bool _16092;
    if (_16080)
    {
        _16092 = as_type<uint>(blo) == as_type<uint>(bhi);
    }
    else
    {
        _16092 = false;
    }
    float param_var_a = alo;
    float param_var_b = blo;
    bool param_var_upper = false;
    float _16095 = quotient_bound(param_var_a, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_1 = alo;
    float param_var_b_1 = blo;
    bool param_var_upper_1 = true;
    float _16098 = quotient_bound(param_var_a_1, param_var_b_1, param_var_upper_1, intervalFailed);
    float _16106;
    float _16107;
    if (!_16092)
    {
        float param_var_a_2 = alo;
        float param_var_b_2 = bhi;
        bool param_var_upper_2 = false;
        float _16102 = quotient_bound(param_var_a_2, param_var_b_2, param_var_upper_2, intervalFailed);
        float param_var_a_3 = alo;
        float param_var_b_3 = bhi;
        bool param_var_upper_3 = true;
        float _16105 = quotient_bound(param_var_a_3, param_var_b_3, param_var_upper_3, intervalFailed);
        _16106 = _16105;
        _16107 = _16102;
    }
    else
    {
        _16106 = _16098;
        _16107 = _16095;
    }
    float _16115;
    float _16116;
    if (!_16086)
    {
        float param_var_a_4 = ahi;
        float param_var_b_4 = blo;
        bool param_var_upper_4 = false;
        float _16111 = quotient_bound(param_var_a_4, param_var_b_4, param_var_upper_4, intervalFailed);
        float param_var_a_5 = ahi;
        float param_var_b_5 = blo;
        bool param_var_upper_5 = true;
        float _16114 = quotient_bound(param_var_a_5, param_var_b_5, param_var_upper_5, intervalFailed);
        _16115 = _16114;
        _16116 = _16111;
    }
    else
    {
        _16115 = _16098;
        _16116 = _16095;
    }
    float _16126;
    float _16127;
    if (!_16086)
    {
        float _16124;
        float _16125;
        if (_16092)
        {
            _16124 = _16115;
            _16125 = _16116;
        }
        else
        {
            float param_var_a_6 = ahi;
            float param_var_b_6 = bhi;
            bool param_var_upper_6 = false;
            float _16120 = quotient_bound(param_var_a_6, param_var_b_6, param_var_upper_6, intervalFailed);
            float param_var_a_7 = ahi;
            float param_var_b_7 = bhi;
            bool param_var_upper_7 = true;
            float _16123 = quotient_bound(param_var_a_7, param_var_b_7, param_var_upper_7, intervalFailed);
            _16124 = _16123;
            _16125 = _16120;
        }
        _16126 = _16124;
        _16127 = _16125;
    }
    else
    {
        _16126 = _16106;
        _16127 = _16107;
    }
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _16098), _16106), _16115), _16126);
    return precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _16095), _16107), _16116), _16127);
}

static inline __attribute__((always_inline))
Interval idiv(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _11602;
    if (b.lo <= 0.0)
    {
        _11602 = b.hi >= 0.0;
    }
    else
    {
        _11602 = false;
    }
    bool _11612;
    if (!_11602)
    {
        _11612 = precise::max(abs(b.lo), abs(b.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _11612 = true;
    }
    bool _11622;
    if (!_11612)
    {
        _11622 = precise::max(abs(a.lo), abs(a.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _11622 = true;
    }
    if (_11622)
    {
        intervalFailed = true;
        return Interval{ -1000000015047466219876688855040.0, 1000000015047466219876688855040.0 };
    }
    bool _11636;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11636 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11636 = false;
    }
    bool _11640;
    if (_11636)
    {
        _11640 = a.lo <= a.hi;
    }
    else
    {
        _11640 = false;
    }
    bool _11659;
    if (_11640)
    {
        bool _11654;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11654 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11654 = false;
        }
        bool _11658;
        if (_11654)
        {
            _11658 = b.lo <= b.hi;
        }
        else
        {
            _11658 = false;
        }
        _11659 = _11658;
    }
    else
    {
        _11659 = false;
    }
    if (_11659)
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
    float _11685 = interval_divide_pair(param_var_alo, param_var_ahi, param_var_blo, param_var_bhi, intervalFailed, interval_divide_upper);
    float param_var_x_3 = _11685;
    float _11686 = interval_down(param_var_x_3, intervalFailed);
    float param_var_x_4 = interval_divide_upper;
    float _11688 = interval_up(param_var_x_4, intervalFailed);
    return Interval{ _11686, _11688 };
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, device type_ByteAddressBuffer& frames, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        Interval param_var_a = Interval{ feature.x, feature.x };
        Interval param_var_b = Interval{ feature.y, feature.y };
        Interval _3736 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval _3725 = Interval{ 1.0, 1.0 };
        Interval _3726 = Interval{ as_type<float>(as_type<uint>(_3736.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_3736.lo) ^ 2147483648u) };
        Interval _3746 = iadd(_3725, _3726, intervalFailed);
        uint _3748 = (at + 432u) >> 2u;
        uint _3750 = frames._m0[_3748];
        uint _3752 = frames._m0[_3748 + 1u];
        uint _3754 = frames._m0[_3748 + 2u];
        uint _3756 = frames._m0[_3748 + 3u];
        float4 _3758 = as_type<float4>(uint4(_3750, _3752, _3754, _3756));
        float _3759 = _3758.x;
        float _3760 = _3758.y;
        float _3761 = _3758.z;
        Interval _3719 = Interval{ _3759, _3759 };
        Interval _3720 = _3746;
        Interval _3763 = imul(_3719, _3720, intervalFailed, optical_product_upper);
        Interval _3721 = Interval{ _3760, _3760 };
        Interval _3722 = _3746;
        Interval _3765 = imul(_3721, _3722, intervalFailed, optical_product_upper);
        Interval _3723 = Interval{ _3761, _3761 };
        Interval _3724 = _3746;
        Interval _3767 = imul(_3723, _3724, intervalFailed, optical_product_upper);
        uint _3769 = (at + 448u) >> 2u;
        uint _3771 = frames._m0[_3769];
        uint _3773 = frames._m0[_3769 + 1u];
        uint _3775 = frames._m0[_3769 + 2u];
        uint _3777 = frames._m0[_3769 + 3u];
        float4 _3779 = as_type<float4>(uint4(_3771, _3773, _3775, _3777));
        float _3780 = _3779.x;
        float _3781 = _3779.y;
        float _3782 = _3779.z;
        Interval _3713 = Interval{ _3780, _3780 };
        Interval _3714 = Interval{ feature.x, feature.x };
        Interval _3785 = imul(_3713, _3714, intervalFailed, optical_product_upper);
        Interval _3715 = Interval{ _3781, _3781 };
        Interval _3716 = Interval{ feature.x, feature.x };
        Interval _3788 = imul(_3715, _3716, intervalFailed, optical_product_upper);
        Interval _3717 = Interval{ _3782, _3782 };
        Interval _3718 = Interval{ feature.x, feature.x };
        Interval _3791 = imul(_3717, _3718, intervalFailed, optical_product_upper);
        Interval _3707 = _3763;
        Interval _3708 = _3785;
        Interval _3792 = iadd(_3707, _3708, intervalFailed);
        Interval _3709 = _3765;
        Interval _3710 = _3788;
        Interval _3793 = iadd(_3709, _3710, intervalFailed);
        Interval _3711 = _3767;
        Interval _3712 = _3791;
        Interval _3794 = iadd(_3711, _3712, intervalFailed);
        uint _3796 = (at + 464u) >> 2u;
        uint _3798 = frames._m0[_3796];
        uint _3800 = frames._m0[_3796 + 1u];
        uint _3802 = frames._m0[_3796 + 2u];
        uint _3804 = frames._m0[_3796 + 3u];
        float4 _3806 = as_type<float4>(uint4(_3798, _3800, _3802, _3804));
        float _3807 = _3806.x;
        float _3808 = _3806.y;
        float _3809 = _3806.z;
        Interval _3701 = Interval{ _3807, _3807 };
        Interval _3702 = Interval{ feature.y, feature.y };
        Interval _3812 = imul(_3701, _3702, intervalFailed, optical_product_upper);
        Interval _3703 = Interval{ _3808, _3808 };
        Interval _3704 = Interval{ feature.y, feature.y };
        Interval _3815 = imul(_3703, _3704, intervalFailed, optical_product_upper);
        Interval _3705 = Interval{ _3809, _3809 };
        Interval _3706 = Interval{ feature.y, feature.y };
        Interval _3818 = imul(_3705, _3706, intervalFailed, optical_product_upper);
        Interval _3695 = _3792;
        Interval _3696 = _3812;
        Interval _3819 = iadd(_3695, _3696, intervalFailed);
        Interval _3697 = _3793;
        Interval _3698 = _3815;
        Interval _3820 = iadd(_3697, _3698, intervalFailed);
        Interval _3699 = _3794;
        Interval _3700 = _3818;
        Interval _3821 = iadd(_3699, _3700, intervalFailed);
        return Interval3{ _3819, _3820, _3821 };
    }
    Interval _3685 = Interval{ TargetSettings.targetCurrentCube[0].x, TargetSettings.targetCurrentCube[0].x };
    Interval _3686 = Interval{ feature.x, feature.x };
    Interval _3835 = imul(_3685, _3686, intervalFailed, optical_product_upper);
    Interval _3687 = _3835;
    Interval _3688 = Interval{ TargetSettings.targetCurrentCube[0].y, TargetSettings.targetCurrentCube[0].y };
    Interval _3689 = Interval{ feature.y, feature.y };
    Interval _3838 = imul(_3688, _3689, intervalFailed, optical_product_upper);
    Interval _3690 = _3838;
    Interval _3839 = iadd(_3687, _3690, intervalFailed);
    Interval _3691 = _3839;
    Interval _3692 = Interval{ TargetSettings.targetCurrentCube[0].z, TargetSettings.targetCurrentCube[0].z };
    Interval _3693 = Interval{ feature.z, feature.z };
    Interval _3842 = imul(_3692, _3693, intervalFailed, optical_product_upper);
    Interval _3694 = _3842;
    Interval _3843 = iadd(_3691, _3694, intervalFailed);
    Interval _3675 = Interval{ TargetSettings.targetCurrentCube[1].x, TargetSettings.targetCurrentCube[1].x };
    Interval _3676 = Interval{ feature.x, feature.x };
    Interval _3852 = imul(_3675, _3676, intervalFailed, optical_product_upper);
    Interval _3677 = _3852;
    Interval _3678 = Interval{ TargetSettings.targetCurrentCube[1].y, TargetSettings.targetCurrentCube[1].y };
    Interval _3679 = Interval{ feature.y, feature.y };
    Interval _3855 = imul(_3678, _3679, intervalFailed, optical_product_upper);
    Interval _3680 = _3855;
    Interval _3856 = iadd(_3677, _3680, intervalFailed);
    Interval _3681 = _3856;
    Interval _3682 = Interval{ TargetSettings.targetCurrentCube[1].z, TargetSettings.targetCurrentCube[1].z };
    Interval _3683 = Interval{ feature.z, feature.z };
    Interval _3859 = imul(_3682, _3683, intervalFailed, optical_product_upper);
    Interval _3684 = _3859;
    Interval _3860 = iadd(_3681, _3684, intervalFailed);
    Interval _3665 = Interval{ TargetSettings.targetCurrentCube[2].x, TargetSettings.targetCurrentCube[2].x };
    Interval _3666 = Interval{ feature.x, feature.x };
    Interval _3869 = imul(_3665, _3666, intervalFailed, optical_product_upper);
    Interval _3667 = _3869;
    Interval _3668 = Interval{ TargetSettings.targetCurrentCube[2].y, TargetSettings.targetCurrentCube[2].y };
    Interval _3669 = Interval{ feature.y, feature.y };
    Interval _3872 = imul(_3668, _3669, intervalFailed, optical_product_upper);
    Interval _3670 = _3872;
    Interval _3873 = iadd(_3667, _3670, intervalFailed);
    Interval _3671 = _3873;
    Interval _3672 = Interval{ TargetSettings.targetCurrentCube[2].z, TargetSettings.targetCurrentCube[2].z };
    Interval _3673 = Interval{ feature.z, feature.z };
    Interval _3876 = imul(_3672, _3673, intervalFailed, optical_product_upper);
    Interval _3674 = _3876;
    Interval _3877 = iadd(_3671, _3674, intervalFailed);
    Interval _3655 = Interval{ TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[0].x };
    Interval _3656 = _3843;
    Interval _3891 = imul(_3655, _3656, intervalFailed, optical_product_upper);
    Interval _3657 = _3891;
    Interval _3658 = Interval{ TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[1].x };
    Interval _3659 = _3860;
    Interval _3893 = imul(_3658, _3659, intervalFailed, optical_product_upper);
    Interval _3660 = _3893;
    Interval _3894 = iadd(_3657, _3660, intervalFailed);
    Interval _3661 = _3894;
    Interval _3662 = Interval{ TargetSettings.targetPreviousCube[2].x, TargetSettings.targetPreviousCube[2].x };
    Interval _3663 = _3877;
    Interval _3896 = imul(_3662, _3663, intervalFailed, optical_product_upper);
    Interval _3664 = _3896;
    Interval _3897 = iadd(_3661, _3664, intervalFailed);
    Interval _3645 = Interval{ TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[0].y };
    Interval _3646 = _3843;
    Interval _3911 = imul(_3645, _3646, intervalFailed, optical_product_upper);
    Interval _3647 = _3911;
    Interval _3648 = Interval{ TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[1].y };
    Interval _3649 = _3860;
    Interval _3913 = imul(_3648, _3649, intervalFailed, optical_product_upper);
    Interval _3650 = _3913;
    Interval _3914 = iadd(_3647, _3650, intervalFailed);
    Interval _3651 = _3914;
    Interval _3652 = Interval{ TargetSettings.targetPreviousCube[2].y, TargetSettings.targetPreviousCube[2].y };
    Interval _3653 = _3877;
    Interval _3916 = imul(_3652, _3653, intervalFailed, optical_product_upper);
    Interval _3654 = _3916;
    Interval _3917 = iadd(_3651, _3654, intervalFailed);
    Interval _3635 = Interval{ TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[0].z };
    Interval _3636 = _3843;
    Interval _3931 = imul(_3635, _3636, intervalFailed, optical_product_upper);
    Interval _3637 = _3931;
    Interval _3638 = Interval{ TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[1].z };
    Interval _3639 = _3860;
    Interval _3933 = imul(_3638, _3639, intervalFailed, optical_product_upper);
    Interval _3640 = _3933;
    Interval _3934 = iadd(_3637, _3640, intervalFailed);
    Interval _3641 = _3934;
    Interval _3642 = Interval{ TargetSettings.targetPreviousCube[2].z, TargetSettings.targetPreviousCube[2].z };
    Interval _3643 = _3877;
    Interval _3936 = imul(_3642, _3643, intervalFailed, optical_product_upper);
    Interval _3644 = _3936;
    Interval _3937 = iadd(_3641, _3644, intervalFailed);
    Interval _3633 = Interval{ 1.0, 1.0 };
    bool _3944;
    if (_3897.lo <= 0.0)
    {
        _3944 = _3897.hi >= 0.0;
    }
    else
    {
        _3944 = false;
    }
    float _3951;
    if (_3944)
    {
        _3951 = 0.0;
    }
    else
    {
        _3951 = precise::min(abs(_3897.lo), abs(_3897.hi));
    }
    float _3954 = precise::max(abs(_3897.lo), abs(_3897.hi));
    float _3626 = spvFMul(_3951, _3951);
    float _3955 = interval_down(_3626, intervalFailed);
    float _3627 = spvFMul(_3954, _3954);
    float _3957 = interval_up(_3627, intervalFailed);
    Interval _3628 = Interval{ precise::max(0.0, _3955), _3957 };
    bool _3965;
    if (_3917.lo <= 0.0)
    {
        _3965 = _3917.hi >= 0.0;
    }
    else
    {
        _3965 = false;
    }
    float _3972;
    if (_3965)
    {
        _3972 = 0.0;
    }
    else
    {
        _3972 = precise::min(abs(_3917.lo), abs(_3917.hi));
    }
    float _3975 = precise::max(abs(_3917.lo), abs(_3917.hi));
    float _3624 = spvFMul(_3972, _3972);
    float _3976 = interval_down(_3624, intervalFailed);
    float _3625 = spvFMul(_3975, _3975);
    float _3978 = interval_up(_3625, intervalFailed);
    Interval _3629 = Interval{ precise::max(0.0, _3976), _3978 };
    Interval _3980 = iadd(_3628, _3629, intervalFailed);
    Interval _3630 = _3980;
    bool _3987;
    if (_3937.lo <= 0.0)
    {
        _3987 = _3937.hi >= 0.0;
    }
    else
    {
        _3987 = false;
    }
    float _3994;
    if (_3987)
    {
        _3994 = 0.0;
    }
    else
    {
        _3994 = precise::min(abs(_3937.lo), abs(_3937.hi));
    }
    float _3997 = precise::max(abs(_3937.lo), abs(_3937.hi));
    float _3622 = spvFMul(_3994, _3994);
    float _3998 = interval_down(_3622, intervalFailed);
    float _3623 = spvFMul(_3997, _3997);
    float _4000 = interval_up(_3623, intervalFailed);
    Interval _3631 = Interval{ precise::max(0.0, _3998), _4000 };
    Interval _4002 = iadd(_3630, _3631, intervalFailed);
    Interval _3632 = _4002;
    Interval _4003 = isqrt(_3632, intervalFailed);
    Interval _3634 = _4003;
    Interval _4004 = idiv(_3633, _3634, intervalFailed, interval_divide_upper);
    Interval _3616 = _3897;
    Interval _3617 = _4004;
    Interval _4005 = imul(_3616, _3617, intervalFailed, optical_product_upper);
    Interval _3618 = _3917;
    Interval _3619 = _4004;
    Interval _4006 = imul(_3618, _3619, intervalFailed, optical_product_upper);
    Interval _3620 = _3937;
    Interval _3621 = _4004;
    Interval _4007 = imul(_3620, _3621, intervalFailed, optical_product_upper);
    return Interval3{ _4005, _4006, _4007 };
}

static inline __attribute__((always_inline))
Interval jet_add_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _16164;
    if (a.lo == 0.0)
    {
        _16164 = a.hi == 0.0;
    }
    else
    {
        _16164 = false;
    }
    if (_16164)
    {
        return b;
    }
    bool _16173;
    if (b.lo == 0.0)
    {
        _16173 = b.hi == 0.0;
    }
    else
    {
        _16173 = false;
    }
    if (_16173)
    {
        return a;
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16177 = iadd(param_var_a, param_var_b, intervalFailed);
    return _16177;
}

static inline __attribute__((always_inline))
Interval jet_mul_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _16143;
    if (a.lo == 0.0)
    {
        _16143 = a.hi == 0.0;
    }
    else
    {
        _16143 = false;
    }
    bool _16153;
    if (!_16143)
    {
        bool _16152;
        if (b.lo == 0.0)
        {
            _16152 = b.hi == 0.0;
        }
        else
        {
            _16152 = false;
        }
        _16153 = _16152;
    }
    else
    {
        _16153 = true;
    }
    if (_16153)
    {
        return Interval{ 0.0, 0.0 };
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16156 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    return _16156;
}

static inline __attribute__((always_inline))
Interval jet_div_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    bool _16185;
    if (a.lo == 0.0)
    {
        _16185 = a.hi == 0.0;
    }
    else
    {
        _16185 = false;
    }
    bool _16195;
    if (_16185)
    {
        bool _16193;
        if (b.lo <= 0.0)
        {
            _16193 = b.hi >= 0.0;
        }
        else
        {
            _16193 = false;
        }
        _16195 = !_16193;
    }
    else
    {
        _16195 = false;
    }
    if (_16195)
    {
        return Interval{ 0.0, 0.0 };
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16199 = idiv(param_var_a, param_var_b, intervalFailed, interval_divide_upper);
    bool _16210;
    if (!intervalFailed)
    {
        _16210 = intervalFailed;
    }
    else
    {
        _16210 = false;
    }
    bool _16215;
    if (_16210)
    {
        _16215 = jetFailureSite == 0u;
    }
    else
    {
        _16215 = false;
    }
    if (_16215)
    {
        jetFailureSite = 2u;
        jetFailureArguments = float4(a.lo, a.hi, b.lo, b.hi);
    }
    return _16199;
}

static inline __attribute__((always_inline))
Interval3 plane_normal(thread const ReflectionSpecularPlane& p, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    bool _16297;
    if (p.a.w == 2.0)
    {
        _16297 = p.b.w == 2.0;
    }
    else
    {
        _16297 = false;
    }
    bool _16302;
    if (_16297)
    {
        _16302 = p.c.w == 2.0;
    }
    else
    {
        _16302 = false;
    }
    if (_16302)
    {
        Interval _16285 = Interval{ 1.0, 1.0 };
        bool _16312;
        if (p.b.x <= 0.0)
        {
            _16312 = p.b.x >= 0.0;
        }
        else
        {
            _16312 = false;
        }
        float _16319;
        if (_16312)
        {
            _16319 = 0.0;
        }
        else
        {
            _16319 = precise::min(abs(p.b.x), abs(p.b.x));
        }
        float _16322 = precise::max(abs(p.b.x), abs(p.b.x));
        float _16278 = spvFMul(_16319, _16319);
        float _16323 = interval_down(_16278, intervalFailed);
        float _16279 = spvFMul(_16322, _16322);
        float _16325 = interval_up(_16279, intervalFailed);
        Interval _16280 = Interval{ precise::max(0.0, _16323), _16325 };
        bool _16331;
        if (p.b.y <= 0.0)
        {
            _16331 = p.b.y >= 0.0;
        }
        else
        {
            _16331 = false;
        }
        float _16338;
        if (_16331)
        {
            _16338 = 0.0;
        }
        else
        {
            _16338 = precise::min(abs(p.b.y), abs(p.b.y));
        }
        float _16341 = precise::max(abs(p.b.y), abs(p.b.y));
        float _16276 = spvFMul(_16338, _16338);
        float _16342 = interval_down(_16276, intervalFailed);
        float _16277 = spvFMul(_16341, _16341);
        float _16344 = interval_up(_16277, intervalFailed);
        Interval _16281 = Interval{ precise::max(0.0, _16342), _16344 };
        Interval _16346 = iadd(_16280, _16281, intervalFailed);
        Interval _16282 = _16346;
        bool _16351;
        if (p.b.z <= 0.0)
        {
            _16351 = p.b.z >= 0.0;
        }
        else
        {
            _16351 = false;
        }
        float _16358;
        if (_16351)
        {
            _16358 = 0.0;
        }
        else
        {
            _16358 = precise::min(abs(p.b.z), abs(p.b.z));
        }
        float _16361 = precise::max(abs(p.b.z), abs(p.b.z));
        float _16274 = spvFMul(_16358, _16358);
        float _16362 = interval_down(_16274, intervalFailed);
        float _16275 = spvFMul(_16361, _16361);
        float _16364 = interval_up(_16275, intervalFailed);
        Interval _16283 = Interval{ precise::max(0.0, _16362), _16364 };
        Interval _16366 = iadd(_16282, _16283, intervalFailed);
        Interval _16284 = _16366;
        Interval _16367 = isqrt(_16284, intervalFailed);
        Interval _16286 = _16367;
        Interval _16368 = idiv(_16285, _16286, intervalFailed, interval_divide_upper);
        Interval _16268 = Interval{ p.b.x, p.b.x };
        Interval _16269 = _16368;
        Interval _16370 = imul(_16268, _16269, intervalFailed, optical_product_upper);
        Interval _16270 = Interval{ p.b.y, p.b.y };
        Interval _16271 = _16368;
        Interval _16372 = imul(_16270, _16271, intervalFailed, optical_product_upper);
        Interval _16272 = Interval{ p.b.z, p.b.z };
        Interval _16273 = _16368;
        Interval _16374 = imul(_16272, _16273, intervalFailed, optical_product_upper);
        return Interval3{ _16370, _16372, _16374 };
    }
    Interval _16262 = Interval{ p.b.x, p.b.x };
    Interval _16263 = Interval{ as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u) };
    Interval _16406 = iadd(_16262, _16263, intervalFailed);
    Interval _16264 = Interval{ p.b.y, p.b.y };
    Interval _16265 = Interval{ as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u) };
    Interval _16409 = iadd(_16264, _16265, intervalFailed);
    Interval _16266 = Interval{ p.b.z, p.b.z };
    Interval _16267 = Interval{ as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u) };
    Interval _16412 = iadd(_16266, _16267, intervalFailed);
    Interval _16256 = Interval{ p.c.x, p.c.x };
    Interval _16257 = Interval{ as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u) };
    Interval _16443 = iadd(_16256, _16257, intervalFailed);
    Interval _16258 = Interval{ p.c.y, p.c.y };
    Interval _16259 = Interval{ as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u) };
    Interval _16446 = iadd(_16258, _16259, intervalFailed);
    Interval _16260 = Interval{ p.c.z, p.c.z };
    Interval _16261 = Interval{ as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u) };
    Interval _16449 = iadd(_16260, _16261, intervalFailed);
    Interval _16244 = _16409;
    Interval _16245 = _16449;
    Interval _16450 = imul(_16244, _16245, intervalFailed, optical_product_upper);
    Interval _16246 = _16412;
    Interval _16247 = _16446;
    Interval _16451 = imul(_16246, _16247, intervalFailed, optical_product_upper);
    Interval _16242 = _16450;
    Interval _16243 = Interval{ as_type<float>(as_type<uint>(_16451.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16451.lo) ^ 2147483648u) };
    Interval _16461 = iadd(_16242, _16243, intervalFailed);
    Interval _16248 = _16412;
    Interval _16249 = _16443;
    Interval _16462 = imul(_16248, _16249, intervalFailed, optical_product_upper);
    Interval _16250 = _16406;
    Interval _16251 = _16449;
    Interval _16463 = imul(_16250, _16251, intervalFailed, optical_product_upper);
    Interval _16240 = _16462;
    Interval _16241 = Interval{ as_type<float>(as_type<uint>(_16463.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16463.lo) ^ 2147483648u) };
    Interval _16473 = iadd(_16240, _16241, intervalFailed);
    Interval _16252 = _16406;
    Interval _16253 = _16446;
    Interval _16474 = imul(_16252, _16253, intervalFailed, optical_product_upper);
    Interval _16254 = _16409;
    Interval _16255 = _16443;
    Interval _16475 = imul(_16254, _16255, intervalFailed, optical_product_upper);
    Interval _16238 = _16474;
    Interval _16239 = Interval{ as_type<float>(as_type<uint>(_16475.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16475.lo) ^ 2147483648u) };
    Interval _16485 = iadd(_16238, _16239, intervalFailed);
    Interval _16236 = Interval{ 1.0, 1.0 };
    bool _16492;
    if (_16461.lo <= 0.0)
    {
        _16492 = _16461.hi >= 0.0;
    }
    else
    {
        _16492 = false;
    }
    float _16499;
    if (_16492)
    {
        _16499 = 0.0;
    }
    else
    {
        _16499 = precise::min(abs(_16461.lo), abs(_16461.hi));
    }
    float _16502 = precise::max(abs(_16461.lo), abs(_16461.hi));
    float _16229 = spvFMul(_16499, _16499);
    float _16503 = interval_down(_16229, intervalFailed);
    float _16230 = spvFMul(_16502, _16502);
    float _16505 = interval_up(_16230, intervalFailed);
    Interval _16231 = Interval{ precise::max(0.0, _16503), _16505 };
    bool _16513;
    if (_16473.lo <= 0.0)
    {
        _16513 = _16473.hi >= 0.0;
    }
    else
    {
        _16513 = false;
    }
    float _16520;
    if (_16513)
    {
        _16520 = 0.0;
    }
    else
    {
        _16520 = precise::min(abs(_16473.lo), abs(_16473.hi));
    }
    float _16523 = precise::max(abs(_16473.lo), abs(_16473.hi));
    float _16227 = spvFMul(_16520, _16520);
    float _16524 = interval_down(_16227, intervalFailed);
    float _16228 = spvFMul(_16523, _16523);
    float _16526 = interval_up(_16228, intervalFailed);
    Interval _16232 = Interval{ precise::max(0.0, _16524), _16526 };
    Interval _16528 = iadd(_16231, _16232, intervalFailed);
    Interval _16233 = _16528;
    bool _16535;
    if (_16485.lo <= 0.0)
    {
        _16535 = _16485.hi >= 0.0;
    }
    else
    {
        _16535 = false;
    }
    float _16542;
    if (_16535)
    {
        _16542 = 0.0;
    }
    else
    {
        _16542 = precise::min(abs(_16485.lo), abs(_16485.hi));
    }
    float _16545 = precise::max(abs(_16485.lo), abs(_16485.hi));
    float _16225 = spvFMul(_16542, _16542);
    float _16546 = interval_down(_16225, intervalFailed);
    float _16226 = spvFMul(_16545, _16545);
    float _16548 = interval_up(_16226, intervalFailed);
    Interval _16234 = Interval{ precise::max(0.0, _16546), _16548 };
    Interval _16550 = iadd(_16233, _16234, intervalFailed);
    Interval _16235 = _16550;
    Interval _16551 = isqrt(_16235, intervalFailed);
    Interval _16237 = _16551;
    Interval _16552 = idiv(_16236, _16237, intervalFailed, interval_divide_upper);
    Interval _16219 = _16461;
    Interval _16220 = _16552;
    Interval _16553 = imul(_16219, _16220, intervalFailed, optical_product_upper);
    Interval _16221 = _16473;
    Interval _16222 = _16552;
    Interval _16554 = imul(_16221, _16222, intervalFailed, optical_product_upper);
    Interval _16223 = _16485;
    Interval _16224 = _16552;
    Interval _16555 = imul(_16223, _16224, intervalFailed, optical_product_upper);
    return Interval3{ _16553, _16554, _16555 };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_plane_normal(thread const ReflectionSpecularPlane& plane, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = plane;
    Interval3 _11691 = plane_normal(param_var_p, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _11714;
    if (!intervalFailed)
    {
        bool _11707;
        if (plane.a.w == 2.0)
        {
            _11707 = plane.b.w == 2.0;
        }
        else
        {
            _11707 = false;
        }
        bool _11712;
        if (_11707)
        {
            _11712 = plane.c.w == 2.0;
        }
        else
        {
            _11712 = false;
        }
        _11714 = !_11712;
    }
    else
    {
        _11714 = false;
    }
    if (_11714)
    {
        for (uint _11715 = 0u; _11715 < 3u; _11715++)
        {
            bool _11727;
            if ((isunordered(plane.a[_11715], plane.b[_11715]) || plane.a[_11715] == plane.b[_11715]))
            {
                _11727 = plane.a[_11715] != plane.c[_11715];
            }
            else
            {
                _11727 = true;
            }
            if (_11727)
            {
                continue;
            }
            float _11738;
            float _11739;
            if (_11715 == 0u)
            {
                _11738 = _11691.x.hi;
                _11739 = _11691.x.lo;
            }
            else
            {
                float _11734;
                float _11735;
                if (_11715 == 1u)
                {
                    _11734 = _11691.y.hi;
                    _11735 = _11691.y.lo;
                }
                else
                {
                    _11734 = _11691.z.hi;
                    _11735 = _11691.z.lo;
                }
                _11738 = _11734;
                _11739 = _11735;
            }
            bool _11742;
            if (_11739 <= 0.0)
            {
                _11742 = _11738 >= 0.0;
            }
            else
            {
                _11742 = false;
            }
            if (_11742)
            {
                continue;
            }
            float3 exact = float3(0.0);
            int _11744;
            if (_11739 > 0.0)
            {
                _11744 = 1;
            }
            else
            {
                _11744 = -1;
            }
            exact[_11715] = float(_11744);
            return OpticalJet3{ OpticalJet{ Interval{ exact.x, exact.x }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ exact.y, exact.y }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ exact.z, exact.z }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
        }
    }
    return OpticalJet3{ OpticalJet{ _11691.x, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ _11691.y, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ _11691.z, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_oriented(thread const OpticalJet3& n, thread const OpticalJet3& direction, thread bool& intervalFailed, thread float& optical_product_upper, thread bool& jetBranchKnown)
{
    Interval _11847 = n.x.v;
    Interval _11848 = direction.x.v;
    Interval _11875 = imul(_11847, _11848, intervalFailed, optical_product_upper);
    Interval _11849 = n.x.dx;
    Interval _11850 = direction.x.v;
    Interval _11876 = jet_mul_derivative(_11849, _11850, intervalFailed, optical_product_upper);
    Interval _11851 = _11876;
    Interval _11852 = n.x.v;
    Interval _11853 = direction.x.dx;
    Interval _11877 = jet_mul_derivative(_11852, _11853, intervalFailed, optical_product_upper);
    Interval _11854 = _11877;
    Interval _11878 = jet_add_derivative(_11851, _11854, intervalFailed);
    Interval _11855 = n.x.dy;
    Interval _11856 = direction.x.v;
    Interval _11879 = jet_mul_derivative(_11855, _11856, intervalFailed, optical_product_upper);
    Interval _11857 = _11879;
    Interval _11858 = n.x.v;
    Interval _11859 = direction.x.dy;
    Interval _11880 = jet_mul_derivative(_11858, _11859, intervalFailed, optical_product_upper);
    Interval _11860 = _11880;
    Interval _11881 = jet_add_derivative(_11857, _11860, intervalFailed);
    Interval _11833 = n.y.v;
    Interval _11834 = direction.y.v;
    Interval _11888 = imul(_11833, _11834, intervalFailed, optical_product_upper);
    Interval _11835 = n.y.dx;
    Interval _11836 = direction.y.v;
    Interval _11889 = jet_mul_derivative(_11835, _11836, intervalFailed, optical_product_upper);
    Interval _11837 = _11889;
    Interval _11838 = n.y.v;
    Interval _11839 = direction.y.dx;
    Interval _11890 = jet_mul_derivative(_11838, _11839, intervalFailed, optical_product_upper);
    Interval _11840 = _11890;
    Interval _11891 = jet_add_derivative(_11837, _11840, intervalFailed);
    Interval _11841 = n.y.dy;
    Interval _11842 = direction.y.v;
    Interval _11892 = jet_mul_derivative(_11841, _11842, intervalFailed, optical_product_upper);
    Interval _11843 = _11892;
    Interval _11844 = n.y.v;
    Interval _11845 = direction.y.dy;
    Interval _11893 = jet_mul_derivative(_11844, _11845, intervalFailed, optical_product_upper);
    Interval _11846 = _11893;
    Interval _11894 = jet_add_derivative(_11843, _11846, intervalFailed);
    Interval _11827 = _11875;
    Interval _11828 = _11888;
    Interval _11895 = iadd(_11827, _11828, intervalFailed);
    Interval _11829 = _11878;
    Interval _11830 = _11891;
    Interval _11896 = jet_add_derivative(_11829, _11830, intervalFailed);
    Interval _11831 = _11881;
    Interval _11832 = _11894;
    Interval _11897 = jet_add_derivative(_11831, _11832, intervalFailed);
    Interval _11813 = n.z.v;
    Interval _11814 = direction.z.v;
    Interval _11904 = imul(_11813, _11814, intervalFailed, optical_product_upper);
    Interval _11815 = n.z.dx;
    Interval _11816 = direction.z.v;
    Interval _11905 = jet_mul_derivative(_11815, _11816, intervalFailed, optical_product_upper);
    Interval _11817 = _11905;
    Interval _11818 = n.z.v;
    Interval _11819 = direction.z.dx;
    Interval _11906 = jet_mul_derivative(_11818, _11819, intervalFailed, optical_product_upper);
    Interval _11820 = _11906;
    Interval _11907 = jet_add_derivative(_11817, _11820, intervalFailed);
    Interval _11821 = n.z.dy;
    Interval _11822 = direction.z.v;
    Interval _11908 = jet_mul_derivative(_11821, _11822, intervalFailed, optical_product_upper);
    Interval _11823 = _11908;
    Interval _11824 = n.z.v;
    Interval _11825 = direction.z.dy;
    Interval _11909 = jet_mul_derivative(_11824, _11825, intervalFailed, optical_product_upper);
    Interval _11826 = _11909;
    Interval _11910 = jet_add_derivative(_11823, _11826, intervalFailed);
    Interval _11807 = _11895;
    Interval _11808 = _11904;
    Interval _11911 = iadd(_11807, _11808, intervalFailed);
    Interval _11809 = _11896;
    Interval _11810 = _11907;
    Interval _11912 = jet_add_derivative(_11809, _11810, intervalFailed);
    Interval _11811 = _11897;
    Interval _11812 = _11910;
    Interval _11913 = jet_add_derivative(_11811, _11812, intervalFailed);
    if (_11911.lo > 0.0)
    {
        Interval _11793 = n.x.v;
        Interval _11794 = Interval{ -1.0, -1.0 };
        Interval _11924 = imul(_11793, _11794, intervalFailed, optical_product_upper);
        Interval _11795 = n.x.dx;
        Interval _11796 = Interval{ -1.0, -1.0 };
        Interval _11925 = jet_mul_derivative(_11795, _11796, intervalFailed, optical_product_upper);
        Interval _11797 = _11925;
        Interval _11798 = n.x.v;
        Interval _11799 = Interval{ 0.0, 0.0 };
        Interval _11926 = jet_mul_derivative(_11798, _11799, intervalFailed, optical_product_upper);
        Interval _11800 = _11926;
        Interval _11927 = jet_add_derivative(_11797, _11800, intervalFailed);
        Interval _11801 = n.x.dy;
        Interval _11802 = Interval{ -1.0, -1.0 };
        Interval _11928 = jet_mul_derivative(_11801, _11802, intervalFailed, optical_product_upper);
        Interval _11803 = _11928;
        Interval _11804 = n.x.v;
        Interval _11805 = Interval{ 0.0, 0.0 };
        Interval _11929 = jet_mul_derivative(_11804, _11805, intervalFailed, optical_product_upper);
        Interval _11806 = _11929;
        Interval _11930 = jet_add_derivative(_11803, _11806, intervalFailed);
        Interval _11779 = n.y.v;
        Interval _11780 = Interval{ -1.0, -1.0 };
        Interval _11934 = imul(_11779, _11780, intervalFailed, optical_product_upper);
        Interval _11781 = n.y.dx;
        Interval _11782 = Interval{ -1.0, -1.0 };
        Interval _11935 = jet_mul_derivative(_11781, _11782, intervalFailed, optical_product_upper);
        Interval _11783 = _11935;
        Interval _11784 = n.y.v;
        Interval _11785 = Interval{ 0.0, 0.0 };
        Interval _11936 = jet_mul_derivative(_11784, _11785, intervalFailed, optical_product_upper);
        Interval _11786 = _11936;
        Interval _11937 = jet_add_derivative(_11783, _11786, intervalFailed);
        Interval _11787 = n.y.dy;
        Interval _11788 = Interval{ -1.0, -1.0 };
        Interval _11938 = jet_mul_derivative(_11787, _11788, intervalFailed, optical_product_upper);
        Interval _11789 = _11938;
        Interval _11790 = n.y.v;
        Interval _11791 = Interval{ 0.0, 0.0 };
        Interval _11939 = jet_mul_derivative(_11790, _11791, intervalFailed, optical_product_upper);
        Interval _11792 = _11939;
        Interval _11940 = jet_add_derivative(_11789, _11792, intervalFailed);
        Interval _11765 = n.z.v;
        Interval _11766 = Interval{ -1.0, -1.0 };
        Interval _11944 = imul(_11765, _11766, intervalFailed, optical_product_upper);
        Interval _11767 = n.z.dx;
        Interval _11768 = Interval{ -1.0, -1.0 };
        Interval _11945 = jet_mul_derivative(_11767, _11768, intervalFailed, optical_product_upper);
        Interval _11769 = _11945;
        Interval _11770 = n.z.v;
        Interval _11771 = Interval{ 0.0, 0.0 };
        Interval _11946 = jet_mul_derivative(_11770, _11771, intervalFailed, optical_product_upper);
        Interval _11772 = _11946;
        Interval _11947 = jet_add_derivative(_11769, _11772, intervalFailed);
        Interval _11773 = n.z.dy;
        Interval _11774 = Interval{ -1.0, -1.0 };
        Interval _11948 = jet_mul_derivative(_11773, _11774, intervalFailed, optical_product_upper);
        Interval _11775 = _11948;
        Interval _11776 = n.z.v;
        Interval _11777 = Interval{ 0.0, 0.0 };
        Interval _11949 = jet_mul_derivative(_11776, _11777, intervalFailed, optical_product_upper);
        Interval _11778 = _11949;
        Interval _11950 = jet_add_derivative(_11775, _11778, intervalFailed);
        return OpticalJet3{ OpticalJet{ _11924, _11927, _11930 }, OpticalJet{ _11934, _11937, _11940 }, OpticalJet{ _11944, _11947, _11950 } };
    }
    if (_11911.hi < 0.0)
    {
        return n;
    }
    jetBranchKnown = false;
    return n;
}

static inline __attribute__((always_inline))
bool jet_inside_face(thread const ReflectionSpecularPlane& plane, thread const OpticalJet3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    bool _15684;
    if (plane.a.w == 2.0)
    {
        _15684 = plane.b.w == 2.0;
    }
    else
    {
        _15684 = false;
    }
    bool _15689;
    if (_15684)
    {
        _15689 = plane.c.w == 2.0;
    }
    else
    {
        _15689 = false;
    }
    if (_15689)
    {
        return true;
    }
    Interval _15668 = Interval{ plane.b.x, plane.b.x };
    Interval _15669 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15720 = iadd(_15668, _15669, intervalFailed);
    Interval _15670 = Interval{ plane.b.y, plane.b.y };
    Interval _15671 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15723 = iadd(_15670, _15671, intervalFailed);
    Interval _15672 = Interval{ plane.b.z, plane.b.z };
    Interval _15673 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15726 = iadd(_15672, _15673, intervalFailed);
    Interval _15662 = Interval{ plane.c.x, plane.c.x };
    Interval _15663 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15757 = iadd(_15662, _15663, intervalFailed);
    Interval _15664 = Interval{ plane.c.y, plane.c.y };
    Interval _15665 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15760 = iadd(_15664, _15665, intervalFailed);
    Interval _15666 = Interval{ plane.c.z, plane.c.z };
    Interval _15667 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15763 = iadd(_15666, _15667, intervalFailed);
    Interval _15656 = hit.x.v;
    Interval _15657 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15794 = iadd(_15656, _15657, intervalFailed);
    Interval _15658 = hit.y.v;
    Interval _15659 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15796 = iadd(_15658, _15659, intervalFailed);
    Interval _15660 = hit.z.v;
    Interval _15661 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15798 = iadd(_15660, _15661, intervalFailed);
    Interval _15646 = _15720;
    Interval _15647 = _15720;
    Interval _15799 = imul(_15646, _15647, intervalFailed, optical_product_upper);
    Interval _15648 = _15799;
    Interval _15649 = _15723;
    Interval _15650 = _15723;
    Interval _15800 = imul(_15649, _15650, intervalFailed, optical_product_upper);
    Interval _15651 = _15800;
    Interval _15801 = iadd(_15648, _15651, intervalFailed);
    Interval _15652 = _15801;
    Interval _15653 = _15726;
    Interval _15654 = _15726;
    Interval _15802 = imul(_15653, _15654, intervalFailed, optical_product_upper);
    Interval _15655 = _15802;
    Interval _15803 = iadd(_15652, _15655, intervalFailed);
    Interval _15636 = _15720;
    Interval _15637 = _15757;
    Interval _15804 = imul(_15636, _15637, intervalFailed, optical_product_upper);
    Interval _15638 = _15804;
    Interval _15639 = _15723;
    Interval _15640 = _15760;
    Interval _15805 = imul(_15639, _15640, intervalFailed, optical_product_upper);
    Interval _15641 = _15805;
    Interval _15806 = iadd(_15638, _15641, intervalFailed);
    Interval _15642 = _15806;
    Interval _15643 = _15726;
    Interval _15644 = _15763;
    Interval _15807 = imul(_15643, _15644, intervalFailed, optical_product_upper);
    Interval _15645 = _15807;
    Interval _15808 = iadd(_15642, _15645, intervalFailed);
    Interval _15626 = _15757;
    Interval _15627 = _15757;
    Interval _15809 = imul(_15626, _15627, intervalFailed, optical_product_upper);
    Interval _15628 = _15809;
    Interval _15629 = _15760;
    Interval _15630 = _15760;
    Interval _15810 = imul(_15629, _15630, intervalFailed, optical_product_upper);
    Interval _15631 = _15810;
    Interval _15811 = iadd(_15628, _15631, intervalFailed);
    Interval _15632 = _15811;
    Interval _15633 = _15763;
    Interval _15634 = _15763;
    Interval _15812 = imul(_15633, _15634, intervalFailed, optical_product_upper);
    Interval _15635 = _15812;
    Interval _15813 = iadd(_15632, _15635, intervalFailed);
    Interval _15616 = _15794;
    Interval _15617 = _15720;
    Interval _15814 = imul(_15616, _15617, intervalFailed, optical_product_upper);
    Interval _15618 = _15814;
    Interval _15619 = _15796;
    Interval _15620 = _15723;
    Interval _15815 = imul(_15619, _15620, intervalFailed, optical_product_upper);
    Interval _15621 = _15815;
    Interval _15816 = iadd(_15618, _15621, intervalFailed);
    Interval _15622 = _15816;
    Interval _15623 = _15798;
    Interval _15624 = _15726;
    Interval _15817 = imul(_15623, _15624, intervalFailed, optical_product_upper);
    Interval _15625 = _15817;
    Interval _15818 = iadd(_15622, _15625, intervalFailed);
    Interval _15606 = _15794;
    Interval _15607 = _15757;
    Interval _15819 = imul(_15606, _15607, intervalFailed, optical_product_upper);
    Interval _15608 = _15819;
    Interval _15609 = _15796;
    Interval _15610 = _15760;
    Interval _15820 = imul(_15609, _15610, intervalFailed, optical_product_upper);
    Interval _15611 = _15820;
    Interval _15821 = iadd(_15608, _15611, intervalFailed);
    Interval _15612 = _15821;
    Interval _15613 = _15798;
    Interval _15614 = _15763;
    Interval _15822 = imul(_15613, _15614, intervalFailed, optical_product_upper);
    Interval _15615 = _15822;
    Interval _15823 = iadd(_15612, _15615, intervalFailed);
    Interval param_var_a = _15803;
    Interval param_var_b = _15813;
    Interval _15824 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    bool _15831;
    if (_15808.lo <= 0.0)
    {
        _15831 = _15808.hi >= 0.0;
    }
    else
    {
        _15831 = false;
    }
    float _15838;
    if (_15831)
    {
        _15838 = 0.0;
    }
    else
    {
        _15838 = precise::min(abs(_15808.lo), abs(_15808.hi));
    }
    float _15841 = precise::max(abs(_15808.lo), abs(_15808.hi));
    float _15604 = spvFMul(_15838, _15838);
    float _15842 = interval_down(_15604, intervalFailed);
    float _15605 = spvFMul(_15841, _15841);
    float _15844 = interval_up(_15605, intervalFailed);
    Interval _15602 = _15824;
    Interval _15603 = Interval{ as_type<float>(as_type<uint>(_15844) ^ 2147483648u), as_type<float>(as_type<uint>(precise::max(0.0, _15842)) ^ 2147483648u) };
    Interval _15852 = iadd(_15602, _15603, intervalFailed);
    if (_15852.lo <= 0.0)
    {
        return false;
    }
    Interval param_var_a_1 = _15813;
    Interval param_var_b_1 = _15818;
    Interval _15855 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
    Interval param_var_a_2 = _15808;
    Interval param_var_b_2 = _15823;
    Interval _15856 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval _15600 = _15855;
    Interval _15601 = Interval{ as_type<float>(as_type<uint>(_15856.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15856.lo) ^ 2147483648u) };
    Interval _15866 = iadd(_15600, _15601, intervalFailed);
    Interval param_var_a_3 = _15866;
    Interval param_var_b_3 = _15852;
    Interval _15867 = idiv(param_var_a_3, param_var_b_3, intervalFailed, interval_divide_upper);
    Interval param_var_a_4 = _15803;
    Interval param_var_b_4 = _15823;
    Interval _15869 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
    Interval param_var_a_5 = _15808;
    Interval param_var_b_5 = _15818;
    Interval _15870 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
    Interval _15598 = _15869;
    Interval _15599 = Interval{ as_type<float>(as_type<uint>(_15870.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15870.lo) ^ 2147483648u) };
    Interval _15880 = iadd(_15598, _15599, intervalFailed);
    Interval param_var_a_6 = _15880;
    Interval param_var_b_6 = _15852;
    Interval _15881 = idiv(param_var_a_6, param_var_b_6, intervalFailed, interval_divide_upper);
    float _15596 = -9.9999997473787516355514526367188e-06;
    float _15883 = interval_down(_15596, intervalFailed);
    float _15597 = -9.9999997473787516355514526367188e-06;
    float _15884 = interval_up(_15597, intervalFailed);
    bool _15889;
    if (_15867.lo >= _15884)
    {
        float _15594 = -9.9999997473787516355514526367188e-06;
        float _15886 = interval_down(_15594, intervalFailed);
        float _15595 = -9.9999997473787516355514526367188e-06;
        float _15887 = interval_up(_15595, intervalFailed);
        _15889 = _15881.lo >= _15887;
    }
    else
    {
        _15889 = false;
    }
    bool _15895;
    if (_15889)
    {
        Interval param_var_a_7 = _15867;
        Interval param_var_b_7 = _15881;
        Interval _15890 = iadd(param_var_a_7, param_var_b_7, intervalFailed);
        float _15592 = 1.000010013580322265625;
        float _15892 = interval_down(_15592, intervalFailed);
        float _15593 = 1.000010013580322265625;
        float _15893 = interval_up(_15593, intervalFailed);
        _15895 = _15890.hi <= _15892;
    }
    else
    {
        _15895 = false;
    }
    return _15895;
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
    bool _12035;
    if (a.v.lo <= 0.0)
    {
        _12035 = a.v.hi >= 0.0;
    }
    else
    {
        _12035 = false;
    }
    float _12042;
    if (_12035)
    {
        _12042 = 0.0;
    }
    else
    {
        _12042 = precise::min(abs(a.v.lo), abs(a.v.hi));
    }
    return OpticalJet{ Interval{ _12042, precise::max(abs(a.v.lo), abs(a.v.hi)) }, Interval{ precise::min(a.dx.lo, as_type<float>(as_type<uint>(a.dx.hi) ^ 2147483648u)), precise::max(a.dx.hi, as_type<float>(as_type<uint>(a.dx.lo) ^ 2147483648u)) }, Interval{ precise::min(a.dy.lo, as_type<float>(as_type<uint>(a.dy.hi) ^ 2147483648u)), precise::max(a.dy.hi, as_type<float>(as_type<uint>(a.dy.lo) ^ 2147483648u)) } };
}

static inline __attribute__((always_inline))
Interval iratio(thread const float& n, thread const float& d, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    if (d == 10.0)
    {
        Interval param_var_a = Interval{ n, n };
        Interval param_var_b = Interval{ as_type<float>(1036831948u), as_type<float>(1036831950u) };
        Interval _16564 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        return _16564;
    }
    if (d == 100.0)
    {
        Interval param_var_a_1 = Interval{ n, n };
        Interval param_var_b_1 = Interval{ as_type<float>(1008981769u), as_type<float>(1008981771u) };
        Interval _16572 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        return _16572;
    }
    if (d == 1000.0)
    {
        Interval param_var_a_2 = Interval{ n, n };
        Interval param_var_b_2 = Interval{ as_type<float>(981668462u), as_type<float>(981668464u) };
        Interval _16580 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        return _16580;
    }
    if (d == 10000.0)
    {
        Interval param_var_a_3 = Interval{ n, n };
        Interval param_var_b_3 = Interval{ as_type<float>(953267990u), as_type<float>(953267992u) };
        Interval _16588 = imul(param_var_a_3, param_var_b_3, intervalFailed, optical_product_upper);
        return _16588;
    }
    if (d == 100000.0)
    {
        Interval param_var_a_4 = Interval{ n, n };
        Interval param_var_b_4 = Interval{ as_type<float>(925353387u), as_type<float>(925353389u) };
        Interval _16596 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
        return _16596;
    }
    if (d == 128.0)
    {
        Interval param_var_a_5 = Interval{ n, n };
        Interval param_var_b_5 = Interval{ as_type<float>(1006632960u), as_type<float>(1006632960u) };
        Interval _16604 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
        return _16604;
    }
    if (d == 65535.0)
    {
        Interval param_var_a_6 = Interval{ n, n };
        Interval param_var_b_6 = Interval{ as_type<float>(931135615u), as_type<float>(931135617u) };
        Interval _16612 = imul(param_var_a_6, param_var_b_6, intervalFailed, optical_product_upper);
        return _16612;
    }
    Interval param_var_a_7 = Interval{ n, n };
    Interval param_var_b_7 = Interval{ d, d };
    Interval _16617 = idiv(param_var_a_7, param_var_b_7, intervalFailed, interval_divide_upper);
    return _16617;
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
    bool _21627;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _21627 = isnan(a.hi) || isinf(a.hi);
    }
    else
    {
        _21627 = true;
    }
    bool _21637;
    if (!_21627)
    {
        _21637 = precise::max(abs(a.lo), abs(a.hi)) > 1048576.0;
    }
    else
    {
        _21637 = true;
    }
    if (_21637)
    {
        intervalFailed = true;
        return Interval{ -1.0, 1.0 };
    }
    float _1730 = spvFMul(floor(spvFAdd(spvFMul(spvFAdd(a.lo, a.hi), 0.5) / 6.283185482025146484375, 0.5)), 2.0);
    Interval param_var_a = Interval{ _1730, _1730 };
    float _21614 = 3.1415927410125732421875;
    float _21645 = interval_down(_21614, intervalFailed);
    float _21615 = 3.1415927410125732421875;
    float _21646 = interval_up(_21615, intervalFailed);
    Interval param_var_b = Interval{ _21645, _21646 };
    Interval _21648 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    Interval _21612 = a;
    Interval _21613 = Interval{ as_type<float>(as_type<uint>(_21648.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21648.lo) ^ 2147483648u) };
    Interval _21658 = iadd(_21612, _21613, intervalFailed);
    float _21610 = 3.1415927410125732421875;
    float _21661 = interval_down(_21610, intervalFailed);
    float _21611 = 3.1415927410125732421875;
    float _21662 = interval_up(_21611, intervalFailed);
    Interval param_var_a_1 = Interval{ _21661, _21662 };
    Interval param_var_b_1 = Interval{ 0.5, 0.5 };
    Interval _21664 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
    float _21706;
    float _21707;
    if (_21658.lo > _21664.hi)
    {
        float _21608 = 3.1415927410125732421875;
        float _21691 = interval_down(_21608, intervalFailed);
        float _21609 = 3.1415927410125732421875;
        float _21692 = interval_up(_21609, intervalFailed);
        Interval _21606 = Interval{ _21691, _21692 };
        Interval _21607 = Interval{ as_type<float>(as_type<uint>(_21658.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21658.lo) ^ 2147483648u) };
        Interval _21703 = iadd(_21606, _21607, intervalFailed);
        _21706 = _21703.hi;
        _21707 = _21703.lo;
    }
    else
    {
        float _21689;
        float _21690;
        if (_21658.hi < (-_21664.hi))
        {
            float _21604 = 3.1415927410125732421875;
            float _21668 = interval_down(_21604, intervalFailed);
            float _21605 = 3.1415927410125732421875;
            float _21669 = interval_up(_21605, intervalFailed);
            Interval _21602 = Interval{ as_type<float>(as_type<uint>(_21669) ^ 2147483648u), as_type<float>(as_type<uint>(_21668) ^ 2147483648u) };
            Interval _21603 = Interval{ as_type<float>(as_type<uint>(_21658.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21658.lo) ^ 2147483648u) };
            Interval _21686 = iadd(_21602, _21603, intervalFailed);
            _21689 = _21686.hi;
            _21690 = _21686.lo;
        }
        else
        {
            _21689 = _21658.hi;
            _21690 = _21658.lo;
        }
        _21706 = _21689;
        _21707 = _21690;
    }
    float _1732 = -_21664.hi;
    bool _21710;
    if ((isunordered(_21707, _1732) || _21707 >= _1732))
    {
        _21710 = _21706 > _21664.hi;
    }
    else
    {
        _21710 = true;
    }
    if (_21710)
    {
        return Interval{ -1.0, 1.0 };
    }
    bool _21715;
    if (_21707 <= 0.0)
    {
        _21715 = _21706 >= 0.0;
    }
    else
    {
        _21715 = false;
    }
    float _21722;
    if (_21715)
    {
        _21722 = 0.0;
    }
    else
    {
        _21722 = precise::min(abs(_21707), abs(_21706));
    }
    float _21725 = precise::max(abs(_21707), abs(_21706));
    float _21600 = spvFMul(_21722, _21722);
    float _21726 = interval_down(_21600, intervalFailed);
    float _21727 = precise::max(0.0, _21726);
    float _21601 = spvFMul(_21725, _21725);
    float _21728 = interval_up(_21601, intervalFailed);
    float _21598 = _2288[8];
    float _21731 = interval_down(_21598, intervalFailed);
    float _21599 = _2288[8];
    float _21732 = interval_up(_21599, intervalFailed);
    float _21733;
    float _21735;
    _21733 = _21731;
    _21735 = _21732;
    float _21734;
    float _21736;
    for (int _21737 = 7; _21737 >= 0; _21733 = _21734, _21735 = _21736, _21737--)
    {
        Interval param_var_a_2 = Interval{ _21733, _21735 };
        Interval param_var_b_2 = Interval{ _21727, _21728 };
        Interval _21741 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        Interval param_var_a_3 = _21741;
        float _21596 = _2288[_21737];
        float _21744 = interval_down(_21596, intervalFailed);
        float _21597 = _2288[_21737];
        float _21745 = interval_up(_21597, intervalFailed);
        Interval param_var_b_3 = Interval{ _21744, _21745 };
        Interval _21747 = iadd(param_var_a_3, param_var_b_3, intervalFailed);
        _21734 = _21747.lo;
        _21736 = _21747.hi;
    }
    Interval param_var_a_4 = Interval{ _21707, _21706 };
    Interval param_var_b_4 = Interval{ _21733, _21735 };
    Interval _21750 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
    Interval param_var_a_5 = _21750;
    Interval param_var_b_5 = Interval{ -3.9999999840167888010000751819462e-12, 3.9999999840167888010000751819462e-12 };
    Interval _21751 = iadd(param_var_a_5, param_var_b_5, intervalFailed);
    return Interval{ precise::min(precise::max(_21751.lo, -1.0), 1.0), precise::min(precise::max(_21751.hi, -1.0), 1.0) };
}

static inline __attribute__((always_inline))
float sine_bounds(thread const float& lo, thread const float& hi, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_sine_upper)
{
    Interval param_var_a = Interval{ lo, hi };
    Interval _21593 = isin_body(param_var_a, intervalFailed, optical_product_upper);
    interval_sine_upper = _21593.hi;
    return _21593.lo;
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

static inline __attribute__((always_inline))
OpticalJet3 jlava(thread const OpticalJet& x, thread const OpticalJet& z, thread const OpticalJet& t, thread const OpticalJet& footprint, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    float _18418 = 6.0;
    float _18419 = 1000.0;
    Interval _18425 = iratio(_18418, _18419, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18430;
    if (!intervalFailed)
    {
        _18430 = intervalFailed;
    }
    else
    {
        _18430 = false;
    }
    bool _18435;
    if (_18430)
    {
        _18435 = jetFailureSite == 0u;
    }
    else
    {
        _18435 = false;
    }
    if (_18435)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(6.0, 6.0, 1000.0, 1000.0);
    }
    Interval _18404 = x.v;
    Interval _18405 = _18425;
    Interval _18438 = imul(_18404, _18405, intervalFailed, optical_product_upper);
    Interval _18406 = x.dx;
    Interval _18407 = _18425;
    Interval _18439 = jet_mul_derivative(_18406, _18407, intervalFailed, optical_product_upper);
    Interval _18408 = _18439;
    Interval _18409 = x.v;
    Interval _18410 = Interval{ 0.0, 0.0 };
    Interval _18440 = jet_mul_derivative(_18409, _18410, intervalFailed, optical_product_upper);
    Interval _18411 = _18440;
    Interval _18441 = jet_add_derivative(_18408, _18411, intervalFailed);
    Interval _18412 = x.dy;
    Interval _18413 = _18425;
    Interval _18442 = jet_mul_derivative(_18412, _18413, intervalFailed, optical_product_upper);
    Interval _18414 = _18442;
    Interval _18415 = x.v;
    Interval _18416 = Interval{ 0.0, 0.0 };
    Interval _18443 = jet_mul_derivative(_18415, _18416, intervalFailed, optical_product_upper);
    Interval _18417 = _18443;
    Interval _18444 = jet_add_derivative(_18414, _18417, intervalFailed);
    float _18402 = 8.0;
    float _18403 = 1000.0;
    Interval _18450 = iratio(_18402, _18403, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18455;
    if (!intervalFailed)
    {
        _18455 = intervalFailed;
    }
    else
    {
        _18455 = false;
    }
    bool _18460;
    if (_18455)
    {
        _18460 = jetFailureSite == 0u;
    }
    else
    {
        _18460 = false;
    }
    if (_18460)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(8.0, 8.0, 1000.0, 1000.0);
    }
    Interval _18388 = z.v;
    Interval _18389 = _18450;
    Interval _18463 = imul(_18388, _18389, intervalFailed, optical_product_upper);
    Interval _18390 = z.dx;
    Interval _18391 = _18450;
    Interval _18464 = jet_mul_derivative(_18390, _18391, intervalFailed, optical_product_upper);
    Interval _18392 = _18464;
    Interval _18393 = z.v;
    Interval _18394 = Interval{ 0.0, 0.0 };
    Interval _18465 = jet_mul_derivative(_18393, _18394, intervalFailed, optical_product_upper);
    Interval _18395 = _18465;
    Interval _18466 = jet_add_derivative(_18392, _18395, intervalFailed);
    Interval _18396 = z.dy;
    Interval _18397 = _18450;
    Interval _18467 = jet_mul_derivative(_18396, _18397, intervalFailed, optical_product_upper);
    Interval _18398 = _18467;
    Interval _18399 = z.v;
    Interval _18400 = Interval{ 0.0, 0.0 };
    Interval _18468 = jet_mul_derivative(_18399, _18400, intervalFailed, optical_product_upper);
    Interval _18401 = _18468;
    Interval _18469 = jet_add_derivative(_18398, _18401, intervalFailed);
    Interval _18382 = _18438;
    Interval _18383 = Interval{ as_type<float>(as_type<uint>(_18463.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18463.lo) ^ 2147483648u) };
    Interval _18495 = iadd(_18382, _18383, intervalFailed);
    Interval _18384 = _18441;
    Interval _18385 = Interval{ as_type<float>(as_type<uint>(_18466.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18466.lo) ^ 2147483648u) };
    Interval _18497 = jet_add_derivative(_18384, _18385, intervalFailed);
    Interval _18386 = _18444;
    Interval _18387 = Interval{ as_type<float>(as_type<uint>(_18469.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18469.lo) ^ 2147483648u) };
    Interval _18499 = jet_add_derivative(_18386, _18387, intervalFailed);
    float _18380 = 11.0;
    float _18381 = 100.0;
    Interval _18505 = iratio(_18380, _18381, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18510;
    if (!intervalFailed)
    {
        _18510 = intervalFailed;
    }
    else
    {
        _18510 = false;
    }
    bool _18515;
    if (_18510)
    {
        _18515 = jetFailureSite == 0u;
    }
    else
    {
        _18515 = false;
    }
    if (_18515)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 100.0, 100.0);
    }
    Interval _18366 = t.v;
    Interval _18367 = _18505;
    Interval _18518 = imul(_18366, _18367, intervalFailed, optical_product_upper);
    Interval _18368 = t.dx;
    Interval _18369 = _18505;
    Interval _18519 = jet_mul_derivative(_18368, _18369, intervalFailed, optical_product_upper);
    Interval _18370 = _18519;
    Interval _18371 = t.v;
    Interval _18372 = Interval{ 0.0, 0.0 };
    Interval _18520 = jet_mul_derivative(_18371, _18372, intervalFailed, optical_product_upper);
    Interval _18373 = _18520;
    Interval _18521 = jet_add_derivative(_18370, _18373, intervalFailed);
    Interval _18374 = t.dy;
    Interval _18375 = _18505;
    Interval _18522 = jet_mul_derivative(_18374, _18375, intervalFailed, optical_product_upper);
    Interval _18376 = _18522;
    Interval _18377 = t.v;
    Interval _18378 = Interval{ 0.0, 0.0 };
    Interval _18523 = jet_mul_derivative(_18377, _18378, intervalFailed, optical_product_upper);
    Interval _18379 = _18523;
    Interval _18524 = jet_add_derivative(_18376, _18379, intervalFailed);
    Interval _18360 = _18495;
    Interval _18361 = _18518;
    Interval _18525 = iadd(_18360, _18361, intervalFailed);
    Interval _18362 = _18497;
    Interval _18363 = _18521;
    Interval _18526 = jet_add_derivative(_18362, _18363, intervalFailed);
    Interval _18364 = _18499;
    Interval _18365 = _18524;
    Interval _18527 = jet_add_derivative(_18364, _18365, intervalFailed);
    float _18358 = 10.0;
    float _18359 = 1000.0;
    Interval _18530 = iratio(_18358, _18359, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18535;
    if (!intervalFailed)
    {
        _18535 = intervalFailed;
    }
    else
    {
        _18535 = false;
    }
    bool _18540;
    if (_18535)
    {
        _18540 = jetFailureSite == 0u;
    }
    else
    {
        _18540 = false;
    }
    if (_18540)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(10.0, 10.0, 1000.0, 1000.0);
    }
    Interval _18344 = footprint.v;
    Interval _18345 = _18530;
    Interval _18546 = imul(_18344, _18345, intervalFailed, optical_product_upper);
    Interval _18346 = footprint.dx;
    Interval _18347 = _18530;
    Interval _18547 = jet_mul_derivative(_18346, _18347, intervalFailed, optical_product_upper);
    Interval _18348 = _18547;
    Interval _18349 = footprint.v;
    Interval _18350 = Interval{ 0.0, 0.0 };
    Interval _18548 = jet_mul_derivative(_18349, _18350, intervalFailed, optical_product_upper);
    Interval _18351 = _18548;
    Interval _18549 = jet_add_derivative(_18348, _18351, intervalFailed);
    Interval _18352 = footprint.dy;
    Interval _18353 = _18530;
    Interval _18550 = jet_mul_derivative(_18352, _18353, intervalFailed, optical_product_upper);
    Interval _18354 = _18550;
    Interval _18355 = footprint.v;
    Interval _18356 = Interval{ 0.0, 0.0 };
    Interval _18551 = jet_mul_derivative(_18355, _18356, intervalFailed, optical_product_upper);
    Interval _18357 = _18551;
    Interval _18552 = jet_add_derivative(_18354, _18357, intervalFailed);
    bool _18559;
    if (_18546.lo <= 0.0)
    {
        _18559 = _18546.hi >= 0.0;
    }
    else
    {
        _18559 = false;
    }
    float _18566;
    if (_18559)
    {
        _18566 = 0.0;
    }
    else
    {
        _18566 = precise::min(abs(_18546.lo), abs(_18546.hi));
    }
    float _18569 = precise::max(abs(_18546.lo), abs(_18546.hi));
    float _18334 = spvFMul(_18566, _18566);
    float _18570 = interval_down(_18334, intervalFailed);
    float _18571 = precise::max(0.0, _18570);
    float _18335 = spvFMul(_18569, _18569);
    float _18572 = interval_up(_18335, intervalFailed);
    Interval _18336 = Interval{ 2.0, 2.0 };
    Interval _18337 = _18546;
    Interval _18573 = imul(_18336, _18337, intervalFailed, optical_product_upper);
    Interval _18338 = _18573;
    Interval _18339 = _18549;
    Interval _18574 = jet_mul_derivative(_18338, _18339, intervalFailed, optical_product_upper);
    Interval _18340 = Interval{ 2.0, 2.0 };
    Interval _18341 = _18546;
    Interval _18575 = imul(_18340, _18341, intervalFailed, optical_product_upper);
    Interval _18342 = _18575;
    Interval _18343 = _18552;
    Interval _18576 = jet_mul_derivative(_18342, _18343, intervalFailed, optical_product_upper);
    bool _18581;
    if (_18571 <= 0.0)
    {
        _18581 = _18572 >= 0.0;
    }
    else
    {
        _18581 = false;
    }
    float _18588;
    if (_18581)
    {
        _18588 = 0.0;
    }
    else
    {
        _18588 = precise::min(abs(_18571), abs(_18572));
    }
    float _18591 = precise::max(abs(_18571), abs(_18572));
    float _18324 = spvFMul(_18588, _18588);
    float _18592 = interval_down(_18324, intervalFailed);
    float _18325 = spvFMul(_18591, _18591);
    float _18594 = interval_up(_18325, intervalFailed);
    Interval _18326 = Interval{ 2.0, 2.0 };
    Interval _18327 = Interval{ _18571, _18572 };
    Interval _18596 = imul(_18326, _18327, intervalFailed, optical_product_upper);
    Interval _18328 = _18596;
    Interval _18329 = _18574;
    Interval _18597 = jet_mul_derivative(_18328, _18329, intervalFailed, optical_product_upper);
    Interval _18330 = Interval{ 2.0, 2.0 };
    Interval _18331 = Interval{ _18571, _18572 };
    Interval _18599 = imul(_18330, _18331, intervalFailed, optical_product_upper);
    Interval _18332 = _18599;
    Interval _18333 = _18576;
    Interval _18600 = jet_mul_derivative(_18332, _18333, intervalFailed, optical_product_upper);
    Interval _18318 = Interval{ 1.0, 1.0 };
    Interval _18319 = Interval{ precise::max(0.0, _18592), _18594 };
    Interval _18602 = iadd(_18318, _18319, intervalFailed);
    Interval _18320 = Interval{ 0.0, 0.0 };
    Interval _18321 = _18597;
    Interval _18603 = jet_add_derivative(_18320, _18321, intervalFailed);
    Interval _18322 = Interval{ 0.0, 0.0 };
    Interval _18323 = _18600;
    Interval _18604 = jet_add_derivative(_18322, _18323, intervalFailed);
    Interval _18304 = Interval{ 1.0, 1.0 };
    Interval _18305 = _18602;
    Interval _18606 = idiv(_18304, _18305, intervalFailed, interval_divide_upper);
    bool _18613;
    if (!intervalFailed)
    {
        _18613 = intervalFailed;
    }
    else
    {
        _18613 = false;
    }
    bool _18618;
    if (_18613)
    {
        _18618 = jetFailureSite == 0u;
    }
    else
    {
        _18618 = false;
    }
    if (_18618)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _18602.lo, _18602.hi);
    }
    Interval _18306 = Interval{ 0.0, 0.0 };
    Interval _18307 = _18606;
    Interval _18308 = _18603;
    Interval _18622 = jet_mul_derivative(_18307, _18308, intervalFailed, optical_product_upper);
    Interval _18309 = Interval{ as_type<float>(as_type<uint>(_18622.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18622.lo) ^ 2147483648u) };
    Interval _18632 = jet_add_derivative(_18306, _18309, intervalFailed);
    Interval _18310 = _18632;
    Interval _18311 = _18602;
    Interval _18633 = jet_div_derivative(_18310, _18311, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18312 = Interval{ 0.0, 0.0 };
    Interval _18313 = _18606;
    Interval _18314 = _18604;
    Interval _18634 = jet_mul_derivative(_18313, _18314, intervalFailed, optical_product_upper);
    Interval _18315 = Interval{ as_type<float>(as_type<uint>(_18634.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18634.lo) ^ 2147483648u) };
    Interval _18644 = jet_add_derivative(_18312, _18315, intervalFailed);
    Interval _18316 = _18644;
    Interval _18317 = _18602;
    Interval _18645 = jet_div_derivative(_18316, _18317, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18302 = 18.0;
    float _18303 = 1000.0;
    Interval _18651 = iratio(_18302, _18303, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18656;
    if (!intervalFailed)
    {
        _18656 = intervalFailed;
    }
    else
    {
        _18656 = false;
    }
    bool _18661;
    if (_18656)
    {
        _18661 = jetFailureSite == 0u;
    }
    else
    {
        _18661 = false;
    }
    if (_18661)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
    }
    Interval _18288 = x.v;
    Interval _18289 = _18651;
    Interval _18664 = imul(_18288, _18289, intervalFailed, optical_product_upper);
    Interval _18290 = x.dx;
    Interval _18291 = _18651;
    Interval _18665 = jet_mul_derivative(_18290, _18291, intervalFailed, optical_product_upper);
    Interval _18292 = _18665;
    Interval _18293 = x.v;
    Interval _18294 = Interval{ 0.0, 0.0 };
    Interval _18666 = jet_mul_derivative(_18293, _18294, intervalFailed, optical_product_upper);
    Interval _18295 = _18666;
    Interval _18667 = jet_add_derivative(_18292, _18295, intervalFailed);
    Interval _18296 = x.dy;
    Interval _18297 = _18651;
    Interval _18668 = jet_mul_derivative(_18296, _18297, intervalFailed, optical_product_upper);
    Interval _18298 = _18668;
    Interval _18299 = x.v;
    Interval _18300 = Interval{ 0.0, 0.0 };
    Interval _18669 = jet_mul_derivative(_18299, _18300, intervalFailed, optical_product_upper);
    Interval _18301 = _18669;
    Interval _18670 = jet_add_derivative(_18298, _18301, intervalFailed);
    float _18286 = 11.0;
    float _18287 = 1000.0;
    Interval _18676 = iratio(_18286, _18287, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18681;
    if (!intervalFailed)
    {
        _18681 = intervalFailed;
    }
    else
    {
        _18681 = false;
    }
    bool _18686;
    if (_18681)
    {
        _18686 = jetFailureSite == 0u;
    }
    else
    {
        _18686 = false;
    }
    if (_18686)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
    }
    Interval _18272 = z.v;
    Interval _18273 = _18676;
    Interval _18689 = imul(_18272, _18273, intervalFailed, optical_product_upper);
    Interval _18274 = z.dx;
    Interval _18275 = _18676;
    Interval _18690 = jet_mul_derivative(_18274, _18275, intervalFailed, optical_product_upper);
    Interval _18276 = _18690;
    Interval _18277 = z.v;
    Interval _18278 = Interval{ 0.0, 0.0 };
    Interval _18691 = jet_mul_derivative(_18277, _18278, intervalFailed, optical_product_upper);
    Interval _18279 = _18691;
    Interval _18692 = jet_add_derivative(_18276, _18279, intervalFailed);
    Interval _18280 = z.dy;
    Interval _18281 = _18676;
    Interval _18693 = jet_mul_derivative(_18280, _18281, intervalFailed, optical_product_upper);
    Interval _18282 = _18693;
    Interval _18283 = z.v;
    Interval _18284 = Interval{ 0.0, 0.0 };
    Interval _18694 = jet_mul_derivative(_18283, _18284, intervalFailed, optical_product_upper);
    Interval _18285 = _18694;
    Interval _18695 = jet_add_derivative(_18282, _18285, intervalFailed);
    Interval _18266 = _18664;
    Interval _18267 = _18689;
    Interval _18696 = iadd(_18266, _18267, intervalFailed);
    Interval _18268 = _18667;
    Interval _18269 = _18692;
    Interval _18697 = jet_add_derivative(_18268, _18269, intervalFailed);
    Interval _18270 = _18670;
    Interval _18271 = _18695;
    Interval _18698 = jet_add_derivative(_18270, _18271, intervalFailed);
    float _18264 = 45.0;
    float _18265 = 100.0;
    Interval _18704 = iratio(_18264, _18265, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18709;
    if (!intervalFailed)
    {
        _18709 = intervalFailed;
    }
    else
    {
        _18709 = false;
    }
    bool _18714;
    if (_18709)
    {
        _18714 = jetFailureSite == 0u;
    }
    else
    {
        _18714 = false;
    }
    if (_18714)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(45.0, 45.0, 100.0, 100.0);
    }
    Interval _18250 = t.v;
    Interval _18251 = _18704;
    Interval _18717 = imul(_18250, _18251, intervalFailed, optical_product_upper);
    Interval _18252 = t.dx;
    Interval _18253 = _18704;
    Interval _18718 = jet_mul_derivative(_18252, _18253, intervalFailed, optical_product_upper);
    Interval _18254 = _18718;
    Interval _18255 = t.v;
    Interval _18256 = Interval{ 0.0, 0.0 };
    Interval _18719 = jet_mul_derivative(_18255, _18256, intervalFailed, optical_product_upper);
    Interval _18257 = _18719;
    Interval _18720 = jet_add_derivative(_18254, _18257, intervalFailed);
    Interval _18258 = t.dy;
    Interval _18259 = _18704;
    Interval _18721 = jet_mul_derivative(_18258, _18259, intervalFailed, optical_product_upper);
    Interval _18260 = _18721;
    Interval _18261 = t.v;
    Interval _18262 = Interval{ 0.0, 0.0 };
    Interval _18722 = jet_mul_derivative(_18261, _18262, intervalFailed, optical_product_upper);
    Interval _18263 = _18722;
    Interval _18723 = jet_add_derivative(_18260, _18263, intervalFailed);
    Interval _18244 = _18696;
    Interval _18245 = Interval{ as_type<float>(as_type<uint>(_18717.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18717.lo) ^ 2147483648u) };
    Interval _18749 = iadd(_18244, _18245, intervalFailed);
    Interval _18246 = _18697;
    Interval _18247 = Interval{ as_type<float>(as_type<uint>(_18720.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18720.lo) ^ 2147483648u) };
    Interval _18751 = jet_add_derivative(_18246, _18247, intervalFailed);
    Interval _18248 = _18698;
    Interval _18249 = Interval{ as_type<float>(as_type<uint>(_18723.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18723.lo) ^ 2147483648u) };
    Interval _18753 = jet_add_derivative(_18248, _18249, intervalFailed);
    float _18242 = 65.0;
    float _18243 = 100.0;
    Interval _18755 = iratio(_18242, _18243, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18760;
    if (!intervalFailed)
    {
        _18760 = intervalFailed;
    }
    else
    {
        _18760 = false;
    }
    bool _18765;
    if (_18760)
    {
        _18765 = jetFailureSite == 0u;
    }
    else
    {
        _18765 = false;
    }
    if (_18765)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(65.0, 65.0, 100.0, 100.0);
    }
    Interval _18234 = _18525;
    float _18232 = 3.1415927410125732421875;
    float _18768 = interval_down(_18232, intervalFailed);
    float _18233 = 3.1415927410125732421875;
    float _18769 = interval_up(_18233, intervalFailed);
    Interval _18235 = Interval{ _18768, _18769 };
    Interval _18236 = Interval{ 0.5, 0.5 };
    Interval _18771 = imul(_18235, _18236, intervalFailed, optical_product_upper);
    Interval _18237 = _18771;
    Interval _18772 = iadd(_18234, _18237, intervalFailed);
    float _18230 = _18772.lo;
    float _18231 = _18772.hi;
    float _18775 = sine_bounds(_18230, _18231, intervalFailed, optical_product_upper, interval_sine_upper);
    float _18228 = _18525.lo;
    float _18229 = _18525.hi;
    float _18779 = sine_bounds(_18228, _18229, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18238 = Interval{ _18775, interval_sine_upper };
    Interval _18239 = _18526;
    Interval _18782 = jet_mul_derivative(_18238, _18239, intervalFailed, optical_product_upper);
    Interval _18240 = Interval{ _18775, interval_sine_upper };
    Interval _18241 = _18527;
    Interval _18784 = jet_mul_derivative(_18240, _18241, intervalFailed, optical_product_upper);
    Interval _18214 = _18755;
    Interval _18215 = Interval{ _18779, interval_sine_upper };
    Interval _18786 = imul(_18214, _18215, intervalFailed, optical_product_upper);
    Interval _18216 = Interval{ 0.0, 0.0 };
    Interval _18217 = Interval{ _18779, interval_sine_upper };
    Interval _18788 = jet_mul_derivative(_18216, _18217, intervalFailed, optical_product_upper);
    Interval _18218 = _18788;
    Interval _18219 = _18755;
    Interval _18220 = _18782;
    Interval _18789 = jet_mul_derivative(_18219, _18220, intervalFailed, optical_product_upper);
    Interval _18221 = _18789;
    Interval _18790 = jet_add_derivative(_18218, _18221, intervalFailed);
    Interval _18222 = Interval{ 0.0, 0.0 };
    Interval _18223 = Interval{ _18779, interval_sine_upper };
    Interval _18792 = jet_mul_derivative(_18222, _18223, intervalFailed, optical_product_upper);
    Interval _18224 = _18792;
    Interval _18225 = _18755;
    Interval _18226 = _18784;
    Interval _18793 = jet_mul_derivative(_18225, _18226, intervalFailed, optical_product_upper);
    Interval _18227 = _18793;
    Interval _18794 = jet_add_derivative(_18224, _18227, intervalFailed);
    Interval _18200 = _18786;
    Interval _18201 = _18606;
    Interval _18795 = imul(_18200, _18201, intervalFailed, optical_product_upper);
    Interval _18202 = _18790;
    Interval _18203 = _18606;
    Interval _18796 = jet_mul_derivative(_18202, _18203, intervalFailed, optical_product_upper);
    Interval _18204 = _18796;
    Interval _18205 = _18786;
    Interval _18206 = _18633;
    Interval _18797 = jet_mul_derivative(_18205, _18206, intervalFailed, optical_product_upper);
    Interval _18207 = _18797;
    Interval _18798 = jet_add_derivative(_18204, _18207, intervalFailed);
    Interval _18208 = _18794;
    Interval _18209 = _18606;
    Interval _18799 = jet_mul_derivative(_18208, _18209, intervalFailed, optical_product_upper);
    Interval _18210 = _18799;
    Interval _18211 = _18786;
    Interval _18212 = _18645;
    Interval _18800 = jet_mul_derivative(_18211, _18212, intervalFailed, optical_product_upper);
    Interval _18213 = _18800;
    Interval _18801 = jet_add_derivative(_18210, _18213, intervalFailed);
    Interval _18194 = _18749;
    Interval _18195 = _18795;
    Interval _18802 = iadd(_18194, _18195, intervalFailed);
    Interval _18196 = _18751;
    Interval _18197 = _18798;
    Interval _18803 = jet_add_derivative(_18196, _18197, intervalFailed);
    Interval _18198 = _18753;
    Interval _18199 = _18801;
    Interval _18804 = jet_add_derivative(_18198, _18199, intervalFailed);
    float _18192 = 47.0;
    float _18193 = 1000.0;
    Interval _18810 = iratio(_18192, _18193, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18815;
    if (!intervalFailed)
    {
        _18815 = intervalFailed;
    }
    else
    {
        _18815 = false;
    }
    bool _18820;
    if (_18815)
    {
        _18820 = jetFailureSite == 0u;
    }
    else
    {
        _18820 = false;
    }
    if (_18820)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(47.0, 47.0, 1000.0, 1000.0);
    }
    Interval _18178 = x.v;
    Interval _18179 = _18810;
    Interval _18823 = imul(_18178, _18179, intervalFailed, optical_product_upper);
    Interval _18180 = x.dx;
    Interval _18181 = _18810;
    Interval _18824 = jet_mul_derivative(_18180, _18181, intervalFailed, optical_product_upper);
    Interval _18182 = _18824;
    Interval _18183 = x.v;
    Interval _18184 = Interval{ 0.0, 0.0 };
    Interval _18825 = jet_mul_derivative(_18183, _18184, intervalFailed, optical_product_upper);
    Interval _18185 = _18825;
    Interval _18826 = jet_add_derivative(_18182, _18185, intervalFailed);
    Interval _18186 = x.dy;
    Interval _18187 = _18810;
    Interval _18827 = jet_mul_derivative(_18186, _18187, intervalFailed, optical_product_upper);
    Interval _18188 = _18827;
    Interval _18189 = x.v;
    Interval _18190 = Interval{ 0.0, 0.0 };
    Interval _18828 = jet_mul_derivative(_18189, _18190, intervalFailed, optical_product_upper);
    Interval _18191 = _18828;
    Interval _18829 = jet_add_derivative(_18188, _18191, intervalFailed);
    float _18176 = 25.0;
    float _18177 = 1000.0;
    Interval _18835 = iratio(_18176, _18177, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18840;
    if (!intervalFailed)
    {
        _18840 = intervalFailed;
    }
    else
    {
        _18840 = false;
    }
    bool _18845;
    if (_18840)
    {
        _18845 = jetFailureSite == 0u;
    }
    else
    {
        _18845 = false;
    }
    if (_18845)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
    }
    Interval _18162 = z.v;
    Interval _18163 = _18835;
    Interval _18848 = imul(_18162, _18163, intervalFailed, optical_product_upper);
    Interval _18164 = z.dx;
    Interval _18165 = _18835;
    Interval _18849 = jet_mul_derivative(_18164, _18165, intervalFailed, optical_product_upper);
    Interval _18166 = _18849;
    Interval _18167 = z.v;
    Interval _18168 = Interval{ 0.0, 0.0 };
    Interval _18850 = jet_mul_derivative(_18167, _18168, intervalFailed, optical_product_upper);
    Interval _18169 = _18850;
    Interval _18851 = jet_add_derivative(_18166, _18169, intervalFailed);
    Interval _18170 = z.dy;
    Interval _18171 = _18835;
    Interval _18852 = jet_mul_derivative(_18170, _18171, intervalFailed, optical_product_upper);
    Interval _18172 = _18852;
    Interval _18173 = z.v;
    Interval _18174 = Interval{ 0.0, 0.0 };
    Interval _18853 = jet_mul_derivative(_18173, _18174, intervalFailed, optical_product_upper);
    Interval _18175 = _18853;
    Interval _18854 = jet_add_derivative(_18172, _18175, intervalFailed);
    Interval _18156 = _18823;
    Interval _18157 = Interval{ as_type<float>(as_type<uint>(_18848.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18848.lo) ^ 2147483648u) };
    Interval _18880 = iadd(_18156, _18157, intervalFailed);
    Interval _18158 = _18826;
    Interval _18159 = Interval{ as_type<float>(as_type<uint>(_18851.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18851.lo) ^ 2147483648u) };
    Interval _18882 = jet_add_derivative(_18158, _18159, intervalFailed);
    Interval _18160 = _18829;
    Interval _18161 = Interval{ as_type<float>(as_type<uint>(_18854.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18854.lo) ^ 2147483648u) };
    Interval _18884 = jet_add_derivative(_18160, _18161, intervalFailed);
    float _18154 = 60.0;
    float _18155 = 100.0;
    Interval _18890 = iratio(_18154, _18155, intervalFailed, optical_product_upper, interval_divide_upper);
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
        jetFailureArguments = float4(60.0, 60.0, 100.0, 100.0);
    }
    Interval _18140 = t.v;
    Interval _18141 = _18890;
    Interval _18903 = imul(_18140, _18141, intervalFailed, optical_product_upper);
    Interval _18142 = t.dx;
    Interval _18143 = _18890;
    Interval _18904 = jet_mul_derivative(_18142, _18143, intervalFailed, optical_product_upper);
    Interval _18144 = _18904;
    Interval _18145 = t.v;
    Interval _18146 = Interval{ 0.0, 0.0 };
    Interval _18905 = jet_mul_derivative(_18145, _18146, intervalFailed, optical_product_upper);
    Interval _18147 = _18905;
    Interval _18906 = jet_add_derivative(_18144, _18147, intervalFailed);
    Interval _18148 = t.dy;
    Interval _18149 = _18890;
    Interval _18907 = jet_mul_derivative(_18148, _18149, intervalFailed, optical_product_upper);
    Interval _18150 = _18907;
    Interval _18151 = t.v;
    Interval _18152 = Interval{ 0.0, 0.0 };
    Interval _18908 = jet_mul_derivative(_18151, _18152, intervalFailed, optical_product_upper);
    Interval _18153 = _18908;
    Interval _18909 = jet_add_derivative(_18150, _18153, intervalFailed);
    Interval _18134 = _18880;
    Interval _18135 = _18903;
    Interval _18910 = iadd(_18134, _18135, intervalFailed);
    Interval _18136 = _18882;
    Interval _18137 = _18906;
    Interval _18911 = jet_add_derivative(_18136, _18137, intervalFailed);
    Interval _18138 = _18884;
    Interval _18139 = _18909;
    Interval _18912 = jet_add_derivative(_18138, _18139, intervalFailed);
    float _18132 = 22.0;
    float _18133 = 1000.0;
    Interval _18918 = iratio(_18132, _18133, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18923;
    if (!intervalFailed)
    {
        _18923 = intervalFailed;
    }
    else
    {
        _18923 = false;
    }
    bool _18928;
    if (_18923)
    {
        _18928 = jetFailureSite == 0u;
    }
    else
    {
        _18928 = false;
    }
    if (_18928)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
    }
    Interval _18118 = z.v;
    Interval _18119 = _18918;
    Interval _18931 = imul(_18118, _18119, intervalFailed, optical_product_upper);
    Interval _18120 = z.dx;
    Interval _18121 = _18918;
    Interval _18932 = jet_mul_derivative(_18120, _18121, intervalFailed, optical_product_upper);
    Interval _18122 = _18932;
    Interval _18123 = z.v;
    Interval _18124 = Interval{ 0.0, 0.0 };
    Interval _18933 = jet_mul_derivative(_18123, _18124, intervalFailed, optical_product_upper);
    Interval _18125 = _18933;
    Interval _18934 = jet_add_derivative(_18122, _18125, intervalFailed);
    Interval _18126 = z.dy;
    Interval _18127 = _18918;
    Interval _18935 = jet_mul_derivative(_18126, _18127, intervalFailed, optical_product_upper);
    Interval _18128 = _18935;
    Interval _18129 = z.v;
    Interval _18130 = Interval{ 0.0, 0.0 };
    Interval _18936 = jet_mul_derivative(_18129, _18130, intervalFailed, optical_product_upper);
    Interval _18131 = _18936;
    Interval _18937 = jet_add_derivative(_18128, _18131, intervalFailed);
    float _18116 = 9.0;
    float _18117 = 1000.0;
    Interval _18943 = iratio(_18116, _18117, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18948;
    if (!intervalFailed)
    {
        _18948 = intervalFailed;
    }
    else
    {
        _18948 = false;
    }
    bool _18953;
    if (_18948)
    {
        _18953 = jetFailureSite == 0u;
    }
    else
    {
        _18953 = false;
    }
    if (_18953)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(9.0, 9.0, 1000.0, 1000.0);
    }
    Interval _18102 = x.v;
    Interval _18103 = _18943;
    Interval _18956 = imul(_18102, _18103, intervalFailed, optical_product_upper);
    Interval _18104 = x.dx;
    Interval _18105 = _18943;
    Interval _18957 = jet_mul_derivative(_18104, _18105, intervalFailed, optical_product_upper);
    Interval _18106 = _18957;
    Interval _18107 = x.v;
    Interval _18108 = Interval{ 0.0, 0.0 };
    Interval _18958 = jet_mul_derivative(_18107, _18108, intervalFailed, optical_product_upper);
    Interval _18109 = _18958;
    Interval _18959 = jet_add_derivative(_18106, _18109, intervalFailed);
    Interval _18110 = x.dy;
    Interval _18111 = _18943;
    Interval _18960 = jet_mul_derivative(_18110, _18111, intervalFailed, optical_product_upper);
    Interval _18112 = _18960;
    Interval _18113 = x.v;
    Interval _18114 = Interval{ 0.0, 0.0 };
    Interval _18961 = jet_mul_derivative(_18113, _18114, intervalFailed, optical_product_upper);
    Interval _18115 = _18961;
    Interval _18962 = jet_add_derivative(_18112, _18115, intervalFailed);
    Interval _18096 = _18931;
    Interval _18097 = Interval{ as_type<float>(as_type<uint>(_18956.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18956.lo) ^ 2147483648u) };
    Interval _18988 = iadd(_18096, _18097, intervalFailed);
    Interval _18098 = _18934;
    Interval _18099 = Interval{ as_type<float>(as_type<uint>(_18959.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18959.lo) ^ 2147483648u) };
    Interval _18990 = jet_add_derivative(_18098, _18099, intervalFailed);
    Interval _18100 = _18937;
    Interval _18101 = Interval{ as_type<float>(as_type<uint>(_18962.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18962.lo) ^ 2147483648u) };
    Interval _18992 = jet_add_derivative(_18100, _18101, intervalFailed);
    float _18094 = 32.0;
    float _18095 = 100.0;
    Interval _18998 = iratio(_18094, _18095, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19003;
    if (!intervalFailed)
    {
        _19003 = intervalFailed;
    }
    else
    {
        _19003 = false;
    }
    bool _19008;
    if (_19003)
    {
        _19008 = jetFailureSite == 0u;
    }
    else
    {
        _19008 = false;
    }
    if (_19008)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(32.0, 32.0, 100.0, 100.0);
    }
    Interval _18080 = t.v;
    Interval _18081 = _18998;
    Interval _19011 = imul(_18080, _18081, intervalFailed, optical_product_upper);
    Interval _18082 = t.dx;
    Interval _18083 = _18998;
    Interval _19012 = jet_mul_derivative(_18082, _18083, intervalFailed, optical_product_upper);
    Interval _18084 = _19012;
    Interval _18085 = t.v;
    Interval _18086 = Interval{ 0.0, 0.0 };
    Interval _19013 = jet_mul_derivative(_18085, _18086, intervalFailed, optical_product_upper);
    Interval _18087 = _19013;
    Interval _19014 = jet_add_derivative(_18084, _18087, intervalFailed);
    Interval _18088 = t.dy;
    Interval _18089 = _18998;
    Interval _19015 = jet_mul_derivative(_18088, _18089, intervalFailed, optical_product_upper);
    Interval _18090 = _19015;
    Interval _18091 = t.v;
    Interval _18092 = Interval{ 0.0, 0.0 };
    Interval _19016 = jet_mul_derivative(_18091, _18092, intervalFailed, optical_product_upper);
    Interval _18093 = _19016;
    Interval _19017 = jet_add_derivative(_18090, _18093, intervalFailed);
    Interval _18074 = _18988;
    Interval _18075 = Interval{ as_type<float>(as_type<uint>(_19011.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19011.lo) ^ 2147483648u) };
    Interval _19043 = iadd(_18074, _18075, intervalFailed);
    Interval _18076 = _18990;
    Interval _18077 = Interval{ as_type<float>(as_type<uint>(_19014.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19014.lo) ^ 2147483648u) };
    Interval _19045 = jet_add_derivative(_18076, _18077, intervalFailed);
    Interval _18078 = _18992;
    Interval _18079 = Interval{ as_type<float>(as_type<uint>(_19017.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19017.lo) ^ 2147483648u) };
    Interval _19047 = jet_add_derivative(_18078, _18079, intervalFailed);
    float _18072 = 22.0;
    float _18073 = 1000.0;
    Interval _19050 = iratio(_18072, _18073, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19055;
    if (!intervalFailed)
    {
        _19055 = intervalFailed;
    }
    else
    {
        _19055 = false;
    }
    bool _19060;
    if (_19055)
    {
        _19060 = jetFailureSite == 0u;
    }
    else
    {
        _19060 = false;
    }
    if (_19060)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
    }
    Interval _18058 = footprint.v;
    Interval _18059 = _19050;
    Interval _19066 = imul(_18058, _18059, intervalFailed, optical_product_upper);
    Interval _18060 = footprint.dx;
    Interval _18061 = _19050;
    Interval _19067 = jet_mul_derivative(_18060, _18061, intervalFailed, optical_product_upper);
    Interval _18062 = _19067;
    Interval _18063 = footprint.v;
    Interval _18064 = Interval{ 0.0, 0.0 };
    Interval _19068 = jet_mul_derivative(_18063, _18064, intervalFailed, optical_product_upper);
    Interval _18065 = _19068;
    Interval _19069 = jet_add_derivative(_18062, _18065, intervalFailed);
    Interval _18066 = footprint.dy;
    Interval _18067 = _19050;
    Interval _19070 = jet_mul_derivative(_18066, _18067, intervalFailed, optical_product_upper);
    Interval _18068 = _19070;
    Interval _18069 = footprint.v;
    Interval _18070 = Interval{ 0.0, 0.0 };
    Interval _19071 = jet_mul_derivative(_18069, _18070, intervalFailed, optical_product_upper);
    Interval _18071 = _19071;
    Interval _19072 = jet_add_derivative(_18068, _18071, intervalFailed);
    bool _19079;
    if (_19066.lo <= 0.0)
    {
        _19079 = _19066.hi >= 0.0;
    }
    else
    {
        _19079 = false;
    }
    float _19086;
    if (_19079)
    {
        _19086 = 0.0;
    }
    else
    {
        _19086 = precise::min(abs(_19066.lo), abs(_19066.hi));
    }
    float _19089 = precise::max(abs(_19066.lo), abs(_19066.hi));
    float _18048 = spvFMul(_19086, _19086);
    float _19090 = interval_down(_18048, intervalFailed);
    float _19091 = precise::max(0.0, _19090);
    float _18049 = spvFMul(_19089, _19089);
    float _19092 = interval_up(_18049, intervalFailed);
    Interval _18050 = Interval{ 2.0, 2.0 };
    Interval _18051 = _19066;
    Interval _19093 = imul(_18050, _18051, intervalFailed, optical_product_upper);
    Interval _18052 = _19093;
    Interval _18053 = _19069;
    Interval _19094 = jet_mul_derivative(_18052, _18053, intervalFailed, optical_product_upper);
    Interval _18054 = Interval{ 2.0, 2.0 };
    Interval _18055 = _19066;
    Interval _19095 = imul(_18054, _18055, intervalFailed, optical_product_upper);
    Interval _18056 = _19095;
    Interval _18057 = _19072;
    Interval _19096 = jet_mul_derivative(_18056, _18057, intervalFailed, optical_product_upper);
    bool _19101;
    if (_19091 <= 0.0)
    {
        _19101 = _19092 >= 0.0;
    }
    else
    {
        _19101 = false;
    }
    float _19108;
    if (_19101)
    {
        _19108 = 0.0;
    }
    else
    {
        _19108 = precise::min(abs(_19091), abs(_19092));
    }
    float _19111 = precise::max(abs(_19091), abs(_19092));
    float _18038 = spvFMul(_19108, _19108);
    float _19112 = interval_down(_18038, intervalFailed);
    float _18039 = spvFMul(_19111, _19111);
    float _19114 = interval_up(_18039, intervalFailed);
    Interval _18040 = Interval{ 2.0, 2.0 };
    Interval _18041 = Interval{ _19091, _19092 };
    Interval _19116 = imul(_18040, _18041, intervalFailed, optical_product_upper);
    Interval _18042 = _19116;
    Interval _18043 = _19094;
    Interval _19117 = jet_mul_derivative(_18042, _18043, intervalFailed, optical_product_upper);
    Interval _18044 = Interval{ 2.0, 2.0 };
    Interval _18045 = Interval{ _19091, _19092 };
    Interval _19119 = imul(_18044, _18045, intervalFailed, optical_product_upper);
    Interval _18046 = _19119;
    Interval _18047 = _19096;
    Interval _19120 = jet_mul_derivative(_18046, _18047, intervalFailed, optical_product_upper);
    Interval _18032 = Interval{ 1.0, 1.0 };
    Interval _18033 = Interval{ precise::max(0.0, _19112), _19114 };
    Interval _19122 = iadd(_18032, _18033, intervalFailed);
    Interval _18034 = Interval{ 0.0, 0.0 };
    Interval _18035 = _19117;
    Interval _19123 = jet_add_derivative(_18034, _18035, intervalFailed);
    Interval _18036 = Interval{ 0.0, 0.0 };
    Interval _18037 = _19120;
    Interval _19124 = jet_add_derivative(_18036, _18037, intervalFailed);
    Interval _18018 = Interval{ 1.0, 1.0 };
    Interval _18019 = _19122;
    Interval _19126 = idiv(_18018, _18019, intervalFailed, interval_divide_upper);
    bool _19133;
    if (!intervalFailed)
    {
        _19133 = intervalFailed;
    }
    else
    {
        _19133 = false;
    }
    bool _19138;
    if (_19133)
    {
        _19138 = jetFailureSite == 0u;
    }
    else
    {
        _19138 = false;
    }
    if (_19138)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19122.lo, _19122.hi);
    }
    Interval _18020 = Interval{ 0.0, 0.0 };
    Interval _18021 = _19126;
    Interval _18022 = _19123;
    Interval _19142 = jet_mul_derivative(_18021, _18022, intervalFailed, optical_product_upper);
    Interval _18023 = Interval{ as_type<float>(as_type<uint>(_19142.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19142.lo) ^ 2147483648u) };
    Interval _19152 = jet_add_derivative(_18020, _18023, intervalFailed);
    Interval _18024 = _19152;
    Interval _18025 = _19122;
    Interval _19153 = jet_div_derivative(_18024, _18025, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18026 = Interval{ 0.0, 0.0 };
    Interval _18027 = _19126;
    Interval _18028 = _19124;
    Interval _19154 = jet_mul_derivative(_18027, _18028, intervalFailed, optical_product_upper);
    Interval _18029 = Interval{ as_type<float>(as_type<uint>(_19154.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19154.lo) ^ 2147483648u) };
    Interval _19164 = jet_add_derivative(_18026, _18029, intervalFailed);
    Interval _18030 = _19164;
    Interval _18031 = _19122;
    Interval _19165 = jet_div_derivative(_18030, _18031, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18016 = 54.0;
    float _18017 = 1000.0;
    Interval _19168 = iratio(_18016, _18017, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19173;
    if (!intervalFailed)
    {
        _19173 = intervalFailed;
    }
    else
    {
        _19173 = false;
    }
    bool _19178;
    if (_19173)
    {
        _19178 = jetFailureSite == 0u;
    }
    else
    {
        _19178 = false;
    }
    if (_19178)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
    }
    Interval _18002 = footprint.v;
    Interval _18003 = _19168;
    Interval _19184 = imul(_18002, _18003, intervalFailed, optical_product_upper);
    Interval _18004 = footprint.dx;
    Interval _18005 = _19168;
    Interval _19185 = jet_mul_derivative(_18004, _18005, intervalFailed, optical_product_upper);
    Interval _18006 = _19185;
    Interval _18007 = footprint.v;
    Interval _18008 = Interval{ 0.0, 0.0 };
    Interval _19186 = jet_mul_derivative(_18007, _18008, intervalFailed, optical_product_upper);
    Interval _18009 = _19186;
    Interval _19187 = jet_add_derivative(_18006, _18009, intervalFailed);
    Interval _18010 = footprint.dy;
    Interval _18011 = _19168;
    Interval _19188 = jet_mul_derivative(_18010, _18011, intervalFailed, optical_product_upper);
    Interval _18012 = _19188;
    Interval _18013 = footprint.v;
    Interval _18014 = Interval{ 0.0, 0.0 };
    Interval _19189 = jet_mul_derivative(_18013, _18014, intervalFailed, optical_product_upper);
    Interval _18015 = _19189;
    Interval _19190 = jet_add_derivative(_18012, _18015, intervalFailed);
    bool _19197;
    if (_19184.lo <= 0.0)
    {
        _19197 = _19184.hi >= 0.0;
    }
    else
    {
        _19197 = false;
    }
    float _19204;
    if (_19197)
    {
        _19204 = 0.0;
    }
    else
    {
        _19204 = precise::min(abs(_19184.lo), abs(_19184.hi));
    }
    float _19207 = precise::max(abs(_19184.lo), abs(_19184.hi));
    float _17992 = spvFMul(_19204, _19204);
    float _19208 = interval_down(_17992, intervalFailed);
    float _19209 = precise::max(0.0, _19208);
    float _17993 = spvFMul(_19207, _19207);
    float _19210 = interval_up(_17993, intervalFailed);
    Interval _17994 = Interval{ 2.0, 2.0 };
    Interval _17995 = _19184;
    Interval _19211 = imul(_17994, _17995, intervalFailed, optical_product_upper);
    Interval _17996 = _19211;
    Interval _17997 = _19187;
    Interval _19212 = jet_mul_derivative(_17996, _17997, intervalFailed, optical_product_upper);
    Interval _17998 = Interval{ 2.0, 2.0 };
    Interval _17999 = _19184;
    Interval _19213 = imul(_17998, _17999, intervalFailed, optical_product_upper);
    Interval _18000 = _19213;
    Interval _18001 = _19190;
    Interval _19214 = jet_mul_derivative(_18000, _18001, intervalFailed, optical_product_upper);
    bool _19219;
    if (_19209 <= 0.0)
    {
        _19219 = _19210 >= 0.0;
    }
    else
    {
        _19219 = false;
    }
    float _19226;
    if (_19219)
    {
        _19226 = 0.0;
    }
    else
    {
        _19226 = precise::min(abs(_19209), abs(_19210));
    }
    float _19229 = precise::max(abs(_19209), abs(_19210));
    float _17982 = spvFMul(_19226, _19226);
    float _19230 = interval_down(_17982, intervalFailed);
    float _17983 = spvFMul(_19229, _19229);
    float _19232 = interval_up(_17983, intervalFailed);
    Interval _17984 = Interval{ 2.0, 2.0 };
    Interval _17985 = Interval{ _19209, _19210 };
    Interval _19234 = imul(_17984, _17985, intervalFailed, optical_product_upper);
    Interval _17986 = _19234;
    Interval _17987 = _19212;
    Interval _19235 = jet_mul_derivative(_17986, _17987, intervalFailed, optical_product_upper);
    Interval _17988 = Interval{ 2.0, 2.0 };
    Interval _17989 = Interval{ _19209, _19210 };
    Interval _19237 = imul(_17988, _17989, intervalFailed, optical_product_upper);
    Interval _17990 = _19237;
    Interval _17991 = _19214;
    Interval _19238 = jet_mul_derivative(_17990, _17991, intervalFailed, optical_product_upper);
    Interval _17976 = Interval{ 1.0, 1.0 };
    Interval _17977 = Interval{ precise::max(0.0, _19230), _19232 };
    Interval _19240 = iadd(_17976, _17977, intervalFailed);
    Interval _17978 = Interval{ 0.0, 0.0 };
    Interval _17979 = _19235;
    Interval _19241 = jet_add_derivative(_17978, _17979, intervalFailed);
    Interval _17980 = Interval{ 0.0, 0.0 };
    Interval _17981 = _19238;
    Interval _19242 = jet_add_derivative(_17980, _17981, intervalFailed);
    Interval _17962 = Interval{ 1.0, 1.0 };
    Interval _17963 = _19240;
    Interval _19244 = idiv(_17962, _17963, intervalFailed, interval_divide_upper);
    bool _19251;
    if (!intervalFailed)
    {
        _19251 = intervalFailed;
    }
    else
    {
        _19251 = false;
    }
    bool _19256;
    if (_19251)
    {
        _19256 = jetFailureSite == 0u;
    }
    else
    {
        _19256 = false;
    }
    if (_19256)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19240.lo, _19240.hi);
    }
    Interval _17964 = Interval{ 0.0, 0.0 };
    Interval _17965 = _19244;
    Interval _17966 = _19241;
    Interval _19260 = jet_mul_derivative(_17965, _17966, intervalFailed, optical_product_upper);
    Interval _17967 = Interval{ as_type<float>(as_type<uint>(_19260.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19260.lo) ^ 2147483648u) };
    Interval _19270 = jet_add_derivative(_17964, _17967, intervalFailed);
    Interval _17968 = _19270;
    Interval _17969 = _19240;
    Interval _19271 = jet_div_derivative(_17968, _17969, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17970 = Interval{ 0.0, 0.0 };
    Interval _17971 = _19244;
    Interval _17972 = _19242;
    Interval _19272 = jet_mul_derivative(_17971, _17972, intervalFailed, optical_product_upper);
    Interval _17973 = Interval{ as_type<float>(as_type<uint>(_19272.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19272.lo) ^ 2147483648u) };
    Interval _19282 = jet_add_derivative(_17970, _17973, intervalFailed);
    Interval _17974 = _19282;
    Interval _17975 = _19240;
    Interval _19283 = jet_div_derivative(_17974, _17975, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _17960 = 24.0;
    float _17961 = 1000.0;
    Interval _19286 = iratio(_17960, _17961, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19291;
    if (!intervalFailed)
    {
        _19291 = intervalFailed;
    }
    else
    {
        _19291 = false;
    }
    bool _19296;
    if (_19291)
    {
        _19296 = jetFailureSite == 0u;
    }
    else
    {
        _19296 = false;
    }
    if (_19296)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(24.0, 24.0, 1000.0, 1000.0);
    }
    Interval _17946 = footprint.v;
    Interval _17947 = _19286;
    Interval _19302 = imul(_17946, _17947, intervalFailed, optical_product_upper);
    Interval _17948 = footprint.dx;
    Interval _17949 = _19286;
    Interval _19303 = jet_mul_derivative(_17948, _17949, intervalFailed, optical_product_upper);
    Interval _17950 = _19303;
    Interval _17951 = footprint.v;
    Interval _17952 = Interval{ 0.0, 0.0 };
    Interval _19304 = jet_mul_derivative(_17951, _17952, intervalFailed, optical_product_upper);
    Interval _17953 = _19304;
    Interval _19305 = jet_add_derivative(_17950, _17953, intervalFailed);
    Interval _17954 = footprint.dy;
    Interval _17955 = _19286;
    Interval _19306 = jet_mul_derivative(_17954, _17955, intervalFailed, optical_product_upper);
    Interval _17956 = _19306;
    Interval _17957 = footprint.v;
    Interval _17958 = Interval{ 0.0, 0.0 };
    Interval _19307 = jet_mul_derivative(_17957, _17958, intervalFailed, optical_product_upper);
    Interval _17959 = _19307;
    Interval _19308 = jet_add_derivative(_17956, _17959, intervalFailed);
    bool _19315;
    if (_19302.lo <= 0.0)
    {
        _19315 = _19302.hi >= 0.0;
    }
    else
    {
        _19315 = false;
    }
    float _19322;
    if (_19315)
    {
        _19322 = 0.0;
    }
    else
    {
        _19322 = precise::min(abs(_19302.lo), abs(_19302.hi));
    }
    float _19325 = precise::max(abs(_19302.lo), abs(_19302.hi));
    float _17936 = spvFMul(_19322, _19322);
    float _19326 = interval_down(_17936, intervalFailed);
    float _19327 = precise::max(0.0, _19326);
    float _17937 = spvFMul(_19325, _19325);
    float _19328 = interval_up(_17937, intervalFailed);
    Interval _17938 = Interval{ 2.0, 2.0 };
    Interval _17939 = _19302;
    Interval _19329 = imul(_17938, _17939, intervalFailed, optical_product_upper);
    Interval _17940 = _19329;
    Interval _17941 = _19305;
    Interval _19330 = jet_mul_derivative(_17940, _17941, intervalFailed, optical_product_upper);
    Interval _17942 = Interval{ 2.0, 2.0 };
    Interval _17943 = _19302;
    Interval _19331 = imul(_17942, _17943, intervalFailed, optical_product_upper);
    Interval _17944 = _19331;
    Interval _17945 = _19308;
    Interval _19332 = jet_mul_derivative(_17944, _17945, intervalFailed, optical_product_upper);
    bool _19337;
    if (_19327 <= 0.0)
    {
        _19337 = _19328 >= 0.0;
    }
    else
    {
        _19337 = false;
    }
    float _19344;
    if (_19337)
    {
        _19344 = 0.0;
    }
    else
    {
        _19344 = precise::min(abs(_19327), abs(_19328));
    }
    float _19347 = precise::max(abs(_19327), abs(_19328));
    float _17926 = spvFMul(_19344, _19344);
    float _19348 = interval_down(_17926, intervalFailed);
    float _17927 = spvFMul(_19347, _19347);
    float _19350 = interval_up(_17927, intervalFailed);
    Interval _17928 = Interval{ 2.0, 2.0 };
    Interval _17929 = Interval{ _19327, _19328 };
    Interval _19352 = imul(_17928, _17929, intervalFailed, optical_product_upper);
    Interval _17930 = _19352;
    Interval _17931 = _19330;
    Interval _19353 = jet_mul_derivative(_17930, _17931, intervalFailed, optical_product_upper);
    Interval _17932 = Interval{ 2.0, 2.0 };
    Interval _17933 = Interval{ _19327, _19328 };
    Interval _19355 = imul(_17932, _17933, intervalFailed, optical_product_upper);
    Interval _17934 = _19355;
    Interval _17935 = _19332;
    Interval _19356 = jet_mul_derivative(_17934, _17935, intervalFailed, optical_product_upper);
    Interval _17920 = Interval{ 1.0, 1.0 };
    Interval _17921 = Interval{ precise::max(0.0, _19348), _19350 };
    Interval _19358 = iadd(_17920, _17921, intervalFailed);
    Interval _17922 = Interval{ 0.0, 0.0 };
    Interval _17923 = _19353;
    Interval _19359 = jet_add_derivative(_17922, _17923, intervalFailed);
    Interval _17924 = Interval{ 0.0, 0.0 };
    Interval _17925 = _19356;
    Interval _19360 = jet_add_derivative(_17924, _17925, intervalFailed);
    Interval _17906 = Interval{ 1.0, 1.0 };
    Interval _17907 = _19358;
    Interval _19362 = idiv(_17906, _17907, intervalFailed, interval_divide_upper);
    bool _19369;
    if (!intervalFailed)
    {
        _19369 = intervalFailed;
    }
    else
    {
        _19369 = false;
    }
    bool _19374;
    if (_19369)
    {
        _19374 = jetFailureSite == 0u;
    }
    else
    {
        _19374 = false;
    }
    if (_19374)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19358.lo, _19358.hi);
    }
    Interval _17908 = Interval{ 0.0, 0.0 };
    Interval _17909 = _19362;
    Interval _17910 = _19359;
    Interval _19378 = jet_mul_derivative(_17909, _17910, intervalFailed, optical_product_upper);
    Interval _17911 = Interval{ as_type<float>(as_type<uint>(_19378.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19378.lo) ^ 2147483648u) };
    Interval _19388 = jet_add_derivative(_17908, _17911, intervalFailed);
    Interval _17912 = _19388;
    Interval _17913 = _19358;
    Interval _19389 = jet_div_derivative(_17912, _17913, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17914 = Interval{ 0.0, 0.0 };
    Interval _17915 = _19362;
    Interval _17916 = _19360;
    Interval _19390 = jet_mul_derivative(_17915, _17916, intervalFailed, optical_product_upper);
    Interval _17917 = Interval{ as_type<float>(as_type<uint>(_19390.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19390.lo) ^ 2147483648u) };
    Interval _19400 = jet_add_derivative(_17914, _17917, intervalFailed);
    Interval _17918 = _19400;
    Interval _17919 = _19358;
    Interval _19401 = jet_div_derivative(_17918, _17919, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17898 = _18802;
    float _17896 = 3.1415927410125732421875;
    float _19402 = interval_down(_17896, intervalFailed);
    float _17897 = 3.1415927410125732421875;
    float _19403 = interval_up(_17897, intervalFailed);
    Interval _17899 = Interval{ _19402, _19403 };
    Interval _17900 = Interval{ 0.5, 0.5 };
    Interval _19405 = imul(_17899, _17900, intervalFailed, optical_product_upper);
    Interval _17901 = _19405;
    Interval _19406 = iadd(_17898, _17901, intervalFailed);
    float _17894 = _19406.lo;
    float _17895 = _19406.hi;
    float _19409 = sine_bounds(_17894, _17895, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17892 = _18802.lo;
    float _17893 = _18802.hi;
    float _19413 = sine_bounds(_17892, _17893, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17902 = Interval{ _19409, interval_sine_upper };
    Interval _17903 = _18803;
    Interval _19416 = jet_mul_derivative(_17902, _17903, intervalFailed, optical_product_upper);
    Interval _17904 = Interval{ _19409, interval_sine_upper };
    Interval _17905 = _18804;
    Interval _19418 = jet_mul_derivative(_17904, _17905, intervalFailed, optical_product_upper);
    Interval _17878 = Interval{ 9.0, 9.0 };
    Interval _17879 = Interval{ _19413, interval_sine_upper };
    Interval _19420 = imul(_17878, _17879, intervalFailed, optical_product_upper);
    Interval _17880 = Interval{ 0.0, 0.0 };
    Interval _17881 = Interval{ _19413, interval_sine_upper };
    Interval _19422 = jet_mul_derivative(_17880, _17881, intervalFailed, optical_product_upper);
    Interval _17882 = _19422;
    Interval _17883 = Interval{ 9.0, 9.0 };
    Interval _17884 = _19416;
    Interval _19423 = jet_mul_derivative(_17883, _17884, intervalFailed, optical_product_upper);
    Interval _17885 = _19423;
    Interval _19424 = jet_add_derivative(_17882, _17885, intervalFailed);
    Interval _17886 = Interval{ 0.0, 0.0 };
    Interval _17887 = Interval{ _19413, interval_sine_upper };
    Interval _19426 = jet_mul_derivative(_17886, _17887, intervalFailed, optical_product_upper);
    Interval _17888 = _19426;
    Interval _17889 = Interval{ 9.0, 9.0 };
    Interval _17890 = _19418;
    Interval _19427 = jet_mul_derivative(_17889, _17890, intervalFailed, optical_product_upper);
    Interval _17891 = _19427;
    Interval _19428 = jet_add_derivative(_17888, _17891, intervalFailed);
    Interval _17864 = _19420;
    Interval _17865 = _19126;
    Interval _19429 = imul(_17864, _17865, intervalFailed, optical_product_upper);
    Interval _17866 = _19424;
    Interval _17867 = _19126;
    Interval _19430 = jet_mul_derivative(_17866, _17867, intervalFailed, optical_product_upper);
    Interval _17868 = _19430;
    Interval _17869 = _19420;
    Interval _17870 = _19153;
    Interval _19431 = jet_mul_derivative(_17869, _17870, intervalFailed, optical_product_upper);
    Interval _17871 = _19431;
    Interval _19432 = jet_add_derivative(_17868, _17871, intervalFailed);
    Interval _17872 = _19428;
    Interval _17873 = _19126;
    Interval _19433 = jet_mul_derivative(_17872, _17873, intervalFailed, optical_product_upper);
    Interval _17874 = _19433;
    Interval _17875 = _19420;
    Interval _17876 = _19165;
    Interval _19434 = jet_mul_derivative(_17875, _17876, intervalFailed, optical_product_upper);
    Interval _17877 = _19434;
    Interval _19435 = jet_add_derivative(_17874, _17877, intervalFailed);
    Interval _17856 = _18910;
    float _17854 = 3.1415927410125732421875;
    float _19436 = interval_down(_17854, intervalFailed);
    float _17855 = 3.1415927410125732421875;
    float _19437 = interval_up(_17855, intervalFailed);
    Interval _17857 = Interval{ _19436, _19437 };
    Interval _17858 = Interval{ 0.5, 0.5 };
    Interval _19439 = imul(_17857, _17858, intervalFailed, optical_product_upper);
    Interval _17859 = _19439;
    Interval _19440 = iadd(_17856, _17859, intervalFailed);
    float _17852 = _19440.lo;
    float _17853 = _19440.hi;
    float _19443 = sine_bounds(_17852, _17853, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17850 = _18910.lo;
    float _17851 = _18910.hi;
    float _19447 = sine_bounds(_17850, _17851, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17860 = Interval{ _19443, interval_sine_upper };
    Interval _17861 = _18911;
    Interval _19450 = jet_mul_derivative(_17860, _17861, intervalFailed, optical_product_upper);
    Interval _17862 = Interval{ _19443, interval_sine_upper };
    Interval _17863 = _18912;
    Interval _19452 = jet_mul_derivative(_17862, _17863, intervalFailed, optical_product_upper);
    Interval _17836 = Interval{ 2.5, 2.5 };
    Interval _17837 = Interval{ _19447, interval_sine_upper };
    Interval _19454 = imul(_17836, _17837, intervalFailed, optical_product_upper);
    Interval _17838 = Interval{ 0.0, 0.0 };
    Interval _17839 = Interval{ _19447, interval_sine_upper };
    Interval _19456 = jet_mul_derivative(_17838, _17839, intervalFailed, optical_product_upper);
    Interval _17840 = _19456;
    Interval _17841 = Interval{ 2.5, 2.5 };
    Interval _17842 = _19450;
    Interval _19457 = jet_mul_derivative(_17841, _17842, intervalFailed, optical_product_upper);
    Interval _17843 = _19457;
    Interval _19458 = jet_add_derivative(_17840, _17843, intervalFailed);
    Interval _17844 = Interval{ 0.0, 0.0 };
    Interval _17845 = Interval{ _19447, interval_sine_upper };
    Interval _19460 = jet_mul_derivative(_17844, _17845, intervalFailed, optical_product_upper);
    Interval _17846 = _19460;
    Interval _17847 = Interval{ 2.5, 2.5 };
    Interval _17848 = _19452;
    Interval _19461 = jet_mul_derivative(_17847, _17848, intervalFailed, optical_product_upper);
    Interval _17849 = _19461;
    Interval _19462 = jet_add_derivative(_17846, _17849, intervalFailed);
    Interval _17822 = _19454;
    Interval _17823 = _19244;
    Interval _19463 = imul(_17822, _17823, intervalFailed, optical_product_upper);
    Interval _17824 = _19458;
    Interval _17825 = _19244;
    Interval _19464 = jet_mul_derivative(_17824, _17825, intervalFailed, optical_product_upper);
    Interval _17826 = _19464;
    Interval _17827 = _19454;
    Interval _17828 = _19271;
    Interval _19465 = jet_mul_derivative(_17827, _17828, intervalFailed, optical_product_upper);
    Interval _17829 = _19465;
    Interval _19466 = jet_add_derivative(_17826, _17829, intervalFailed);
    Interval _17830 = _19462;
    Interval _17831 = _19244;
    Interval _19467 = jet_mul_derivative(_17830, _17831, intervalFailed, optical_product_upper);
    Interval _17832 = _19467;
    Interval _17833 = _19454;
    Interval _17834 = _19283;
    Interval _19468 = jet_mul_derivative(_17833, _17834, intervalFailed, optical_product_upper);
    Interval _17835 = _19468;
    Interval _19469 = jet_add_derivative(_17832, _17835, intervalFailed);
    Interval _17816 = _19429;
    Interval _17817 = _19463;
    Interval _19470 = iadd(_17816, _17817, intervalFailed);
    Interval _17818 = _19432;
    Interval _17819 = _19466;
    Interval _19471 = jet_add_derivative(_17818, _17819, intervalFailed);
    Interval _17820 = _19435;
    Interval _17821 = _19469;
    Interval _19472 = jet_add_derivative(_17820, _17821, intervalFailed);
    Interval _17808 = _19043;
    float _17806 = 3.1415927410125732421875;
    float _19473 = interval_down(_17806, intervalFailed);
    float _17807 = 3.1415927410125732421875;
    float _19474 = interval_up(_17807, intervalFailed);
    Interval _17809 = Interval{ _19473, _19474 };
    Interval _17810 = Interval{ 0.5, 0.5 };
    Interval _19476 = imul(_17809, _17810, intervalFailed, optical_product_upper);
    Interval _17811 = _19476;
    Interval _19477 = iadd(_17808, _17811, intervalFailed);
    float _17804 = _19477.lo;
    float _17805 = _19477.hi;
    float _19480 = sine_bounds(_17804, _17805, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17802 = _19043.lo;
    float _17803 = _19043.hi;
    float _19484 = sine_bounds(_17802, _17803, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17812 = Interval{ _19480, interval_sine_upper };
    Interval _17813 = _19045;
    Interval _19487 = jet_mul_derivative(_17812, _17813, intervalFailed, optical_product_upper);
    Interval _17814 = Interval{ _19480, interval_sine_upper };
    Interval _17815 = _19047;
    Interval _19489 = jet_mul_derivative(_17814, _17815, intervalFailed, optical_product_upper);
    Interval _17788 = Interval{ 5.0, 5.0 };
    Interval _17789 = Interval{ _19484, interval_sine_upper };
    Interval _19491 = imul(_17788, _17789, intervalFailed, optical_product_upper);
    Interval _17790 = Interval{ 0.0, 0.0 };
    Interval _17791 = Interval{ _19484, interval_sine_upper };
    Interval _19493 = jet_mul_derivative(_17790, _17791, intervalFailed, optical_product_upper);
    Interval _17792 = _19493;
    Interval _17793 = Interval{ 5.0, 5.0 };
    Interval _17794 = _19487;
    Interval _19494 = jet_mul_derivative(_17793, _17794, intervalFailed, optical_product_upper);
    Interval _17795 = _19494;
    Interval _19495 = jet_add_derivative(_17792, _17795, intervalFailed);
    Interval _17796 = Interval{ 0.0, 0.0 };
    Interval _17797 = Interval{ _19484, interval_sine_upper };
    Interval _19497 = jet_mul_derivative(_17796, _17797, intervalFailed, optical_product_upper);
    Interval _17798 = _19497;
    Interval _17799 = Interval{ 5.0, 5.0 };
    Interval _17800 = _19489;
    Interval _19498 = jet_mul_derivative(_17799, _17800, intervalFailed, optical_product_upper);
    Interval _17801 = _19498;
    Interval _19499 = jet_add_derivative(_17798, _17801, intervalFailed);
    Interval _17774 = _19491;
    Interval _17775 = _19362;
    Interval _19500 = imul(_17774, _17775, intervalFailed, optical_product_upper);
    Interval _17776 = _19495;
    Interval _17777 = _19362;
    Interval _19501 = jet_mul_derivative(_17776, _17777, intervalFailed, optical_product_upper);
    Interval _17778 = _19501;
    Interval _17779 = _19491;
    Interval _17780 = _19389;
    Interval _19502 = jet_mul_derivative(_17779, _17780, intervalFailed, optical_product_upper);
    Interval _17781 = _19502;
    Interval _19503 = jet_add_derivative(_17778, _17781, intervalFailed);
    Interval _17782 = _19499;
    Interval _17783 = _19362;
    Interval _19504 = jet_mul_derivative(_17782, _17783, intervalFailed, optical_product_upper);
    Interval _17784 = _19504;
    Interval _17785 = _19491;
    Interval _17786 = _19401;
    Interval _19505 = jet_mul_derivative(_17785, _17786, intervalFailed, optical_product_upper);
    Interval _17787 = _19505;
    Interval _19506 = jet_add_derivative(_17784, _17787, intervalFailed);
    Interval _17768 = _19470;
    Interval _17769 = _19500;
    Interval _19507 = iadd(_17768, _17769, intervalFailed);
    Interval _17770 = _19471;
    Interval _17771 = _19503;
    Interval _19508 = jet_add_derivative(_17770, _17771, intervalFailed);
    Interval _17772 = _19472;
    Interval _17773 = _19506;
    Interval _19509 = jet_add_derivative(_17772, _17773, intervalFailed);
    float _17762 = _18525.lo;
    float _17763 = _18525.hi;
    float _19512 = sine_bounds(_17762, _17763, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19516 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19519 = as_type<float>(as_type<uint>(_19512) ^ 2147483648u);
    Interval _17758 = _18525;
    float _17756 = 3.1415927410125732421875;
    float _19520 = interval_down(_17756, intervalFailed);
    float _17757 = 3.1415927410125732421875;
    float _19521 = interval_up(_17757, intervalFailed);
    Interval _17759 = Interval{ _19520, _19521 };
    Interval _17760 = Interval{ 0.5, 0.5 };
    Interval _19523 = imul(_17759, _17760, intervalFailed, optical_product_upper);
    Interval _17761 = _19523;
    Interval _19524 = iadd(_17758, _17761, intervalFailed);
    float _17754 = _19524.lo;
    float _17755 = _19524.hi;
    float _19527 = sine_bounds(_17754, _17755, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17764 = Interval{ _19516, _19519 };
    Interval _17765 = _18526;
    Interval _19530 = jet_mul_derivative(_17764, _17765, intervalFailed, optical_product_upper);
    Interval _17766 = Interval{ _19516, _19519 };
    Interval _17767 = _18527;
    Interval _19532 = jet_mul_derivative(_17766, _17767, intervalFailed, optical_product_upper);
    Interval _17740 = Interval{ _19527, interval_sine_upper };
    Interval _17741 = _18606;
    Interval _19534 = imul(_17740, _17741, intervalFailed, optical_product_upper);
    Interval _17742 = _19530;
    Interval _17743 = _18606;
    Interval _19535 = jet_mul_derivative(_17742, _17743, intervalFailed, optical_product_upper);
    Interval _17744 = _19535;
    Interval _17745 = Interval{ _19527, interval_sine_upper };
    Interval _17746 = _18633;
    Interval _19537 = jet_mul_derivative(_17745, _17746, intervalFailed, optical_product_upper);
    Interval _17747 = _19537;
    Interval _19538 = jet_add_derivative(_17744, _17747, intervalFailed);
    Interval _17748 = _19532;
    Interval _17749 = _18606;
    Interval _19539 = jet_mul_derivative(_17748, _17749, intervalFailed, optical_product_upper);
    Interval _17750 = _19539;
    Interval _17751 = Interval{ _19527, interval_sine_upper };
    Interval _17752 = _18645;
    Interval _19541 = jet_mul_derivative(_17751, _17752, intervalFailed, optical_product_upper);
    Interval _17753 = _19541;
    Interval _19542 = jet_add_derivative(_17750, _17753, intervalFailed);
    float _17738 = 18.0;
    float _17739 = 1000.0;
    Interval _19544 = iratio(_17738, _17739, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19549;
    if (!intervalFailed)
    {
        _19549 = intervalFailed;
    }
    else
    {
        _19549 = false;
    }
    bool _19554;
    if (_19549)
    {
        _19554 = jetFailureSite == 0u;
    }
    else
    {
        _19554 = false;
    }
    if (_19554)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
    }
    float _17736 = 39.0;
    float _17737 = 10000.0;
    Interval _19558 = iratio(_17736, _17737, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19563;
    if (!intervalFailed)
    {
        _19563 = intervalFailed;
    }
    else
    {
        _19563 = false;
    }
    bool _19568;
    if (_19563)
    {
        _19568 = jetFailureSite == 0u;
    }
    else
    {
        _19568 = false;
    }
    if (_19568)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(39.0, 39.0, 10000.0, 10000.0);
    }
    Interval _17722 = _19558;
    Interval _17723 = _19534;
    Interval _19571 = imul(_17722, _17723, intervalFailed, optical_product_upper);
    Interval _17724 = Interval{ 0.0, 0.0 };
    Interval _17725 = _19534;
    Interval _19572 = jet_mul_derivative(_17724, _17725, intervalFailed, optical_product_upper);
    Interval _17726 = _19572;
    Interval _17727 = _19558;
    Interval _17728 = _19538;
    Interval _19573 = jet_mul_derivative(_17727, _17728, intervalFailed, optical_product_upper);
    Interval _17729 = _19573;
    Interval _19574 = jet_add_derivative(_17726, _17729, intervalFailed);
    Interval _17730 = Interval{ 0.0, 0.0 };
    Interval _17731 = _19534;
    Interval _19575 = jet_mul_derivative(_17730, _17731, intervalFailed, optical_product_upper);
    Interval _17732 = _19575;
    Interval _17733 = _19558;
    Interval _17734 = _19542;
    Interval _19576 = jet_mul_derivative(_17733, _17734, intervalFailed, optical_product_upper);
    Interval _17735 = _19576;
    Interval _19577 = jet_add_derivative(_17732, _17735, intervalFailed);
    Interval _17716 = _19544;
    Interval _17717 = _19571;
    Interval _19578 = iadd(_17716, _17717, intervalFailed);
    Interval _17718 = Interval{ 0.0, 0.0 };
    Interval _17719 = _19574;
    Interval _19579 = jet_add_derivative(_17718, _17719, intervalFailed);
    Interval _17720 = Interval{ 0.0, 0.0 };
    Interval _17721 = _19577;
    Interval _19580 = jet_add_derivative(_17720, _17721, intervalFailed);
    Interval _17702 = Interval{ 9.0, 9.0 };
    Interval _17703 = _19578;
    Interval _19581 = imul(_17702, _17703, intervalFailed, optical_product_upper);
    Interval _17704 = Interval{ 0.0, 0.0 };
    Interval _17705 = _19578;
    Interval _19582 = jet_mul_derivative(_17704, _17705, intervalFailed, optical_product_upper);
    Interval _17706 = _19582;
    Interval _17707 = Interval{ 9.0, 9.0 };
    Interval _17708 = _19579;
    Interval _19583 = jet_mul_derivative(_17707, _17708, intervalFailed, optical_product_upper);
    Interval _17709 = _19583;
    Interval _19584 = jet_add_derivative(_17706, _17709, intervalFailed);
    Interval _17710 = Interval{ 0.0, 0.0 };
    Interval _17711 = _19578;
    Interval _19585 = jet_mul_derivative(_17710, _17711, intervalFailed, optical_product_upper);
    Interval _17712 = _19585;
    Interval _17713 = Interval{ 9.0, 9.0 };
    Interval _17714 = _19580;
    Interval _19586 = jet_mul_derivative(_17713, _17714, intervalFailed, optical_product_upper);
    Interval _17715 = _19586;
    Interval _19587 = jet_add_derivative(_17712, _17715, intervalFailed);
    float _17696 = _18802.lo;
    float _17697 = _18802.hi;
    float _19590 = sine_bounds(_17696, _17697, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19594 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19597 = as_type<float>(as_type<uint>(_19590) ^ 2147483648u);
    Interval _17692 = _18802;
    float _17690 = 3.1415927410125732421875;
    float _19598 = interval_down(_17690, intervalFailed);
    float _17691 = 3.1415927410125732421875;
    float _19599 = interval_up(_17691, intervalFailed);
    Interval _17693 = Interval{ _19598, _19599 };
    Interval _17694 = Interval{ 0.5, 0.5 };
    Interval _19601 = imul(_17693, _17694, intervalFailed, optical_product_upper);
    Interval _17695 = _19601;
    Interval _19602 = iadd(_17692, _17695, intervalFailed);
    float _17688 = _19602.lo;
    float _17689 = _19602.hi;
    float _19605 = sine_bounds(_17688, _17689, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17698 = Interval{ _19594, _19597 };
    Interval _17699 = _18803;
    Interval _19608 = jet_mul_derivative(_17698, _17699, intervalFailed, optical_product_upper);
    Interval _17700 = Interval{ _19594, _19597 };
    Interval _17701 = _18804;
    Interval _19610 = jet_mul_derivative(_17700, _17701, intervalFailed, optical_product_upper);
    Interval _17674 = _19581;
    Interval _17675 = Interval{ _19605, interval_sine_upper };
    Interval _19612 = imul(_17674, _17675, intervalFailed, optical_product_upper);
    Interval _17676 = _19584;
    Interval _17677 = Interval{ _19605, interval_sine_upper };
    Interval _19614 = jet_mul_derivative(_17676, _17677, intervalFailed, optical_product_upper);
    Interval _17678 = _19614;
    Interval _17679 = _19581;
    Interval _17680 = _19608;
    Interval _19615 = jet_mul_derivative(_17679, _17680, intervalFailed, optical_product_upper);
    Interval _17681 = _19615;
    Interval _19616 = jet_add_derivative(_17678, _17681, intervalFailed);
    Interval _17682 = _19587;
    Interval _17683 = Interval{ _19605, interval_sine_upper };
    Interval _19618 = jet_mul_derivative(_17682, _17683, intervalFailed, optical_product_upper);
    Interval _17684 = _19618;
    Interval _17685 = _19581;
    Interval _17686 = _19610;
    Interval _19619 = jet_mul_derivative(_17685, _17686, intervalFailed, optical_product_upper);
    Interval _17687 = _19619;
    Interval _19620 = jet_add_derivative(_17684, _17687, intervalFailed);
    Interval _17660 = _19612;
    Interval _17661 = _19126;
    Interval _19621 = imul(_17660, _17661, intervalFailed, optical_product_upper);
    Interval _17662 = _19616;
    Interval _17663 = _19126;
    Interval _19622 = jet_mul_derivative(_17662, _17663, intervalFailed, optical_product_upper);
    Interval _17664 = _19622;
    Interval _17665 = _19612;
    Interval _17666 = _19153;
    Interval _19623 = jet_mul_derivative(_17665, _17666, intervalFailed, optical_product_upper);
    Interval _17667 = _19623;
    Interval _19624 = jet_add_derivative(_17664, _17667, intervalFailed);
    Interval _17668 = _19620;
    Interval _17669 = _19126;
    Interval _19625 = jet_mul_derivative(_17668, _17669, intervalFailed, optical_product_upper);
    Interval _17670 = _19625;
    Interval _17671 = _19612;
    Interval _17672 = _19165;
    Interval _19626 = jet_mul_derivative(_17671, _17672, intervalFailed, optical_product_upper);
    Interval _17673 = _19626;
    Interval _19627 = jet_add_derivative(_17670, _17673, intervalFailed);
    float _17658 = 1175.0;
    float _17659 = 10000.0;
    Interval _19629 = iratio(_17658, _17659, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19634;
    if (!intervalFailed)
    {
        _19634 = intervalFailed;
    }
    else
    {
        _19634 = false;
    }
    bool _19639;
    if (_19634)
    {
        _19639 = jetFailureSite == 0u;
    }
    else
    {
        _19639 = false;
    }
    if (_19639)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(1175.0, 1175.0, 10000.0, 10000.0);
    }
    float _17652 = _18910.lo;
    float _17653 = _18910.hi;
    float _19644 = sine_bounds(_17652, _17653, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19648 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19651 = as_type<float>(as_type<uint>(_19644) ^ 2147483648u);
    Interval _17648 = _18910;
    float _17646 = 3.1415927410125732421875;
    float _19652 = interval_down(_17646, intervalFailed);
    float _17647 = 3.1415927410125732421875;
    float _19653 = interval_up(_17647, intervalFailed);
    Interval _17649 = Interval{ _19652, _19653 };
    Interval _17650 = Interval{ 0.5, 0.5 };
    Interval _19655 = imul(_17649, _17650, intervalFailed, optical_product_upper);
    Interval _17651 = _19655;
    Interval _19656 = iadd(_17648, _17651, intervalFailed);
    float _17644 = _19656.lo;
    float _17645 = _19656.hi;
    float _19659 = sine_bounds(_17644, _17645, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17654 = Interval{ _19648, _19651 };
    Interval _17655 = _18911;
    Interval _19662 = jet_mul_derivative(_17654, _17655, intervalFailed, optical_product_upper);
    Interval _17656 = Interval{ _19648, _19651 };
    Interval _17657 = _18912;
    Interval _19664 = jet_mul_derivative(_17656, _17657, intervalFailed, optical_product_upper);
    Interval _17630 = _19629;
    Interval _17631 = Interval{ _19659, interval_sine_upper };
    Interval _19666 = imul(_17630, _17631, intervalFailed, optical_product_upper);
    Interval _17632 = Interval{ 0.0, 0.0 };
    Interval _17633 = Interval{ _19659, interval_sine_upper };
    Interval _19668 = jet_mul_derivative(_17632, _17633, intervalFailed, optical_product_upper);
    Interval _17634 = _19668;
    Interval _17635 = _19629;
    Interval _17636 = _19662;
    Interval _19669 = jet_mul_derivative(_17635, _17636, intervalFailed, optical_product_upper);
    Interval _17637 = _19669;
    Interval _19670 = jet_add_derivative(_17634, _17637, intervalFailed);
    Interval _17638 = Interval{ 0.0, 0.0 };
    Interval _17639 = Interval{ _19659, interval_sine_upper };
    Interval _19672 = jet_mul_derivative(_17638, _17639, intervalFailed, optical_product_upper);
    Interval _17640 = _19672;
    Interval _17641 = _19629;
    Interval _17642 = _19664;
    Interval _19673 = jet_mul_derivative(_17641, _17642, intervalFailed, optical_product_upper);
    Interval _17643 = _19673;
    Interval _19674 = jet_add_derivative(_17640, _17643, intervalFailed);
    Interval _17616 = _19666;
    Interval _17617 = _19244;
    Interval _19675 = imul(_17616, _17617, intervalFailed, optical_product_upper);
    Interval _17618 = _19670;
    Interval _17619 = _19244;
    Interval _19676 = jet_mul_derivative(_17618, _17619, intervalFailed, optical_product_upper);
    Interval _17620 = _19676;
    Interval _17621 = _19666;
    Interval _17622 = _19271;
    Interval _19677 = jet_mul_derivative(_17621, _17622, intervalFailed, optical_product_upper);
    Interval _17623 = _19677;
    Interval _19678 = jet_add_derivative(_17620, _17623, intervalFailed);
    Interval _17624 = _19674;
    Interval _17625 = _19244;
    Interval _19679 = jet_mul_derivative(_17624, _17625, intervalFailed, optical_product_upper);
    Interval _17626 = _19679;
    Interval _17627 = _19666;
    Interval _17628 = _19283;
    Interval _19680 = jet_mul_derivative(_17627, _17628, intervalFailed, optical_product_upper);
    Interval _17629 = _19680;
    Interval _19681 = jet_add_derivative(_17626, _17629, intervalFailed);
    Interval _17610 = _19621;
    Interval _17611 = _19675;
    Interval _19682 = iadd(_17610, _17611, intervalFailed);
    Interval _17612 = _19624;
    Interval _17613 = _19678;
    Interval _19683 = jet_add_derivative(_17612, _17613, intervalFailed);
    Interval _17614 = _19627;
    Interval _17615 = _19681;
    Interval _19684 = jet_add_derivative(_17614, _17615, intervalFailed);
    float _17608 = 45.0;
    float _17609 = 1000.0;
    Interval _19686 = iratio(_17608, _17609, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19691;
    if (!intervalFailed)
    {
        _19691 = intervalFailed;
    }
    else
    {
        _19691 = false;
    }
    bool _19696;
    if (_19691)
    {
        _19696 = jetFailureSite == 0u;
    }
    else
    {
        _19696 = false;
    }
    if (_19696)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(45.0, 45.0, 1000.0, 1000.0);
    }
    float _17602 = _19043.lo;
    float _17603 = _19043.hi;
    float _19701 = sine_bounds(_17602, _17603, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19705 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19708 = as_type<float>(as_type<uint>(_19701) ^ 2147483648u);
    Interval _17598 = _19043;
    float _17596 = 3.1415927410125732421875;
    float _19709 = interval_down(_17596, intervalFailed);
    float _17597 = 3.1415927410125732421875;
    float _19710 = interval_up(_17597, intervalFailed);
    Interval _17599 = Interval{ _19709, _19710 };
    Interval _17600 = Interval{ 0.5, 0.5 };
    Interval _19712 = imul(_17599, _17600, intervalFailed, optical_product_upper);
    Interval _17601 = _19712;
    Interval _19713 = iadd(_17598, _17601, intervalFailed);
    float _17594 = _19713.lo;
    float _17595 = _19713.hi;
    float _19716 = sine_bounds(_17594, _17595, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17604 = Interval{ _19705, _19708 };
    Interval _17605 = _19045;
    Interval _19719 = jet_mul_derivative(_17604, _17605, intervalFailed, optical_product_upper);
    Interval _17606 = Interval{ _19705, _19708 };
    Interval _17607 = _19047;
    Interval _19721 = jet_mul_derivative(_17606, _17607, intervalFailed, optical_product_upper);
    Interval _17580 = _19686;
    Interval _17581 = Interval{ _19716, interval_sine_upper };
    Interval _19723 = imul(_17580, _17581, intervalFailed, optical_product_upper);
    Interval _17582 = Interval{ 0.0, 0.0 };
    Interval _17583 = Interval{ _19716, interval_sine_upper };
    Interval _19725 = jet_mul_derivative(_17582, _17583, intervalFailed, optical_product_upper);
    Interval _17584 = _19725;
    Interval _17585 = _19686;
    Interval _17586 = _19719;
    Interval _19726 = jet_mul_derivative(_17585, _17586, intervalFailed, optical_product_upper);
    Interval _17587 = _19726;
    Interval _19727 = jet_add_derivative(_17584, _17587, intervalFailed);
    Interval _17588 = Interval{ 0.0, 0.0 };
    Interval _17589 = Interval{ _19716, interval_sine_upper };
    Interval _19729 = jet_mul_derivative(_17588, _17589, intervalFailed, optical_product_upper);
    Interval _17590 = _19729;
    Interval _17591 = _19686;
    Interval _17592 = _19721;
    Interval _19730 = jet_mul_derivative(_17591, _17592, intervalFailed, optical_product_upper);
    Interval _17593 = _19730;
    Interval _19731 = jet_add_derivative(_17590, _17593, intervalFailed);
    Interval _17566 = _19723;
    Interval _17567 = _19362;
    Interval _19732 = imul(_17566, _17567, intervalFailed, optical_product_upper);
    Interval _17568 = _19727;
    Interval _17569 = _19362;
    Interval _19733 = jet_mul_derivative(_17568, _17569, intervalFailed, optical_product_upper);
    Interval _17570 = _19733;
    Interval _17571 = _19723;
    Interval _17572 = _19389;
    Interval _19734 = jet_mul_derivative(_17571, _17572, intervalFailed, optical_product_upper);
    Interval _17573 = _19734;
    Interval _19735 = jet_add_derivative(_17570, _17573, intervalFailed);
    Interval _17574 = _19731;
    Interval _17575 = _19362;
    Interval _19736 = jet_mul_derivative(_17574, _17575, intervalFailed, optical_product_upper);
    Interval _17576 = _19736;
    Interval _17577 = _19723;
    Interval _17578 = _19401;
    Interval _19737 = jet_mul_derivative(_17577, _17578, intervalFailed, optical_product_upper);
    Interval _17579 = _19737;
    Interval _19738 = jet_add_derivative(_17576, _17579, intervalFailed);
    Interval _17560 = _19682;
    Interval _17561 = Interval{ as_type<float>(as_type<uint>(_19732.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19732.lo) ^ 2147483648u) };
    Interval _19764 = iadd(_17560, _17561, intervalFailed);
    Interval _17562 = _19683;
    Interval _17563 = Interval{ as_type<float>(as_type<uint>(_19735.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19735.lo) ^ 2147483648u) };
    Interval _19766 = jet_add_derivative(_17562, _17563, intervalFailed);
    Interval _17564 = _19684;
    Interval _17565 = Interval{ as_type<float>(as_type<uint>(_19738.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19738.lo) ^ 2147483648u) };
    Interval _19768 = jet_add_derivative(_17564, _17565, intervalFailed);
    float _17558 = 11.0;
    float _17559 = 1000.0;
    Interval _19770 = iratio(_17558, _17559, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19775;
    if (!intervalFailed)
    {
        _19775 = intervalFailed;
    }
    else
    {
        _19775 = false;
    }
    bool _19780;
    if (_19775)
    {
        _19780 = jetFailureSite == 0u;
    }
    else
    {
        _19780 = false;
    }
    if (_19780)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
    }
    float _17556 = 52.0;
    float _17557 = 10000.0;
    Interval _19784 = iratio(_17556, _17557, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19789;
    if (!intervalFailed)
    {
        _19789 = intervalFailed;
    }
    else
    {
        _19789 = false;
    }
    bool _19794;
    if (_19789)
    {
        _19794 = jetFailureSite == 0u;
    }
    else
    {
        _19794 = false;
    }
    if (_19794)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(52.0, 52.0, 10000.0, 10000.0);
    }
    Interval _17542 = _19784;
    Interval _17543 = _19534;
    Interval _19797 = imul(_17542, _17543, intervalFailed, optical_product_upper);
    Interval _17544 = Interval{ 0.0, 0.0 };
    Interval _17545 = _19534;
    Interval _19798 = jet_mul_derivative(_17544, _17545, intervalFailed, optical_product_upper);
    Interval _17546 = _19798;
    Interval _17547 = _19784;
    Interval _17548 = _19538;
    Interval _19799 = jet_mul_derivative(_17547, _17548, intervalFailed, optical_product_upper);
    Interval _17549 = _19799;
    Interval _19800 = jet_add_derivative(_17546, _17549, intervalFailed);
    Interval _17550 = Interval{ 0.0, 0.0 };
    Interval _17551 = _19534;
    Interval _19801 = jet_mul_derivative(_17550, _17551, intervalFailed, optical_product_upper);
    Interval _17552 = _19801;
    Interval _17553 = _19784;
    Interval _17554 = _19542;
    Interval _19802 = jet_mul_derivative(_17553, _17554, intervalFailed, optical_product_upper);
    Interval _17555 = _19802;
    Interval _19803 = jet_add_derivative(_17552, _17555, intervalFailed);
    Interval _17536 = _19770;
    Interval _17537 = Interval{ as_type<float>(as_type<uint>(_19797.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19797.lo) ^ 2147483648u) };
    Interval _19829 = iadd(_17536, _17537, intervalFailed);
    Interval _17538 = Interval{ 0.0, 0.0 };
    Interval _17539 = Interval{ as_type<float>(as_type<uint>(_19800.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19800.lo) ^ 2147483648u) };
    Interval _19831 = jet_add_derivative(_17538, _17539, intervalFailed);
    Interval _17540 = Interval{ 0.0, 0.0 };
    Interval _17541 = Interval{ as_type<float>(as_type<uint>(_19803.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19803.lo) ^ 2147483648u) };
    Interval _19833 = jet_add_derivative(_17540, _17541, intervalFailed);
    Interval _17522 = Interval{ 9.0, 9.0 };
    Interval _17523 = _19829;
    Interval _19834 = imul(_17522, _17523, intervalFailed, optical_product_upper);
    Interval _17524 = Interval{ 0.0, 0.0 };
    Interval _17525 = _19829;
    Interval _19835 = jet_mul_derivative(_17524, _17525, intervalFailed, optical_product_upper);
    Interval _17526 = _19835;
    Interval _17527 = Interval{ 9.0, 9.0 };
    Interval _17528 = _19831;
    Interval _19836 = jet_mul_derivative(_17527, _17528, intervalFailed, optical_product_upper);
    Interval _17529 = _19836;
    Interval _19837 = jet_add_derivative(_17526, _17529, intervalFailed);
    Interval _17530 = Interval{ 0.0, 0.0 };
    Interval _17531 = _19829;
    Interval _19838 = jet_mul_derivative(_17530, _17531, intervalFailed, optical_product_upper);
    Interval _17532 = _19838;
    Interval _17533 = Interval{ 9.0, 9.0 };
    Interval _17534 = _19833;
    Interval _19839 = jet_mul_derivative(_17533, _17534, intervalFailed, optical_product_upper);
    Interval _17535 = _19839;
    Interval _19840 = jet_add_derivative(_17532, _17535, intervalFailed);
    float _17516 = _18802.lo;
    float _17517 = _18802.hi;
    float _19843 = sine_bounds(_17516, _17517, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19847 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19850 = as_type<float>(as_type<uint>(_19843) ^ 2147483648u);
    Interval _17512 = _18802;
    float _17510 = 3.1415927410125732421875;
    float _19851 = interval_down(_17510, intervalFailed);
    float _17511 = 3.1415927410125732421875;
    float _19852 = interval_up(_17511, intervalFailed);
    Interval _17513 = Interval{ _19851, _19852 };
    Interval _17514 = Interval{ 0.5, 0.5 };
    Interval _19854 = imul(_17513, _17514, intervalFailed, optical_product_upper);
    Interval _17515 = _19854;
    Interval _19855 = iadd(_17512, _17515, intervalFailed);
    float _17508 = _19855.lo;
    float _17509 = _19855.hi;
    float _19858 = sine_bounds(_17508, _17509, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17518 = Interval{ _19847, _19850 };
    Interval _17519 = _18803;
    Interval _19861 = jet_mul_derivative(_17518, _17519, intervalFailed, optical_product_upper);
    Interval _17520 = Interval{ _19847, _19850 };
    Interval _17521 = _18804;
    Interval _19863 = jet_mul_derivative(_17520, _17521, intervalFailed, optical_product_upper);
    Interval _17494 = _19834;
    Interval _17495 = Interval{ _19858, interval_sine_upper };
    Interval _19865 = imul(_17494, _17495, intervalFailed, optical_product_upper);
    Interval _17496 = _19837;
    Interval _17497 = Interval{ _19858, interval_sine_upper };
    Interval _19867 = jet_mul_derivative(_17496, _17497, intervalFailed, optical_product_upper);
    Interval _17498 = _19867;
    Interval _17499 = _19834;
    Interval _17500 = _19861;
    Interval _19868 = jet_mul_derivative(_17499, _17500, intervalFailed, optical_product_upper);
    Interval _17501 = _19868;
    Interval _19869 = jet_add_derivative(_17498, _17501, intervalFailed);
    Interval _17502 = _19840;
    Interval _17503 = Interval{ _19858, interval_sine_upper };
    Interval _19871 = jet_mul_derivative(_17502, _17503, intervalFailed, optical_product_upper);
    Interval _17504 = _19871;
    Interval _17505 = _19834;
    Interval _17506 = _19863;
    Interval _19872 = jet_mul_derivative(_17505, _17506, intervalFailed, optical_product_upper);
    Interval _17507 = _19872;
    Interval _19873 = jet_add_derivative(_17504, _17507, intervalFailed);
    Interval _17480 = _19865;
    Interval _17481 = _19126;
    Interval _19874 = imul(_17480, _17481, intervalFailed, optical_product_upper);
    Interval _17482 = _19869;
    Interval _17483 = _19126;
    Interval _19875 = jet_mul_derivative(_17482, _17483, intervalFailed, optical_product_upper);
    Interval _17484 = _19875;
    Interval _17485 = _19865;
    Interval _17486 = _19153;
    Interval _19876 = jet_mul_derivative(_17485, _17486, intervalFailed, optical_product_upper);
    Interval _17487 = _19876;
    Interval _19877 = jet_add_derivative(_17484, _17487, intervalFailed);
    Interval _17488 = _19873;
    Interval _17489 = _19126;
    Interval _19878 = jet_mul_derivative(_17488, _17489, intervalFailed, optical_product_upper);
    Interval _17490 = _19878;
    Interval _17491 = _19865;
    Interval _17492 = _19165;
    Interval _19879 = jet_mul_derivative(_17491, _17492, intervalFailed, optical_product_upper);
    Interval _17493 = _19879;
    Interval _19880 = jet_add_derivative(_17490, _17493, intervalFailed);
    float _17478 = 625.0;
    float _17479 = 10000.0;
    Interval _19882 = iratio(_17478, _17479, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19887;
    if (!intervalFailed)
    {
        _19887 = intervalFailed;
    }
    else
    {
        _19887 = false;
    }
    bool _19892;
    if (_19887)
    {
        _19892 = jetFailureSite == 0u;
    }
    else
    {
        _19892 = false;
    }
    if (_19892)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(625.0, 625.0, 10000.0, 10000.0);
    }
    float _17472 = _18910.lo;
    float _17473 = _18910.hi;
    float _19897 = sine_bounds(_17472, _17473, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19901 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19904 = as_type<float>(as_type<uint>(_19897) ^ 2147483648u);
    Interval _17468 = _18910;
    float _17466 = 3.1415927410125732421875;
    float _19905 = interval_down(_17466, intervalFailed);
    float _17467 = 3.1415927410125732421875;
    float _19906 = interval_up(_17467, intervalFailed);
    Interval _17469 = Interval{ _19905, _19906 };
    Interval _17470 = Interval{ 0.5, 0.5 };
    Interval _19908 = imul(_17469, _17470, intervalFailed, optical_product_upper);
    Interval _17471 = _19908;
    Interval _19909 = iadd(_17468, _17471, intervalFailed);
    float _17464 = _19909.lo;
    float _17465 = _19909.hi;
    float _19912 = sine_bounds(_17464, _17465, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17474 = Interval{ _19901, _19904 };
    Interval _17475 = _18911;
    Interval _19915 = jet_mul_derivative(_17474, _17475, intervalFailed, optical_product_upper);
    Interval _17476 = Interval{ _19901, _19904 };
    Interval _17477 = _18912;
    Interval _19917 = jet_mul_derivative(_17476, _17477, intervalFailed, optical_product_upper);
    Interval _17450 = _19882;
    Interval _17451 = Interval{ _19912, interval_sine_upper };
    Interval _19919 = imul(_17450, _17451, intervalFailed, optical_product_upper);
    Interval _17452 = Interval{ 0.0, 0.0 };
    Interval _17453 = Interval{ _19912, interval_sine_upper };
    Interval _19921 = jet_mul_derivative(_17452, _17453, intervalFailed, optical_product_upper);
    Interval _17454 = _19921;
    Interval _17455 = _19882;
    Interval _17456 = _19915;
    Interval _19922 = jet_mul_derivative(_17455, _17456, intervalFailed, optical_product_upper);
    Interval _17457 = _19922;
    Interval _19923 = jet_add_derivative(_17454, _17457, intervalFailed);
    Interval _17458 = Interval{ 0.0, 0.0 };
    Interval _17459 = Interval{ _19912, interval_sine_upper };
    Interval _19925 = jet_mul_derivative(_17458, _17459, intervalFailed, optical_product_upper);
    Interval _17460 = _19925;
    Interval _17461 = _19882;
    Interval _17462 = _19917;
    Interval _19926 = jet_mul_derivative(_17461, _17462, intervalFailed, optical_product_upper);
    Interval _17463 = _19926;
    Interval _19927 = jet_add_derivative(_17460, _17463, intervalFailed);
    Interval _17436 = _19919;
    Interval _17437 = _19244;
    Interval _19928 = imul(_17436, _17437, intervalFailed, optical_product_upper);
    Interval _17438 = _19923;
    Interval _17439 = _19244;
    Interval _19929 = jet_mul_derivative(_17438, _17439, intervalFailed, optical_product_upper);
    Interval _17440 = _19929;
    Interval _17441 = _19919;
    Interval _17442 = _19271;
    Interval _19930 = jet_mul_derivative(_17441, _17442, intervalFailed, optical_product_upper);
    Interval _17443 = _19930;
    Interval _19931 = jet_add_derivative(_17440, _17443, intervalFailed);
    Interval _17444 = _19927;
    Interval _17445 = _19244;
    Interval _19932 = jet_mul_derivative(_17444, _17445, intervalFailed, optical_product_upper);
    Interval _17446 = _19932;
    Interval _17447 = _19919;
    Interval _17448 = _19283;
    Interval _19933 = jet_mul_derivative(_17447, _17448, intervalFailed, optical_product_upper);
    Interval _17449 = _19933;
    Interval _19934 = jet_add_derivative(_17446, _17449, intervalFailed);
    Interval _17430 = _19874;
    Interval _17431 = Interval{ as_type<float>(as_type<uint>(_19928.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19928.lo) ^ 2147483648u) };
    Interval _19960 = iadd(_17430, _17431, intervalFailed);
    Interval _17432 = _19877;
    Interval _17433 = Interval{ as_type<float>(as_type<uint>(_19931.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19931.lo) ^ 2147483648u) };
    Interval _19962 = jet_add_derivative(_17432, _17433, intervalFailed);
    Interval _17434 = _19880;
    Interval _17435 = Interval{ as_type<float>(as_type<uint>(_19934.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19934.lo) ^ 2147483648u) };
    Interval _19964 = jet_add_derivative(_17434, _17435, intervalFailed);
    float _17428 = 110.0;
    float _17429 = 1000.0;
    Interval _19966 = iratio(_17428, _17429, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19971;
    if (!intervalFailed)
    {
        _19971 = intervalFailed;
    }
    else
    {
        _19971 = false;
    }
    bool _19976;
    if (_19971)
    {
        _19976 = jetFailureSite == 0u;
    }
    else
    {
        _19976 = false;
    }
    if (_19976)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(110.0, 110.0, 1000.0, 1000.0);
    }
    float _17422 = _19043.lo;
    float _17423 = _19043.hi;
    float _19981 = sine_bounds(_17422, _17423, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19985 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19988 = as_type<float>(as_type<uint>(_19981) ^ 2147483648u);
    Interval _17418 = _19043;
    float _17416 = 3.1415927410125732421875;
    float _19989 = interval_down(_17416, intervalFailed);
    float _17417 = 3.1415927410125732421875;
    float _19990 = interval_up(_17417, intervalFailed);
    Interval _17419 = Interval{ _19989, _19990 };
    Interval _17420 = Interval{ 0.5, 0.5 };
    Interval _19992 = imul(_17419, _17420, intervalFailed, optical_product_upper);
    Interval _17421 = _19992;
    Interval _19993 = iadd(_17418, _17421, intervalFailed);
    float _17414 = _19993.lo;
    float _17415 = _19993.hi;
    float _19996 = sine_bounds(_17414, _17415, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17424 = Interval{ _19985, _19988 };
    Interval _17425 = _19045;
    Interval _19999 = jet_mul_derivative(_17424, _17425, intervalFailed, optical_product_upper);
    Interval _17426 = Interval{ _19985, _19988 };
    Interval _17427 = _19047;
    Interval _20001 = jet_mul_derivative(_17426, _17427, intervalFailed, optical_product_upper);
    Interval _17400 = _19966;
    Interval _17401 = Interval{ _19996, interval_sine_upper };
    Interval _20003 = imul(_17400, _17401, intervalFailed, optical_product_upper);
    Interval _17402 = Interval{ 0.0, 0.0 };
    Interval _17403 = Interval{ _19996, interval_sine_upper };
    Interval _20005 = jet_mul_derivative(_17402, _17403, intervalFailed, optical_product_upper);
    Interval _17404 = _20005;
    Interval _17405 = _19966;
    Interval _17406 = _19999;
    Interval _20006 = jet_mul_derivative(_17405, _17406, intervalFailed, optical_product_upper);
    Interval _17407 = _20006;
    Interval _20007 = jet_add_derivative(_17404, _17407, intervalFailed);
    Interval _17408 = Interval{ 0.0, 0.0 };
    Interval _17409 = Interval{ _19996, interval_sine_upper };
    Interval _20009 = jet_mul_derivative(_17408, _17409, intervalFailed, optical_product_upper);
    Interval _17410 = _20009;
    Interval _17411 = _19966;
    Interval _17412 = _20001;
    Interval _20010 = jet_mul_derivative(_17411, _17412, intervalFailed, optical_product_upper);
    Interval _17413 = _20010;
    Interval _20011 = jet_add_derivative(_17410, _17413, intervalFailed);
    Interval _17386 = _20003;
    Interval _17387 = _19362;
    Interval _20012 = imul(_17386, _17387, intervalFailed, optical_product_upper);
    Interval _17388 = _20007;
    Interval _17389 = _19362;
    Interval _20013 = jet_mul_derivative(_17388, _17389, intervalFailed, optical_product_upper);
    Interval _17390 = _20013;
    Interval _17391 = _20003;
    Interval _17392 = _19389;
    Interval _20014 = jet_mul_derivative(_17391, _17392, intervalFailed, optical_product_upper);
    Interval _17393 = _20014;
    Interval _20015 = jet_add_derivative(_17390, _17393, intervalFailed);
    Interval _17394 = _20011;
    Interval _17395 = _19362;
    Interval _20016 = jet_mul_derivative(_17394, _17395, intervalFailed, optical_product_upper);
    Interval _17396 = _20016;
    Interval _17397 = _20003;
    Interval _17398 = _19401;
    Interval _20017 = jet_mul_derivative(_17397, _17398, intervalFailed, optical_product_upper);
    Interval _17399 = _20017;
    Interval _20018 = jet_add_derivative(_17396, _17399, intervalFailed);
    Interval _17380 = _19960;
    Interval _17381 = _20012;
    Interval _20019 = iadd(_17380, _17381, intervalFailed);
    Interval _17382 = _19962;
    Interval _17383 = _20015;
    Interval _20020 = jet_add_derivative(_17382, _17383, intervalFailed);
    Interval _17384 = _19964;
    Interval _17385 = _20018;
    Interval _20021 = jet_add_derivative(_17384, _17385, intervalFailed);
    float _17378 = 173.0;
    float _17379 = 1000.0;
    Interval _20027 = iratio(_17378, _17379, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20032;
    if (!intervalFailed)
    {
        _20032 = intervalFailed;
    }
    else
    {
        _20032 = false;
    }
    bool _20037;
    if (_20032)
    {
        _20037 = jetFailureSite == 0u;
    }
    else
    {
        _20037 = false;
    }
    if (_20037)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(173.0, 173.0, 1000.0, 1000.0);
    }
    Interval _17364 = x.v;
    Interval _17365 = _20027;
    Interval _20040 = imul(_17364, _17365, intervalFailed, optical_product_upper);
    Interval _17366 = x.dx;
    Interval _17367 = _20027;
    Interval _20041 = jet_mul_derivative(_17366, _17367, intervalFailed, optical_product_upper);
    Interval _17368 = _20041;
    Interval _17369 = x.v;
    Interval _17370 = Interval{ 0.0, 0.0 };
    Interval _20042 = jet_mul_derivative(_17369, _17370, intervalFailed, optical_product_upper);
    Interval _17371 = _20042;
    Interval _20043 = jet_add_derivative(_17368, _17371, intervalFailed);
    Interval _17372 = x.dy;
    Interval _17373 = _20027;
    Interval _20044 = jet_mul_derivative(_17372, _17373, intervalFailed, optical_product_upper);
    Interval _17374 = _20044;
    Interval _17375 = x.v;
    Interval _17376 = Interval{ 0.0, 0.0 };
    Interval _20045 = jet_mul_derivative(_17375, _17376, intervalFailed, optical_product_upper);
    Interval _17377 = _20045;
    Interval _20046 = jet_add_derivative(_17374, _17377, intervalFailed);
    float _17362 = 129.0;
    float _17363 = 1000.0;
    Interval _20052 = iratio(_17362, _17363, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20057;
    if (!intervalFailed)
    {
        _20057 = intervalFailed;
    }
    else
    {
        _20057 = false;
    }
    bool _20062;
    if (_20057)
    {
        _20062 = jetFailureSite == 0u;
    }
    else
    {
        _20062 = false;
    }
    if (_20062)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(129.0, 129.0, 1000.0, 1000.0);
    }
    Interval _17348 = z.v;
    Interval _17349 = _20052;
    Interval _20065 = imul(_17348, _17349, intervalFailed, optical_product_upper);
    Interval _17350 = z.dx;
    Interval _17351 = _20052;
    Interval _20066 = jet_mul_derivative(_17350, _17351, intervalFailed, optical_product_upper);
    Interval _17352 = _20066;
    Interval _17353 = z.v;
    Interval _17354 = Interval{ 0.0, 0.0 };
    Interval _20067 = jet_mul_derivative(_17353, _17354, intervalFailed, optical_product_upper);
    Interval _17355 = _20067;
    Interval _20068 = jet_add_derivative(_17352, _17355, intervalFailed);
    Interval _17356 = z.dy;
    Interval _17357 = _20052;
    Interval _20069 = jet_mul_derivative(_17356, _17357, intervalFailed, optical_product_upper);
    Interval _17358 = _20069;
    Interval _17359 = z.v;
    Interval _17360 = Interval{ 0.0, 0.0 };
    Interval _20070 = jet_mul_derivative(_17359, _17360, intervalFailed, optical_product_upper);
    Interval _17361 = _20070;
    Interval _20071 = jet_add_derivative(_17358, _17361, intervalFailed);
    Interval _17342 = _20040;
    Interval _17343 = _20065;
    Interval _20072 = iadd(_17342, _17343, intervalFailed);
    Interval _17344 = _20043;
    Interval _17345 = _20068;
    Interval _20073 = jet_add_derivative(_17344, _17345, intervalFailed);
    Interval _17346 = _20046;
    Interval _17347 = _20071;
    Interval _20074 = jet_add_derivative(_17346, _17347, intervalFailed);
    float _17340 = 73.0;
    float _17341 = 100.0;
    Interval _20080 = iratio(_17340, _17341, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20085;
    if (!intervalFailed)
    {
        _20085 = intervalFailed;
    }
    else
    {
        _20085 = false;
    }
    bool _20090;
    if (_20085)
    {
        _20090 = jetFailureSite == 0u;
    }
    else
    {
        _20090 = false;
    }
    if (_20090)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(73.0, 73.0, 100.0, 100.0);
    }
    Interval _17326 = t.v;
    Interval _17327 = _20080;
    Interval _20093 = imul(_17326, _17327, intervalFailed, optical_product_upper);
    Interval _17328 = t.dx;
    Interval _17329 = _20080;
    Interval _20094 = jet_mul_derivative(_17328, _17329, intervalFailed, optical_product_upper);
    Interval _17330 = _20094;
    Interval _17331 = t.v;
    Interval _17332 = Interval{ 0.0, 0.0 };
    Interval _20095 = jet_mul_derivative(_17331, _17332, intervalFailed, optical_product_upper);
    Interval _17333 = _20095;
    Interval _20096 = jet_add_derivative(_17330, _17333, intervalFailed);
    Interval _17334 = t.dy;
    Interval _17335 = _20080;
    Interval _20097 = jet_mul_derivative(_17334, _17335, intervalFailed, optical_product_upper);
    Interval _17336 = _20097;
    Interval _17337 = t.v;
    Interval _17338 = Interval{ 0.0, 0.0 };
    Interval _20098 = jet_mul_derivative(_17337, _17338, intervalFailed, optical_product_upper);
    Interval _17339 = _20098;
    Interval _20099 = jet_add_derivative(_17336, _17339, intervalFailed);
    Interval _17320 = _20072;
    Interval _17321 = Interval{ as_type<float>(as_type<uint>(_20093.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20093.lo) ^ 2147483648u) };
    Interval _20125 = iadd(_17320, _17321, intervalFailed);
    Interval _17322 = _20073;
    Interval _17323 = Interval{ as_type<float>(as_type<uint>(_20096.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20096.lo) ^ 2147483648u) };
    Interval _20127 = jet_add_derivative(_17322, _17323, intervalFailed);
    Interval _17324 = _20074;
    Interval _17325 = Interval{ as_type<float>(as_type<uint>(_20099.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20099.lo) ^ 2147483648u) };
    Interval _20129 = jet_add_derivative(_17324, _17325, intervalFailed);
    float _17318 = 216.0;
    float _17319 = 1000.0;
    Interval _20132 = iratio(_17318, _17319, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20137;
    if (!intervalFailed)
    {
        _20137 = intervalFailed;
    }
    else
    {
        _20137 = false;
    }
    bool _20142;
    if (_20137)
    {
        _20142 = jetFailureSite == 0u;
    }
    else
    {
        _20142 = false;
    }
    if (_20142)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(216.0, 216.0, 1000.0, 1000.0);
    }
    Interval _17304 = footprint.v;
    Interval _17305 = _20132;
    Interval _20148 = imul(_17304, _17305, intervalFailed, optical_product_upper);
    Interval _17306 = footprint.dx;
    Interval _17307 = _20132;
    Interval _20149 = jet_mul_derivative(_17306, _17307, intervalFailed, optical_product_upper);
    Interval _17308 = _20149;
    Interval _17309 = footprint.v;
    Interval _17310 = Interval{ 0.0, 0.0 };
    Interval _20150 = jet_mul_derivative(_17309, _17310, intervalFailed, optical_product_upper);
    Interval _17311 = _20150;
    Interval _20151 = jet_add_derivative(_17308, _17311, intervalFailed);
    Interval _17312 = footprint.dy;
    Interval _17313 = _20132;
    Interval _20152 = jet_mul_derivative(_17312, _17313, intervalFailed, optical_product_upper);
    Interval _17314 = _20152;
    Interval _17315 = footprint.v;
    Interval _17316 = Interval{ 0.0, 0.0 };
    Interval _20153 = jet_mul_derivative(_17315, _17316, intervalFailed, optical_product_upper);
    Interval _17317 = _20153;
    Interval _20154 = jet_add_derivative(_17314, _17317, intervalFailed);
    bool _20161;
    if (_20148.lo <= 0.0)
    {
        _20161 = _20148.hi >= 0.0;
    }
    else
    {
        _20161 = false;
    }
    float _20168;
    if (_20161)
    {
        _20168 = 0.0;
    }
    else
    {
        _20168 = precise::min(abs(_20148.lo), abs(_20148.hi));
    }
    float _20171 = precise::max(abs(_20148.lo), abs(_20148.hi));
    float _17294 = spvFMul(_20168, _20168);
    float _20172 = interval_down(_17294, intervalFailed);
    float _20173 = precise::max(0.0, _20172);
    float _17295 = spvFMul(_20171, _20171);
    float _20174 = interval_up(_17295, intervalFailed);
    Interval _17296 = Interval{ 2.0, 2.0 };
    Interval _17297 = _20148;
    Interval _20175 = imul(_17296, _17297, intervalFailed, optical_product_upper);
    Interval _17298 = _20175;
    Interval _17299 = _20151;
    Interval _20176 = jet_mul_derivative(_17298, _17299, intervalFailed, optical_product_upper);
    Interval _17300 = Interval{ 2.0, 2.0 };
    Interval _17301 = _20148;
    Interval _20177 = imul(_17300, _17301, intervalFailed, optical_product_upper);
    Interval _17302 = _20177;
    Interval _17303 = _20154;
    Interval _20178 = jet_mul_derivative(_17302, _17303, intervalFailed, optical_product_upper);
    bool _20183;
    if (_20173 <= 0.0)
    {
        _20183 = _20174 >= 0.0;
    }
    else
    {
        _20183 = false;
    }
    float _20190;
    if (_20183)
    {
        _20190 = 0.0;
    }
    else
    {
        _20190 = precise::min(abs(_20173), abs(_20174));
    }
    float _20193 = precise::max(abs(_20173), abs(_20174));
    float _17284 = spvFMul(_20190, _20190);
    float _20194 = interval_down(_17284, intervalFailed);
    float _17285 = spvFMul(_20193, _20193);
    float _20196 = interval_up(_17285, intervalFailed);
    Interval _17286 = Interval{ 2.0, 2.0 };
    Interval _17287 = Interval{ _20173, _20174 };
    Interval _20198 = imul(_17286, _17287, intervalFailed, optical_product_upper);
    Interval _17288 = _20198;
    Interval _17289 = _20176;
    Interval _20199 = jet_mul_derivative(_17288, _17289, intervalFailed, optical_product_upper);
    Interval _17290 = Interval{ 2.0, 2.0 };
    Interval _17291 = Interval{ _20173, _20174 };
    Interval _20201 = imul(_17290, _17291, intervalFailed, optical_product_upper);
    Interval _17292 = _20201;
    Interval _17293 = _20178;
    Interval _20202 = jet_mul_derivative(_17292, _17293, intervalFailed, optical_product_upper);
    Interval _17278 = Interval{ 1.0, 1.0 };
    Interval _17279 = Interval{ precise::max(0.0, _20194), _20196 };
    Interval _20204 = iadd(_17278, _17279, intervalFailed);
    Interval _17280 = Interval{ 0.0, 0.0 };
    Interval _17281 = _20199;
    Interval _20205 = jet_add_derivative(_17280, _17281, intervalFailed);
    Interval _17282 = Interval{ 0.0, 0.0 };
    Interval _17283 = _20202;
    Interval _20206 = jet_add_derivative(_17282, _17283, intervalFailed);
    Interval _17264 = Interval{ 1.0, 1.0 };
    Interval _17265 = _20204;
    Interval _20208 = idiv(_17264, _17265, intervalFailed, interval_divide_upper);
    bool _20215;
    if (!intervalFailed)
    {
        _20215 = intervalFailed;
    }
    else
    {
        _20215 = false;
    }
    bool _20220;
    if (_20215)
    {
        _20220 = jetFailureSite == 0u;
    }
    else
    {
        _20220 = false;
    }
    if (_20220)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _20204.lo, _20204.hi);
    }
    Interval _17266 = Interval{ 0.0, 0.0 };
    Interval _17267 = _20208;
    Interval _17268 = _20205;
    Interval _20224 = jet_mul_derivative(_17267, _17268, intervalFailed, optical_product_upper);
    Interval _17269 = Interval{ as_type<float>(as_type<uint>(_20224.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20224.lo) ^ 2147483648u) };
    Interval _20234 = jet_add_derivative(_17266, _17269, intervalFailed);
    Interval _17270 = _20234;
    Interval _17271 = _20204;
    Interval _20235 = jet_div_derivative(_17270, _17271, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17272 = Interval{ 0.0, 0.0 };
    Interval _17273 = _20208;
    Interval _17274 = _20206;
    Interval _20236 = jet_mul_derivative(_17273, _17274, intervalFailed, optical_product_upper);
    Interval _17275 = Interval{ as_type<float>(as_type<uint>(_20236.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20236.lo) ^ 2147483648u) };
    Interval _20246 = jet_add_derivative(_17272, _17275, intervalFailed);
    Interval _17276 = _20246;
    Interval _17277 = _20204;
    Interval _20247 = jet_div_derivative(_17276, _17277, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _17262 = 38.0;
    float _17263 = 100.0;
    Interval _20249 = iratio(_17262, _17263, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20254;
    if (!intervalFailed)
    {
        _20254 = intervalFailed;
    }
    else
    {
        _20254 = false;
    }
    bool _20259;
    if (_20254)
    {
        _20259 = jetFailureSite == 0u;
    }
    else
    {
        _20259 = false;
    }
    if (_20259)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(38.0, 38.0, 100.0, 100.0);
    }
    Interval _17254 = _20125;
    float _17252 = 3.1415927410125732421875;
    float _20262 = interval_down(_17252, intervalFailed);
    float _17253 = 3.1415927410125732421875;
    float _20263 = interval_up(_17253, intervalFailed);
    Interval _17255 = Interval{ _20262, _20263 };
    Interval _17256 = Interval{ 0.5, 0.5 };
    Interval _20265 = imul(_17255, _17256, intervalFailed, optical_product_upper);
    Interval _17257 = _20265;
    Interval _20266 = iadd(_17254, _17257, intervalFailed);
    float _17250 = _20266.lo;
    float _17251 = _20266.hi;
    float _20269 = sine_bounds(_17250, _17251, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17248 = _20125.lo;
    float _17249 = _20125.hi;
    float _20273 = sine_bounds(_17248, _17249, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17258 = Interval{ _20269, interval_sine_upper };
    Interval _17259 = _20127;
    Interval _20276 = jet_mul_derivative(_17258, _17259, intervalFailed, optical_product_upper);
    Interval _17260 = Interval{ _20269, interval_sine_upper };
    Interval _17261 = _20129;
    Interval _20278 = jet_mul_derivative(_17260, _17261, intervalFailed, optical_product_upper);
    Interval _17234 = _20249;
    Interval _17235 = Interval{ _20273, interval_sine_upper };
    Interval _20280 = imul(_17234, _17235, intervalFailed, optical_product_upper);
    Interval _17236 = Interval{ 0.0, 0.0 };
    Interval _17237 = Interval{ _20273, interval_sine_upper };
    Interval _20282 = jet_mul_derivative(_17236, _17237, intervalFailed, optical_product_upper);
    Interval _17238 = _20282;
    Interval _17239 = _20249;
    Interval _17240 = _20276;
    Interval _20283 = jet_mul_derivative(_17239, _17240, intervalFailed, optical_product_upper);
    Interval _17241 = _20283;
    Interval _20284 = jet_add_derivative(_17238, _17241, intervalFailed);
    Interval _17242 = Interval{ 0.0, 0.0 };
    Interval _17243 = Interval{ _20273, interval_sine_upper };
    Interval _20286 = jet_mul_derivative(_17242, _17243, intervalFailed, optical_product_upper);
    Interval _17244 = _20286;
    Interval _17245 = _20249;
    Interval _17246 = _20278;
    Interval _20287 = jet_mul_derivative(_17245, _17246, intervalFailed, optical_product_upper);
    Interval _17247 = _20287;
    Interval _20288 = jet_add_derivative(_17244, _17247, intervalFailed);
    Interval _17220 = _20280;
    Interval _17221 = _20208;
    Interval _20289 = imul(_17220, _17221, intervalFailed, optical_product_upper);
    Interval _17222 = _20284;
    Interval _17223 = _20208;
    Interval _20290 = jet_mul_derivative(_17222, _17223, intervalFailed, optical_product_upper);
    Interval _17224 = _20290;
    Interval _17225 = _20280;
    Interval _17226 = _20235;
    Interval _20291 = jet_mul_derivative(_17225, _17226, intervalFailed, optical_product_upper);
    Interval _17227 = _20291;
    Interval _20292 = jet_add_derivative(_17224, _17227, intervalFailed);
    Interval _17228 = _20288;
    Interval _17229 = _20208;
    Interval _20293 = jet_mul_derivative(_17228, _17229, intervalFailed, optical_product_upper);
    Interval _17230 = _20293;
    Interval _17231 = _20280;
    Interval _17232 = _20247;
    Interval _20294 = jet_mul_derivative(_17231, _17232, intervalFailed, optical_product_upper);
    Interval _17233 = _20294;
    Interval _20295 = jet_add_derivative(_17230, _17233, intervalFailed);
    Interval _17214 = _19507;
    Interval _17215 = _20289;
    Interval _20296 = iadd(_17214, _17215, intervalFailed);
    Interval _17216 = _19508;
    Interval _17217 = _20292;
    Interval _20297 = jet_add_derivative(_17216, _17217, intervalFailed);
    Interval _17218 = _19509;
    Interval _17219 = _20295;
    Interval _20298 = jet_add_derivative(_17218, _17219, intervalFailed);
    float _17212 = 6574.0;
    float _17213 = 100000.0;
    Interval _20306 = iratio(_17212, _17213, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20311;
    if (!intervalFailed)
    {
        _20311 = intervalFailed;
    }
    else
    {
        _20311 = false;
    }
    bool _20316;
    if (_20311)
    {
        _20316 = jetFailureSite == 0u;
    }
    else
    {
        _20316 = false;
    }
    if (_20316)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(6574.0, 6574.0, 100000.0, 100000.0);
    }
    float _17206 = _20125.lo;
    float _17207 = _20125.hi;
    float _20321 = sine_bounds(_17206, _17207, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20325 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20328 = as_type<float>(as_type<uint>(_20321) ^ 2147483648u);
    Interval _17202 = _20125;
    float _17200 = 3.1415927410125732421875;
    float _20329 = interval_down(_17200, intervalFailed);
    float _17201 = 3.1415927410125732421875;
    float _20330 = interval_up(_17201, intervalFailed);
    Interval _17203 = Interval{ _20329, _20330 };
    Interval _17204 = Interval{ 0.5, 0.5 };
    Interval _20332 = imul(_17203, _17204, intervalFailed, optical_product_upper);
    Interval _17205 = _20332;
    Interval _20333 = iadd(_17202, _17205, intervalFailed);
    float _17198 = _20333.lo;
    float _17199 = _20333.hi;
    float _20336 = sine_bounds(_17198, _17199, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17208 = Interval{ _20325, _20328 };
    Interval _17209 = _20127;
    Interval _20339 = jet_mul_derivative(_17208, _17209, intervalFailed, optical_product_upper);
    Interval _17210 = Interval{ _20325, _20328 };
    Interval _17211 = _20129;
    Interval _20341 = jet_mul_derivative(_17210, _17211, intervalFailed, optical_product_upper);
    Interval _17184 = _20306;
    Interval _17185 = Interval{ _20336, interval_sine_upper };
    Interval _20343 = imul(_17184, _17185, intervalFailed, optical_product_upper);
    Interval _17186 = Interval{ 0.0, 0.0 };
    Interval _17187 = Interval{ _20336, interval_sine_upper };
    Interval _20345 = jet_mul_derivative(_17186, _17187, intervalFailed, optical_product_upper);
    Interval _17188 = _20345;
    Interval _17189 = _20306;
    Interval _17190 = _20339;
    Interval _20346 = jet_mul_derivative(_17189, _17190, intervalFailed, optical_product_upper);
    Interval _17191 = _20346;
    Interval _20347 = jet_add_derivative(_17188, _17191, intervalFailed);
    Interval _17192 = Interval{ 0.0, 0.0 };
    Interval _17193 = Interval{ _20336, interval_sine_upper };
    Interval _20349 = jet_mul_derivative(_17192, _17193, intervalFailed, optical_product_upper);
    Interval _17194 = _20349;
    Interval _17195 = _20306;
    Interval _17196 = _20341;
    Interval _20350 = jet_mul_derivative(_17195, _17196, intervalFailed, optical_product_upper);
    Interval _17197 = _20350;
    Interval _20351 = jet_add_derivative(_17194, _17197, intervalFailed);
    Interval _17170 = _20343;
    Interval _17171 = _20208;
    Interval _20352 = imul(_17170, _17171, intervalFailed, optical_product_upper);
    Interval _17172 = _20347;
    Interval _17173 = _20208;
    Interval _20353 = jet_mul_derivative(_17172, _17173, intervalFailed, optical_product_upper);
    Interval _17174 = _20353;
    Interval _17175 = _20343;
    Interval _17176 = _20235;
    Interval _20354 = jet_mul_derivative(_17175, _17176, intervalFailed, optical_product_upper);
    Interval _17177 = _20354;
    Interval _20355 = jet_add_derivative(_17174, _17177, intervalFailed);
    Interval _17178 = _20351;
    Interval _17179 = _20208;
    Interval _20356 = jet_mul_derivative(_17178, _17179, intervalFailed, optical_product_upper);
    Interval _17180 = _20356;
    Interval _17181 = _20343;
    Interval _17182 = _20247;
    Interval _20357 = jet_mul_derivative(_17181, _17182, intervalFailed, optical_product_upper);
    Interval _17183 = _20357;
    Interval _20358 = jet_add_derivative(_17180, _17183, intervalFailed);
    Interval _17164 = _19764;
    Interval _17165 = _20352;
    Interval _20359 = iadd(_17164, _17165, intervalFailed);
    Interval _17166 = _19766;
    Interval _17167 = _20355;
    Interval _20360 = jet_add_derivative(_17166, _17167, intervalFailed);
    Interval _17168 = _19768;
    Interval _17169 = _20358;
    Interval _20361 = jet_add_derivative(_17168, _17169, intervalFailed);
    float _17162 = 4902.0;
    float _17163 = 100000.0;
    Interval _20369 = iratio(_17162, _17163, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20374;
    if (!intervalFailed)
    {
        _20374 = intervalFailed;
    }
    else
    {
        _20374 = false;
    }
    bool _20379;
    if (_20374)
    {
        _20379 = jetFailureSite == 0u;
    }
    else
    {
        _20379 = false;
    }
    if (_20379)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(4902.0, 4902.0, 100000.0, 100000.0);
    }
    float _17156 = _20125.lo;
    float _17157 = _20125.hi;
    float _20384 = sine_bounds(_17156, _17157, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20388 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20391 = as_type<float>(as_type<uint>(_20384) ^ 2147483648u);
    Interval _17152 = _20125;
    float _17150 = 3.1415927410125732421875;
    float _20392 = interval_down(_17150, intervalFailed);
    float _17151 = 3.1415927410125732421875;
    float _20393 = interval_up(_17151, intervalFailed);
    Interval _17153 = Interval{ _20392, _20393 };
    Interval _17154 = Interval{ 0.5, 0.5 };
    Interval _20395 = imul(_17153, _17154, intervalFailed, optical_product_upper);
    Interval _17155 = _20395;
    Interval _20396 = iadd(_17152, _17155, intervalFailed);
    float _17148 = _20396.lo;
    float _17149 = _20396.hi;
    float _20399 = sine_bounds(_17148, _17149, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17158 = Interval{ _20388, _20391 };
    Interval _17159 = _20127;
    Interval _20402 = jet_mul_derivative(_17158, _17159, intervalFailed, optical_product_upper);
    Interval _17160 = Interval{ _20388, _20391 };
    Interval _17161 = _20129;
    Interval _20404 = jet_mul_derivative(_17160, _17161, intervalFailed, optical_product_upper);
    Interval _17134 = _20369;
    Interval _17135 = Interval{ _20399, interval_sine_upper };
    Interval _20406 = imul(_17134, _17135, intervalFailed, optical_product_upper);
    Interval _17136 = Interval{ 0.0, 0.0 };
    Interval _17137 = Interval{ _20399, interval_sine_upper };
    Interval _20408 = jet_mul_derivative(_17136, _17137, intervalFailed, optical_product_upper);
    Interval _17138 = _20408;
    Interval _17139 = _20369;
    Interval _17140 = _20402;
    Interval _20409 = jet_mul_derivative(_17139, _17140, intervalFailed, optical_product_upper);
    Interval _17141 = _20409;
    Interval _20410 = jet_add_derivative(_17138, _17141, intervalFailed);
    Interval _17142 = Interval{ 0.0, 0.0 };
    Interval _17143 = Interval{ _20399, interval_sine_upper };
    Interval _20412 = jet_mul_derivative(_17142, _17143, intervalFailed, optical_product_upper);
    Interval _17144 = _20412;
    Interval _17145 = _20369;
    Interval _17146 = _20404;
    Interval _20413 = jet_mul_derivative(_17145, _17146, intervalFailed, optical_product_upper);
    Interval _17147 = _20413;
    Interval _20414 = jet_add_derivative(_17144, _17147, intervalFailed);
    Interval _17120 = _20406;
    Interval _17121 = _20208;
    Interval _20415 = imul(_17120, _17121, intervalFailed, optical_product_upper);
    Interval _17122 = _20410;
    Interval _17123 = _20208;
    Interval _20416 = jet_mul_derivative(_17122, _17123, intervalFailed, optical_product_upper);
    Interval _17124 = _20416;
    Interval _17125 = _20406;
    Interval _17126 = _20235;
    Interval _20417 = jet_mul_derivative(_17125, _17126, intervalFailed, optical_product_upper);
    Interval _17127 = _20417;
    Interval _20418 = jet_add_derivative(_17124, _17127, intervalFailed);
    Interval _17128 = _20414;
    Interval _17129 = _20208;
    Interval _20419 = jet_mul_derivative(_17128, _17129, intervalFailed, optical_product_upper);
    Interval _17130 = _20419;
    Interval _17131 = _20406;
    Interval _17132 = _20247;
    Interval _20420 = jet_mul_derivative(_17131, _17132, intervalFailed, optical_product_upper);
    Interval _17133 = _20420;
    Interval _20421 = jet_add_derivative(_17130, _17133, intervalFailed);
    Interval _17114 = _20019;
    Interval _17115 = _20415;
    Interval _20422 = iadd(_17114, _17115, intervalFailed);
    Interval _17116 = _20020;
    Interval _17117 = _20418;
    Interval _20423 = jet_add_derivative(_17116, _17117, intervalFailed);
    Interval _17118 = _20021;
    Interval _17119 = _20421;
    Interval _20424 = jet_add_derivative(_17118, _17119, intervalFailed);
    float _21430;
    float _21431;
    float _21432;
    float _21433;
    float _21434;
    float _21435;
    float _21436;
    float _21437;
    float _21438;
    float _21439;
    float _21440;
    float _21441;
    float _21442;
    float _21443;
    float _21444;
    float _21445;
    float _21446;
    float _21447;
    if (footprint.v.lo < 24.0)
    {
        if (footprint.v.hi >= 24.0)
        {
            jetBranchKnown = false;
            return OpticalJet3{ OpticalJet{ _20296, _20297, _20298 }, OpticalJet{ _20359, _20360, _20361 }, OpticalJet{ _20422, _20423, _20424 } };
        }
        Interval _17100 = x.v;
        Interval _17101 = Interval{ 128.0, 128.0 };
        Interval _20446 = idiv(_17100, _17101, intervalFailed, interval_divide_upper);
        bool _20453;
        if (!intervalFailed)
        {
            _20453 = intervalFailed;
        }
        else
        {
            _20453 = false;
        }
        bool _20458;
        if (_20453)
        {
            _20458 = jetFailureSite == 0u;
        }
        else
        {
            _20458 = false;
        }
        if (_20458)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(x.v.lo, x.v.hi, 128.0, 128.0);
        }
        Interval _17102 = x.dx;
        Interval _17103 = _20446;
        Interval _17104 = Interval{ 0.0, 0.0 };
        Interval _20462 = jet_mul_derivative(_17103, _17104, intervalFailed, optical_product_upper);
        Interval _17105 = Interval{ as_type<float>(as_type<uint>(_20462.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20462.lo) ^ 2147483648u) };
        Interval _20472 = jet_add_derivative(_17102, _17105, intervalFailed);
        Interval _17106 = _20472;
        Interval _17107 = Interval{ 128.0, 128.0 };
        Interval _20473 = jet_div_derivative(_17106, _17107, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17108 = x.dy;
        Interval _17109 = _20446;
        Interval _17110 = Interval{ 0.0, 0.0 };
        Interval _20474 = jet_mul_derivative(_17109, _17110, intervalFailed, optical_product_upper);
        Interval _17111 = Interval{ as_type<float>(as_type<uint>(_20474.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20474.lo) ^ 2147483648u) };
        Interval _20484 = jet_add_derivative(_17108, _17111, intervalFailed);
        Interval _17112 = _20484;
        Interval _17113 = Interval{ 128.0, 128.0 };
        Interval _20485 = jet_div_derivative(_17112, _17113, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17086 = z.v;
        Interval _17087 = Interval{ 128.0, 128.0 };
        Interval _20493 = idiv(_17086, _17087, intervalFailed, interval_divide_upper);
        bool _20500;
        if (!intervalFailed)
        {
            _20500 = intervalFailed;
        }
        else
        {
            _20500 = false;
        }
        bool _20505;
        if (_20500)
        {
            _20505 = jetFailureSite == 0u;
        }
        else
        {
            _20505 = false;
        }
        if (_20505)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(z.v.lo, z.v.hi, 128.0, 128.0);
        }
        Interval _17088 = z.dx;
        Interval _17089 = _20493;
        Interval _17090 = Interval{ 0.0, 0.0 };
        Interval _20509 = jet_mul_derivative(_17089, _17090, intervalFailed, optical_product_upper);
        Interval _17091 = Interval{ as_type<float>(as_type<uint>(_20509.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20509.lo) ^ 2147483648u) };
        Interval _20519 = jet_add_derivative(_17088, _17091, intervalFailed);
        Interval _17092 = _20519;
        Interval _17093 = Interval{ 128.0, 128.0 };
        Interval _20520 = jet_div_derivative(_17092, _17093, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17094 = z.dy;
        Interval _17095 = _20493;
        Interval _17096 = Interval{ 0.0, 0.0 };
        Interval _20521 = jet_mul_derivative(_17095, _17096, intervalFailed, optical_product_upper);
        Interval _17097 = Interval{ as_type<float>(as_type<uint>(_20521.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20521.lo) ^ 2147483648u) };
        Interval _20531 = jet_add_derivative(_17094, _17097, intervalFailed);
        Interval _17098 = _20531;
        Interval _17099 = Interval{ 128.0, 128.0 };
        Interval _20532 = jet_div_derivative(_17098, _17099, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        float _20535 = floor(_20446.lo);
        float _20536 = floor(_20446.hi);
        bool _20541;
        if ((isunordered(_20535, _20536) || _20535 == _20536))
        {
            _20541 = floor(_20493.lo) != floor(_20493.hi);
        }
        else
        {
            _20541 = true;
        }
        float _21408;
        float _21409;
        float _21410;
        float _21411;
        float _21412;
        float _21413;
        float _21414;
        float _21415;
        float _21416;
        float _21417;
        float _21418;
        float _21419;
        float _21420;
        float _21421;
        float _21422;
        float _21423;
        float _21424;
        float _21425;
        if (_20541)
        {
            jetBranchKnown = false;
            return OpticalJet3{ OpticalJet{ _20296, _20297, _20298 }, OpticalJet{ _20359, _20360, _20361 }, OpticalJet{ _20422, _20423, _20424 } };
        }
        else
        {
            int _20543 = int(floor(_20446.lo));
            int _20545 = int(floor(_20493.lo));
            uint _20549 = (uint(_20543) * 1597334677u) ^ (uint(_20545) * 3812015801u);
            uint _1918 = (_20549 ^ (_20549 >> 16u)) * 2246822519u;
            float _17084 = float((_1918 ^ (_1918 >> 13u)) & 65535u);
            float _17085 = 65535.0;
            Interval _20556 = iratio(_17084, _17085, intervalFailed, optical_product_upper, interval_divide_upper);
            float _20557 = float(_20543);
            float _20558 = float(_20545);
            bool _20563;
            if (!intervalFailed)
            {
                _20563 = intervalFailed;
            }
            else
            {
                _20563 = false;
            }
            bool _20568;
            if (_20563)
            {
                _20568 = jetFailureSite == 0u;
            }
            else
            {
                _20568 = false;
            }
            if (_20568)
            {
                jetFailureSite = 7u;
                jetFailureArguments = float4(_20557, _20557, _20558, _20558);
            }
            float param_var_n = 64.0;
            float param_var_d = 100.0;
            Interval _20574 = iratio(param_var_n, param_var_d, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _20579;
            if (_20556.lo <= _20574.hi)
            {
                _20579 = _20556.hi > _20574.lo;
            }
            else
            {
                _20579 = false;
            }
            if (_20579)
            {
                jetBranchKnown = false;
                return OpticalJet3{ OpticalJet{ _20296, _20297, _20298 }, OpticalJet{ _20359, _20360, _20361 }, OpticalJet{ _20422, _20423, _20424 } };
            }
            if (_20556.lo > _20574.hi)
            {
                float _20585 = float(_20543);
                Interval _17078 = _20446;
                Interval _17079 = Interval{ as_type<float>(as_type<uint>(_20585) ^ 2147483648u), as_type<float>(as_type<uint>(_20585) ^ 2147483648u) };
                Interval _20597 = iadd(_17078, _17079, intervalFailed);
                Interval _17080 = _20473;
                Interval _17081 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20599 = jet_add_derivative(_17080, _17081, intervalFailed);
                Interval _17082 = _20485;
                Interval _17083 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20601 = jet_add_derivative(_17082, _17083, intervalFailed);
                float _17076 = 28.0;
                float _17077 = 100.0;
                Interval _20603 = iratio(_17076, _17077, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20608;
                if (!intervalFailed)
                {
                    _20608 = intervalFailed;
                }
                else
                {
                    _20608 = false;
                }
                bool _20613;
                if (_20608)
                {
                    _20613 = jetFailureSite == 0u;
                }
                else
                {
                    _20613 = false;
                }
                if (_20613)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(28.0, 28.0, 100.0, 100.0);
                }
                float _17074 = 44.0;
                float _17075 = 100.0;
                Interval _20617 = iratio(_17074, _17075, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20622;
                if (!intervalFailed)
                {
                    _20622 = intervalFailed;
                }
                else
                {
                    _20622 = false;
                }
                bool _20627;
                if (_20622)
                {
                    _20627 = jetFailureSite == 0u;
                }
                else
                {
                    _20627 = false;
                }
                if (_20627)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(44.0, 44.0, 100.0, 100.0);
                }
                int _1721 = _20543 + 19;
                uint _20633 = (uint(_1721) * 1597334677u) ^ (uint(_20545) * 3812015801u);
                uint _1921 = (_20633 ^ (_20633 >> 16u)) * 2246822519u;
                float _17072 = float((_1921 ^ (_1921 >> 13u)) & 65535u);
                float _17073 = 65535.0;
                Interval _20640 = iratio(_17072, _17073, intervalFailed, optical_product_upper, interval_divide_upper);
                float _20641 = float(_1721);
                float _20642 = float(_20545);
                bool _20647;
                if (!intervalFailed)
                {
                    _20647 = intervalFailed;
                }
                else
                {
                    _20647 = false;
                }
                bool _20652;
                if (_20647)
                {
                    _20652 = jetFailureSite == 0u;
                }
                else
                {
                    _20652 = false;
                }
                if (_20652)
                {
                    jetFailureSite = 7u;
                    jetFailureArguments = float4(_20641, _20641, _20642, _20642);
                }
                Interval _17058 = _20617;
                Interval _17059 = _20640;
                Interval _20656 = imul(_17058, _17059, intervalFailed, optical_product_upper);
                Interval _17060 = Interval{ 0.0, 0.0 };
                Interval _17061 = _20640;
                Interval _20657 = jet_mul_derivative(_17060, _17061, intervalFailed, optical_product_upper);
                Interval _17062 = _20657;
                Interval _17063 = _20617;
                Interval _17064 = Interval{ 0.0, 0.0 };
                Interval _20658 = jet_mul_derivative(_17063, _17064, intervalFailed, optical_product_upper);
                Interval _17065 = _20658;
                Interval _20659 = jet_add_derivative(_17062, _17065, intervalFailed);
                Interval _17066 = Interval{ 0.0, 0.0 };
                Interval _17067 = _20640;
                Interval _20660 = jet_mul_derivative(_17066, _17067, intervalFailed, optical_product_upper);
                Interval _17068 = _20660;
                Interval _17069 = _20617;
                Interval _17070 = Interval{ 0.0, 0.0 };
                Interval _20661 = jet_mul_derivative(_17069, _17070, intervalFailed, optical_product_upper);
                Interval _17071 = _20661;
                Interval _20662 = jet_add_derivative(_17068, _17071, intervalFailed);
                Interval _17052 = _20603;
                Interval _17053 = _20656;
                Interval _20663 = iadd(_17052, _17053, intervalFailed);
                Interval _17054 = Interval{ 0.0, 0.0 };
                Interval _17055 = _20659;
                Interval _20664 = jet_add_derivative(_17054, _17055, intervalFailed);
                Interval _17056 = Interval{ 0.0, 0.0 };
                Interval _17057 = _20662;
                Interval _20665 = jet_add_derivative(_17056, _17057, intervalFailed);
                Interval _17046 = _20597;
                Interval _17047 = Interval{ as_type<float>(as_type<uint>(_20663.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20663.lo) ^ 2147483648u) };
                Interval _20691 = iadd(_17046, _17047, intervalFailed);
                Interval _17048 = _20599;
                Interval _17049 = Interval{ as_type<float>(as_type<uint>(_20664.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20664.lo) ^ 2147483648u) };
                Interval _20693 = jet_add_derivative(_17048, _17049, intervalFailed);
                Interval _17050 = _20601;
                Interval _17051 = Interval{ as_type<float>(as_type<uint>(_20665.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20665.lo) ^ 2147483648u) };
                Interval _20695 = jet_add_derivative(_17050, _17051, intervalFailed);
                float _20696 = float(_20545);
                Interval _17040 = _20493;
                Interval _17041 = Interval{ as_type<float>(as_type<uint>(_20696) ^ 2147483648u), as_type<float>(as_type<uint>(_20696) ^ 2147483648u) };
                Interval _20708 = iadd(_17040, _17041, intervalFailed);
                Interval _17042 = _20520;
                Interval _17043 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20710 = jet_add_derivative(_17042, _17043, intervalFailed);
                Interval _17044 = _20532;
                Interval _17045 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20712 = jet_add_derivative(_17044, _17045, intervalFailed);
                float _17038 = 28.0;
                float _17039 = 100.0;
                Interval _20714 = iratio(_17038, _17039, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20719;
                if (!intervalFailed)
                {
                    _20719 = intervalFailed;
                }
                else
                {
                    _20719 = false;
                }
                bool _20724;
                if (_20719)
                {
                    _20724 = jetFailureSite == 0u;
                }
                else
                {
                    _20724 = false;
                }
                if (_20724)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(28.0, 28.0, 100.0, 100.0);
                }
                float _17036 = 44.0;
                float _17037 = 100.0;
                Interval _20728 = iratio(_17036, _17037, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20733;
                if (!intervalFailed)
                {
                    _20733 = intervalFailed;
                }
                else
                {
                    _20733 = false;
                }
                bool _20738;
                if (_20733)
                {
                    _20738 = jetFailureSite == 0u;
                }
                else
                {
                    _20738 = false;
                }
                if (_20738)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(44.0, 44.0, 100.0, 100.0);
                }
                int _1722 = _20545 + 29;
                uint _20744 = (uint(_20543) * 1597334677u) ^ (uint(_1722) * 3812015801u);
                uint _1924 = (_20744 ^ (_20744 >> 16u)) * 2246822519u;
                float _17034 = float((_1924 ^ (_1924 >> 13u)) & 65535u);
                float _17035 = 65535.0;
                Interval _20751 = iratio(_17034, _17035, intervalFailed, optical_product_upper, interval_divide_upper);
                float _20752 = float(_20543);
                float _20753 = float(_1722);
                bool _20758;
                if (!intervalFailed)
                {
                    _20758 = intervalFailed;
                }
                else
                {
                    _20758 = false;
                }
                bool _20763;
                if (_20758)
                {
                    _20763 = jetFailureSite == 0u;
                }
                else
                {
                    _20763 = false;
                }
                if (_20763)
                {
                    jetFailureSite = 7u;
                    jetFailureArguments = float4(_20752, _20752, _20753, _20753);
                }
                Interval _17020 = _20728;
                Interval _17021 = _20751;
                Interval _20767 = imul(_17020, _17021, intervalFailed, optical_product_upper);
                Interval _17022 = Interval{ 0.0, 0.0 };
                Interval _17023 = _20751;
                Interval _20768 = jet_mul_derivative(_17022, _17023, intervalFailed, optical_product_upper);
                Interval _17024 = _20768;
                Interval _17025 = _20728;
                Interval _17026 = Interval{ 0.0, 0.0 };
                Interval _20769 = jet_mul_derivative(_17025, _17026, intervalFailed, optical_product_upper);
                Interval _17027 = _20769;
                Interval _20770 = jet_add_derivative(_17024, _17027, intervalFailed);
                Interval _17028 = Interval{ 0.0, 0.0 };
                Interval _17029 = _20751;
                Interval _20771 = jet_mul_derivative(_17028, _17029, intervalFailed, optical_product_upper);
                Interval _17030 = _20771;
                Interval _17031 = _20728;
                Interval _17032 = Interval{ 0.0, 0.0 };
                Interval _20772 = jet_mul_derivative(_17031, _17032, intervalFailed, optical_product_upper);
                Interval _17033 = _20772;
                Interval _20773 = jet_add_derivative(_17030, _17033, intervalFailed);
                Interval _17014 = _20714;
                Interval _17015 = _20767;
                Interval _20774 = iadd(_17014, _17015, intervalFailed);
                Interval _17016 = Interval{ 0.0, 0.0 };
                Interval _17017 = _20770;
                Interval _20775 = jet_add_derivative(_17016, _17017, intervalFailed);
                Interval _17018 = Interval{ 0.0, 0.0 };
                Interval _17019 = _20773;
                Interval _20776 = jet_add_derivative(_17018, _17019, intervalFailed);
                Interval _17008 = _20708;
                Interval _17009 = Interval{ as_type<float>(as_type<uint>(_20774.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20774.lo) ^ 2147483648u) };
                Interval _20802 = iadd(_17008, _17009, intervalFailed);
                Interval _17010 = _20710;
                Interval _17011 = Interval{ as_type<float>(as_type<uint>(_20775.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20775.lo) ^ 2147483648u) };
                Interval _20804 = jet_add_derivative(_17010, _17011, intervalFailed);
                Interval _17012 = _20712;
                Interval _17013 = Interval{ as_type<float>(as_type<uint>(_20776.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20776.lo) ^ 2147483648u) };
                Interval _20806 = jet_add_derivative(_17012, _17013, intervalFailed);
                float _17006 = 14.0;
                float _17007 = 100.0;
                Interval _20812 = iratio(_17006, _17007, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20817;
                if (!intervalFailed)
                {
                    _20817 = intervalFailed;
                }
                else
                {
                    _20817 = false;
                }
                bool _20822;
                if (_20817)
                {
                    _20822 = jetFailureSite == 0u;
                }
                else
                {
                    _20822 = false;
                }
                if (_20822)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(14.0, 14.0, 100.0, 100.0);
                }
                Interval _20863;
                Interval _20865;
                Interval _20867;
                Interval _16992 = t.v;
                Interval _16993 = _20812;
                Interval _20825 = imul(_16992, _16993, intervalFailed, optical_product_upper);
                Interval _16994 = t.dx;
                Interval _16995 = _20812;
                Interval _20826 = jet_mul_derivative(_16994, _16995, intervalFailed, optical_product_upper);
                Interval _16996 = _20826;
                Interval _16997 = t.v;
                Interval _16998 = Interval{ 0.0, 0.0 };
                Interval _20827 = jet_mul_derivative(_16997, _16998, intervalFailed, optical_product_upper);
                Interval _16999 = _20827;
                Interval _20828 = jet_add_derivative(_16996, _16999, intervalFailed);
                Interval _17000 = t.dy;
                Interval _17001 = _20812;
                Interval _20829 = jet_mul_derivative(_17000, _17001, intervalFailed, optical_product_upper);
                Interval _17002 = _20829;
                Interval _17003 = t.v;
                Interval _17004 = Interval{ 0.0, 0.0 };
                Interval _20830 = jet_mul_derivative(_17003, _17004, intervalFailed, optical_product_upper);
                Interval _17005 = _20830;
                Interval _20831 = jet_add_derivative(_17002, _17005, intervalFailed);
                Interval _16978 = _20556;
                Interval _16979 = Interval{ 7.0, 7.0 };
                Interval _20832 = imul(_16978, _16979, intervalFailed, optical_product_upper);
                Interval _16980 = Interval{ 0.0, 0.0 };
                Interval _16981 = Interval{ 7.0, 7.0 };
                Interval _20833 = jet_mul_derivative(_16980, _16981, intervalFailed, optical_product_upper);
                Interval _16982 = _20833;
                Interval _16983 = _20556;
                Interval _16984 = Interval{ 0.0, 0.0 };
                Interval _20834 = jet_mul_derivative(_16983, _16984, intervalFailed, optical_product_upper);
                Interval _16985 = _20834;
                Interval _20835 = jet_add_derivative(_16982, _16985, intervalFailed);
                Interval _16986 = Interval{ 0.0, 0.0 };
                Interval _16987 = Interval{ 7.0, 7.0 };
                Interval _20836 = jet_mul_derivative(_16986, _16987, intervalFailed, optical_product_upper);
                Interval _16988 = _20836;
                Interval _16989 = _20556;
                Interval _16990 = Interval{ 0.0, 0.0 };
                Interval _20837 = jet_mul_derivative(_16989, _16990, intervalFailed, optical_product_upper);
                Interval _16991 = _20837;
                Interval _20838 = jet_add_derivative(_16988, _16991, intervalFailed);
                Interval _16972 = _20825;
                Interval _16973 = _20832;
                Interval _20839 = iadd(_16972, _16973, intervalFailed);
                Interval _16974 = _20828;
                Interval _16975 = _20835;
                Interval _20840 = jet_add_derivative(_16974, _16975, intervalFailed);
                Interval _16976 = _20831;
                Interval _16977 = _20838;
                Interval _20841 = jet_add_derivative(_16976, _16977, intervalFailed);
                if (floor(_20839.lo) == floor(_20839.hi))
                {
                    float _20851 = floor(_20839.lo);
                    Interval _16966 = _20839;
                    Interval _16967 = Interval{ as_type<float>(as_type<uint>(_20851) ^ 2147483648u), as_type<float>(as_type<uint>(_20851) ^ 2147483648u) };
                    _20863 = iadd(_16966, _16967, intervalFailed);
                    Interval _16968 = _20840;
                    Interval _16969 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                    _20865 = jet_add_derivative(_16968, _16969, intervalFailed);
                    Interval _16970 = _20841;
                    Interval _16971 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                    _20867 = jet_add_derivative(_16970, _16971, intervalFailed);
                }
                else
                {
                    jetBranchKnown = false;
                    return OpticalJet3{ OpticalJet{ _20296, _20297, _20298 }, OpticalJet{ _20359, _20360, _20361 }, OpticalJet{ _20422, _20423, _20424 } };
                }
                float _16964 = 3.1415927410125732421875;
                float _20868 = interval_down(_16964, intervalFailed);
                float _16965 = 3.1415927410125732421875;
                float _20869 = interval_up(_16965, intervalFailed);
                Interval _16950 = _20863;
                Interval _16951 = Interval{ _20868, _20869 };
                Interval _20871 = imul(_16950, _16951, intervalFailed, optical_product_upper);
                Interval _16952 = _20865;
                Interval _16953 = Interval{ _20868, _20869 };
                Interval _20873 = jet_mul_derivative(_16952, _16953, intervalFailed, optical_product_upper);
                Interval _16954 = _20873;
                Interval _16955 = _20863;
                Interval _16956 = Interval{ 0.0, 0.0 };
                Interval _20874 = jet_mul_derivative(_16955, _16956, intervalFailed, optical_product_upper);
                Interval _16957 = _20874;
                Interval _20875 = jet_add_derivative(_16954, _16957, intervalFailed);
                Interval _16958 = _20867;
                Interval _16959 = Interval{ _20868, _20869 };
                Interval _20877 = jet_mul_derivative(_16958, _16959, intervalFailed, optical_product_upper);
                Interval _16960 = _20877;
                Interval _16961 = _20863;
                Interval _16962 = Interval{ 0.0, 0.0 };
                Interval _20878 = jet_mul_derivative(_16961, _16962, intervalFailed, optical_product_upper);
                Interval _16963 = _20878;
                Interval _20879 = jet_add_derivative(_16960, _16963, intervalFailed);
                Interval _16942 = _20871;
                float _16940 = 3.1415927410125732421875;
                float _20880 = interval_down(_16940, intervalFailed);
                float _16941 = 3.1415927410125732421875;
                float _20881 = interval_up(_16941, intervalFailed);
                Interval _16943 = Interval{ _20880, _20881 };
                Interval _16944 = Interval{ 0.5, 0.5 };
                Interval _20883 = imul(_16943, _16944, intervalFailed, optical_product_upper);
                Interval _16945 = _20883;
                Interval _20884 = iadd(_16942, _16945, intervalFailed);
                float _16938 = _20884.lo;
                float _16939 = _20884.hi;
                float _20887 = sine_bounds(_16938, _16939, intervalFailed, optical_product_upper, interval_sine_upper);
                float _16936 = _20871.lo;
                float _16937 = _20871.hi;
                float _20891 = sine_bounds(_16936, _16937, intervalFailed, optical_product_upper, interval_sine_upper);
                Interval _16946 = Interval{ _20887, interval_sine_upper };
                Interval _16947 = _20875;
                Interval _20894 = jet_mul_derivative(_16946, _16947, intervalFailed, optical_product_upper);
                Interval _16948 = Interval{ _20887, interval_sine_upper };
                Interval _16949 = _20879;
                Interval _20896 = jet_mul_derivative(_16948, _16949, intervalFailed, optical_product_upper);
                bool _20901;
                if (_20891 <= 0.0)
                {
                    _20901 = interval_sine_upper >= 0.0;
                }
                else
                {
                    _20901 = false;
                }
                float _20908;
                if (_20901)
                {
                    _20908 = 0.0;
                }
                else
                {
                    _20908 = precise::min(abs(_20891), abs(interval_sine_upper));
                }
                float _20911 = precise::max(abs(_20891), abs(interval_sine_upper));
                float _16926 = spvFMul(_20908, _20908);
                float _20912 = interval_down(_16926, intervalFailed);
                float _20913 = precise::max(0.0, _20912);
                float _16927 = spvFMul(_20911, _20911);
                float _20914 = interval_up(_16927, intervalFailed);
                Interval _16928 = Interval{ 2.0, 2.0 };
                Interval _16929 = Interval{ _20891, interval_sine_upper };
                Interval _20916 = imul(_16928, _16929, intervalFailed, optical_product_upper);
                Interval _16930 = _20916;
                Interval _16931 = _20894;
                Interval _20917 = jet_mul_derivative(_16930, _16931, intervalFailed, optical_product_upper);
                Interval _16932 = Interval{ 2.0, 2.0 };
                Interval _16933 = Interval{ _20891, interval_sine_upper };
                Interval _20919 = imul(_16932, _16933, intervalFailed, optical_product_upper);
                Interval _16934 = _20919;
                Interval _16935 = _20896;
                Interval _20920 = jet_mul_derivative(_16934, _16935, intervalFailed, optical_product_upper);
                float _16924 = 55.0;
                float _16925 = 1000.0;
                Interval _20922 = iratio(_16924, _16925, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20927;
                if (!intervalFailed)
                {
                    _20927 = intervalFailed;
                }
                else
                {
                    _20927 = false;
                }
                bool _20932;
                if (_20927)
                {
                    _20932 = jetFailureSite == 0u;
                }
                else
                {
                    _20932 = false;
                }
                if (_20932)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(55.0, 55.0, 1000.0, 1000.0);
                }
                float _16922 = 14.0;
                float _16923 = 100.0;
                Interval _20936 = iratio(_16922, _16923, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20941;
                if (!intervalFailed)
                {
                    _20941 = intervalFailed;
                }
                else
                {
                    _20941 = false;
                }
                bool _20946;
                if (_20941)
                {
                    _20946 = jetFailureSite == 0u;
                }
                else
                {
                    _20946 = false;
                }
                if (_20946)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(14.0, 14.0, 100.0, 100.0);
                }
                Interval _16908 = _20936;
                Interval _16909 = _20863;
                Interval _20949 = imul(_16908, _16909, intervalFailed, optical_product_upper);
                Interval _16910 = Interval{ 0.0, 0.0 };
                Interval _16911 = _20863;
                Interval _20950 = jet_mul_derivative(_16910, _16911, intervalFailed, optical_product_upper);
                Interval _16912 = _20950;
                Interval _16913 = _20936;
                Interval _16914 = _20865;
                Interval _20951 = jet_mul_derivative(_16913, _16914, intervalFailed, optical_product_upper);
                Interval _16915 = _20951;
                Interval _20952 = jet_add_derivative(_16912, _16915, intervalFailed);
                Interval _16916 = Interval{ 0.0, 0.0 };
                Interval _16917 = _20863;
                Interval _20953 = jet_mul_derivative(_16916, _16917, intervalFailed, optical_product_upper);
                Interval _16918 = _20953;
                Interval _16919 = _20936;
                Interval _16920 = _20867;
                Interval _20954 = jet_mul_derivative(_16919, _16920, intervalFailed, optical_product_upper);
                Interval _16921 = _20954;
                Interval _20955 = jet_add_derivative(_16918, _16921, intervalFailed);
                Interval _16902 = _20922;
                Interval _16903 = _20949;
                Interval _20956 = iadd(_16902, _16903, intervalFailed);
                Interval _16904 = Interval{ 0.0, 0.0 };
                Interval _16905 = _20952;
                Interval _20957 = jet_add_derivative(_16904, _16905, intervalFailed);
                Interval _16906 = Interval{ 0.0, 0.0 };
                Interval _16907 = _20955;
                Interval _20958 = jet_add_derivative(_16906, _16907, intervalFailed);
                bool _20965;
                if (_20956.lo <= 0.0)
                {
                    _20965 = _20956.hi >= 0.0;
                }
                else
                {
                    _20965 = false;
                }
                float _20972;
                if (_20965)
                {
                    _20972 = 0.0;
                }
                else
                {
                    _20972 = precise::min(abs(_20956.lo), abs(_20956.hi));
                }
                float _20975 = precise::max(abs(_20956.lo), abs(_20956.hi));
                float _16892 = spvFMul(_20972, _20972);
                float _20976 = interval_down(_16892, intervalFailed);
                float _20977 = precise::max(0.0, _20976);
                float _16893 = spvFMul(_20975, _20975);
                float _20978 = interval_up(_16893, intervalFailed);
                Interval _16894 = Interval{ 2.0, 2.0 };
                Interval _16895 = _20956;
                Interval _20979 = imul(_16894, _16895, intervalFailed, optical_product_upper);
                Interval _16896 = _20979;
                Interval _16897 = _20957;
                Interval _20980 = jet_mul_derivative(_16896, _16897, intervalFailed, optical_product_upper);
                Interval _16898 = Interval{ 2.0, 2.0 };
                Interval _16899 = _20956;
                Interval _20981 = imul(_16898, _16899, intervalFailed, optical_product_upper);
                Interval _16900 = _20981;
                Interval _16901 = _20958;
                Interval _20982 = jet_mul_derivative(_16900, _16901, intervalFailed, optical_product_upper);
                bool _20989;
                if (_20691.lo <= 0.0)
                {
                    _20989 = _20691.hi >= 0.0;
                }
                else
                {
                    _20989 = false;
                }
                float _20996;
                if (_20989)
                {
                    _20996 = 0.0;
                }
                else
                {
                    _20996 = precise::min(abs(_20691.lo), abs(_20691.hi));
                }
                float _20999 = precise::max(abs(_20691.lo), abs(_20691.hi));
                float _16882 = spvFMul(_20996, _20996);
                float _21000 = interval_down(_16882, intervalFailed);
                float _16883 = spvFMul(_20999, _20999);
                float _21002 = interval_up(_16883, intervalFailed);
                Interval _16884 = Interval{ 2.0, 2.0 };
                Interval _16885 = _20691;
                Interval _21003 = imul(_16884, _16885, intervalFailed, optical_product_upper);
                Interval _16886 = _21003;
                Interval _16887 = _20693;
                Interval _21004 = jet_mul_derivative(_16886, _16887, intervalFailed, optical_product_upper);
                Interval _16888 = Interval{ 2.0, 2.0 };
                Interval _16889 = _20691;
                Interval _21005 = imul(_16888, _16889, intervalFailed, optical_product_upper);
                Interval _16890 = _21005;
                Interval _16891 = _20695;
                Interval _21006 = jet_mul_derivative(_16890, _16891, intervalFailed, optical_product_upper);
                bool _21013;
                if (_20802.lo <= 0.0)
                {
                    _21013 = _20802.hi >= 0.0;
                }
                else
                {
                    _21013 = false;
                }
                float _21020;
                if (_21013)
                {
                    _21020 = 0.0;
                }
                else
                {
                    _21020 = precise::min(abs(_20802.lo), abs(_20802.hi));
                }
                float _21023 = precise::max(abs(_20802.lo), abs(_20802.hi));
                float _16872 = spvFMul(_21020, _21020);
                float _21024 = interval_down(_16872, intervalFailed);
                float _16873 = spvFMul(_21023, _21023);
                float _21026 = interval_up(_16873, intervalFailed);
                Interval _16874 = Interval{ 2.0, 2.0 };
                Interval _16875 = _20802;
                Interval _21027 = imul(_16874, _16875, intervalFailed, optical_product_upper);
                Interval _16876 = _21027;
                Interval _16877 = _20804;
                Interval _21028 = jet_mul_derivative(_16876, _16877, intervalFailed, optical_product_upper);
                Interval _16878 = Interval{ 2.0, 2.0 };
                Interval _16879 = _20802;
                Interval _21029 = imul(_16878, _16879, intervalFailed, optical_product_upper);
                Interval _16880 = _21029;
                Interval _16881 = _20806;
                Interval _21030 = jet_mul_derivative(_16880, _16881, intervalFailed, optical_product_upper);
                Interval _16866 = Interval{ precise::max(0.0, _21000), _21002 };
                Interval _16867 = Interval{ precise::max(0.0, _21024), _21026 };
                Interval _21033 = iadd(_16866, _16867, intervalFailed);
                Interval _16868 = _21004;
                Interval _16869 = _21028;
                Interval _21034 = jet_add_derivative(_16868, _16869, intervalFailed);
                Interval _16870 = _21006;
                Interval _16871 = _21030;
                Interval _21035 = jet_add_derivative(_16870, _16871, intervalFailed);
                float _21038 = precise::max(0.0, _21033.lo);
                Interval _16852 = Interval{ _21038, _21033.hi };
                Interval _16853 = Interval{ _20977, _20978 };
                Interval _21042 = idiv(_16852, _16853, intervalFailed, interval_divide_upper);
                bool _21047;
                if (!intervalFailed)
                {
                    _21047 = intervalFailed;
                }
                else
                {
                    _21047 = false;
                }
                bool _21052;
                if (_21047)
                {
                    _21052 = jetFailureSite == 0u;
                }
                else
                {
                    _21052 = false;
                }
                if (_21052)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_21038, _21033.hi, _20977, _20978);
                }
                Interval _16854 = _21034;
                Interval _16855 = _21042;
                Interval _16856 = _20980;
                Interval _21056 = jet_mul_derivative(_16855, _16856, intervalFailed, optical_product_upper);
                Interval _16857 = Interval{ as_type<float>(as_type<uint>(_21056.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21056.lo) ^ 2147483648u) };
                Interval _21066 = jet_add_derivative(_16854, _16857, intervalFailed);
                Interval _16858 = _21066;
                Interval _16859 = Interval{ _20977, _20978 };
                Interval _21068 = jet_div_derivative(_16858, _16859, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16860 = _21035;
                Interval _16861 = _21042;
                Interval _16862 = _20982;
                Interval _21069 = jet_mul_derivative(_16861, _16862, intervalFailed, optical_product_upper);
                Interval _16863 = Interval{ as_type<float>(as_type<uint>(_21069.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21069.lo) ^ 2147483648u) };
                Interval _21079 = jet_add_derivative(_16860, _16863, intervalFailed);
                Interval _16864 = _21079;
                Interval _16865 = Interval{ _20977, _20978 };
                Interval _21081 = jet_div_derivative(_16864, _16865, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16846 = Interval{ 1.0, 1.0 };
                Interval _16847 = Interval{ as_type<float>(as_type<uint>(_21042.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21042.lo) ^ 2147483648u) };
                Interval _21107 = iadd(_16846, _16847, intervalFailed);
                Interval _16848 = Interval{ 0.0, 0.0 };
                Interval _16849 = Interval{ as_type<float>(as_type<uint>(_21068.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21068.lo) ^ 2147483648u) };
                Interval _21109 = jet_add_derivative(_16848, _16849, intervalFailed);
                Interval _16850 = Interval{ 0.0, 0.0 };
                Interval _16851 = Interval{ as_type<float>(as_type<uint>(_21081.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21081.lo) ^ 2147483648u) };
                Interval _21111 = jet_add_derivative(_16850, _16851, intervalFailed);
                OpticalJet _16842 = OpticalJet{ _21107, _21109, _21111 };
                OpticalJet _16843 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _16844 = jmax(_16842, _16843);
                OpticalJet _16845 = OpticalJet{ Interval{ 1.0, 1.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _21114 = jmin(_16844, _16845);
                Interval _16828 = Interval{ 10.0, 10.0 };
                Interval _16829 = Interval{ _20913, _20914 };
                Interval _21116 = imul(_16828, _16829, intervalFailed, optical_product_upper);
                Interval _16830 = Interval{ 0.0, 0.0 };
                Interval _16831 = Interval{ _20913, _20914 };
                Interval _21118 = jet_mul_derivative(_16830, _16831, intervalFailed, optical_product_upper);
                Interval _16832 = _21118;
                Interval _16833 = Interval{ 10.0, 10.0 };
                Interval _16834 = _20917;
                Interval _21119 = jet_mul_derivative(_16833, _16834, intervalFailed, optical_product_upper);
                Interval _16835 = _21119;
                Interval _21120 = jet_add_derivative(_16832, _16835, intervalFailed);
                Interval _16836 = Interval{ 0.0, 0.0 };
                Interval _16837 = Interval{ _20913, _20914 };
                Interval _21122 = jet_mul_derivative(_16836, _16837, intervalFailed, optical_product_upper);
                Interval _16838 = _21122;
                Interval _16839 = Interval{ 10.0, 10.0 };
                Interval _16840 = _20920;
                Interval _21123 = jet_mul_derivative(_16839, _16840, intervalFailed, optical_product_upper);
                Interval _16841 = _21123;
                Interval _21124 = jet_add_derivative(_16838, _16841, intervalFailed);
                float _16826 = 18.0;
                float _16827 = 100.0;
                Interval _21127 = iratio(_16826, _16827, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21132;
                if (!intervalFailed)
                {
                    _21132 = intervalFailed;
                }
                else
                {
                    _21132 = false;
                }
                bool _21137;
                if (_21132)
                {
                    _21137 = jetFailureSite == 0u;
                }
                else
                {
                    _21137 = false;
                }
                if (_21137)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(18.0, 18.0, 100.0, 100.0);
                }
                Interval _16812 = footprint.v;
                Interval _16813 = _21127;
                Interval _21143 = imul(_16812, _16813, intervalFailed, optical_product_upper);
                Interval _16814 = footprint.dx;
                Interval _16815 = _21127;
                Interval _21144 = jet_mul_derivative(_16814, _16815, intervalFailed, optical_product_upper);
                Interval _16816 = _21144;
                Interval _16817 = footprint.v;
                Interval _16818 = Interval{ 0.0, 0.0 };
                Interval _21145 = jet_mul_derivative(_16817, _16818, intervalFailed, optical_product_upper);
                Interval _16819 = _21145;
                Interval _21146 = jet_add_derivative(_16816, _16819, intervalFailed);
                Interval _16820 = footprint.dy;
                Interval _16821 = _21127;
                Interval _21147 = jet_mul_derivative(_16820, _16821, intervalFailed, optical_product_upper);
                Interval _16822 = _21147;
                Interval _16823 = footprint.v;
                Interval _16824 = Interval{ 0.0, 0.0 };
                Interval _21148 = jet_mul_derivative(_16823, _16824, intervalFailed, optical_product_upper);
                Interval _16825 = _21148;
                Interval _21149 = jet_add_derivative(_16822, _16825, intervalFailed);
                bool _21156;
                if (_21143.lo <= 0.0)
                {
                    _21156 = _21143.hi >= 0.0;
                }
                else
                {
                    _21156 = false;
                }
                float _21163;
                if (_21156)
                {
                    _21163 = 0.0;
                }
                else
                {
                    _21163 = precise::min(abs(_21143.lo), abs(_21143.hi));
                }
                float _21166 = precise::max(abs(_21143.lo), abs(_21143.hi));
                float _16802 = spvFMul(_21163, _21163);
                float _21167 = interval_down(_16802, intervalFailed);
                float _21168 = precise::max(0.0, _21167);
                float _16803 = spvFMul(_21166, _21166);
                float _21169 = interval_up(_16803, intervalFailed);
                Interval _16804 = Interval{ 2.0, 2.0 };
                Interval _16805 = _21143;
                Interval _21170 = imul(_16804, _16805, intervalFailed, optical_product_upper);
                Interval _16806 = _21170;
                Interval _16807 = _21146;
                Interval _21171 = jet_mul_derivative(_16806, _16807, intervalFailed, optical_product_upper);
                Interval _16808 = Interval{ 2.0, 2.0 };
                Interval _16809 = _21143;
                Interval _21172 = imul(_16808, _16809, intervalFailed, optical_product_upper);
                Interval _16810 = _21172;
                Interval _16811 = _21149;
                Interval _21173 = jet_mul_derivative(_16810, _16811, intervalFailed, optical_product_upper);
                bool _21178;
                if (_21168 <= 0.0)
                {
                    _21178 = _21169 >= 0.0;
                }
                else
                {
                    _21178 = false;
                }
                float _21185;
                if (_21178)
                {
                    _21185 = 0.0;
                }
                else
                {
                    _21185 = precise::min(abs(_21168), abs(_21169));
                }
                float _21188 = precise::max(abs(_21168), abs(_21169));
                float _16792 = spvFMul(_21185, _21185);
                float _21189 = interval_down(_16792, intervalFailed);
                float _16793 = spvFMul(_21188, _21188);
                float _21191 = interval_up(_16793, intervalFailed);
                Interval _16794 = Interval{ 2.0, 2.0 };
                Interval _16795 = Interval{ _21168, _21169 };
                Interval _21193 = imul(_16794, _16795, intervalFailed, optical_product_upper);
                Interval _16796 = _21193;
                Interval _16797 = _21171;
                Interval _21194 = jet_mul_derivative(_16796, _16797, intervalFailed, optical_product_upper);
                Interval _16798 = Interval{ 2.0, 2.0 };
                Interval _16799 = Interval{ _21168, _21169 };
                Interval _21196 = imul(_16798, _16799, intervalFailed, optical_product_upper);
                Interval _16800 = _21196;
                Interval _16801 = _21173;
                Interval _21197 = jet_mul_derivative(_16800, _16801, intervalFailed, optical_product_upper);
                Interval _16786 = Interval{ 1.0, 1.0 };
                Interval _16787 = Interval{ precise::max(0.0, _21189), _21191 };
                Interval _21199 = iadd(_16786, _16787, intervalFailed);
                Interval _16788 = Interval{ 0.0, 0.0 };
                Interval _16789 = _21194;
                Interval _21200 = jet_add_derivative(_16788, _16789, intervalFailed);
                Interval _16790 = Interval{ 0.0, 0.0 };
                Interval _16791 = _21197;
                Interval _21201 = jet_add_derivative(_16790, _16791, intervalFailed);
                Interval _16772 = Interval{ 1.0, 1.0 };
                Interval _16773 = _21199;
                Interval _21203 = idiv(_16772, _16773, intervalFailed, interval_divide_upper);
                bool _21210;
                if (!intervalFailed)
                {
                    _21210 = intervalFailed;
                }
                else
                {
                    _21210 = false;
                }
                bool _21215;
                if (_21210)
                {
                    _21215 = jetFailureSite == 0u;
                }
                else
                {
                    _21215 = false;
                }
                if (_21215)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(1.0, 1.0, _21199.lo, _21199.hi);
                }
                Interval _16774 = Interval{ 0.0, 0.0 };
                Interval _16775 = _21203;
                Interval _16776 = _21200;
                Interval _21219 = jet_mul_derivative(_16775, _16776, intervalFailed, optical_product_upper);
                Interval _16777 = Interval{ as_type<float>(as_type<uint>(_21219.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21219.lo) ^ 2147483648u) };
                Interval _21229 = jet_add_derivative(_16774, _16777, intervalFailed);
                Interval _16778 = _21229;
                Interval _16779 = _21199;
                Interval _21230 = jet_div_derivative(_16778, _16779, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16780 = Interval{ 0.0, 0.0 };
                Interval _16781 = _21203;
                Interval _16782 = _21201;
                Interval _21231 = jet_mul_derivative(_16781, _16782, intervalFailed, optical_product_upper);
                Interval _16783 = Interval{ as_type<float>(as_type<uint>(_21231.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21231.lo) ^ 2147483648u) };
                Interval _21241 = jet_add_derivative(_16780, _16783, intervalFailed);
                Interval _16784 = _21241;
                Interval _16785 = _21199;
                Interval _21242 = jet_div_derivative(_16784, _16785, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16758 = _21116;
                Interval _16759 = _21203;
                Interval _21243 = imul(_16758, _16759, intervalFailed, optical_product_upper);
                Interval _16760 = _21120;
                Interval _16761 = _21203;
                Interval _21244 = jet_mul_derivative(_16760, _16761, intervalFailed, optical_product_upper);
                Interval _16762 = _21244;
                Interval _16763 = _21116;
                Interval _16764 = _21230;
                Interval _21245 = jet_mul_derivative(_16763, _16764, intervalFailed, optical_product_upper);
                Interval _16765 = _21245;
                Interval _21246 = jet_add_derivative(_16762, _16765, intervalFailed);
                Interval _16766 = _21124;
                Interval _16767 = _21203;
                Interval _21247 = jet_mul_derivative(_16766, _16767, intervalFailed, optical_product_upper);
                Interval _16768 = _21247;
                Interval _16769 = _21116;
                Interval _16770 = _21242;
                Interval _21248 = jet_mul_derivative(_16769, _16770, intervalFailed, optical_product_upper);
                Interval _16771 = _21248;
                Interval _21249 = jet_add_derivative(_16768, _16771, intervalFailed);
                Interval _21250 = _21114.v;
                float _21253 = _21250.lo;
                float _21254 = _21250.hi;
                bool _21259;
                if (_21253 <= 0.0)
                {
                    _21259 = _21254 >= 0.0;
                }
                else
                {
                    _21259 = false;
                }
                float _21266;
                if (_21259)
                {
                    _21266 = 0.0;
                }
                else
                {
                    _21266 = precise::min(abs(_21253), abs(_21254));
                }
                float _21269 = precise::max(abs(_21253), abs(_21254));
                float _16748 = spvFMul(_21266, _21266);
                float _21270 = interval_down(_16748, intervalFailed);
                float _21271 = precise::max(0.0, _21270);
                float _16749 = spvFMul(_21269, _21269);
                float _21272 = interval_up(_16749, intervalFailed);
                Interval _16750 = Interval{ 2.0, 2.0 };
                Interval _16751 = _21250;
                Interval _21273 = imul(_16750, _16751, intervalFailed, optical_product_upper);
                Interval _16752 = _21273;
                Interval _16753 = _21114.dx;
                Interval _21274 = jet_mul_derivative(_16752, _16753, intervalFailed, optical_product_upper);
                Interval _16754 = Interval{ 2.0, 2.0 };
                Interval _16755 = _21250;
                Interval _21275 = imul(_16754, _16755, intervalFailed, optical_product_upper);
                Interval _16756 = _21275;
                Interval _16757 = _21114.dy;
                Interval _21276 = jet_mul_derivative(_16756, _16757, intervalFailed, optical_product_upper);
                Interval _16734 = _21243;
                Interval _16735 = Interval{ _21271, _21272 };
                Interval _21278 = imul(_16734, _16735, intervalFailed, optical_product_upper);
                Interval _16736 = _21246;
                Interval _16737 = Interval{ _21271, _21272 };
                Interval _21280 = jet_mul_derivative(_16736, _16737, intervalFailed, optical_product_upper);
                Interval _16738 = _21280;
                Interval _16739 = _21243;
                Interval _16740 = _21274;
                Interval _21281 = jet_mul_derivative(_16739, _16740, intervalFailed, optical_product_upper);
                Interval _16741 = _21281;
                Interval _21282 = jet_add_derivative(_16738, _16741, intervalFailed);
                Interval _16742 = _21249;
                Interval _16743 = Interval{ _21271, _21272 };
                Interval _21284 = jet_mul_derivative(_16742, _16743, intervalFailed, optical_product_upper);
                Interval _16744 = _21284;
                Interval _16745 = _21243;
                Interval _16746 = _21276;
                Interval _21285 = jet_mul_derivative(_16745, _16746, intervalFailed, optical_product_upper);
                Interval _16747 = _21285;
                Interval _21286 = jet_add_derivative(_16744, _16747, intervalFailed);
                Interval _21287 = _21114.v;
                Interval _16720 = _21278;
                Interval _16721 = _21287;
                Interval _21290 = imul(_16720, _16721, intervalFailed, optical_product_upper);
                Interval _16722 = _21282;
                Interval _16723 = _21287;
                Interval _21291 = jet_mul_derivative(_16722, _16723, intervalFailed, optical_product_upper);
                Interval _16724 = _21291;
                Interval _16725 = _21278;
                Interval _16726 = _21114.dx;
                Interval _21292 = jet_mul_derivative(_16725, _16726, intervalFailed, optical_product_upper);
                Interval _16727 = _21292;
                Interval _21293 = jet_add_derivative(_16724, _16727, intervalFailed);
                Interval _16728 = _21286;
                Interval _16729 = _21287;
                Interval _21294 = jet_mul_derivative(_16728, _16729, intervalFailed, optical_product_upper);
                Interval _16730 = _21294;
                Interval _16731 = _21278;
                Interval _16732 = _21114.dy;
                Interval _21295 = jet_mul_derivative(_16731, _16732, intervalFailed, optical_product_upper);
                Interval _16733 = _21295;
                Interval _21296 = jet_add_derivative(_16730, _16733, intervalFailed);
                Interval _16706 = Interval{ -6.0, -6.0 };
                Interval _16707 = _21243;
                Interval _21297 = imul(_16706, _16707, intervalFailed, optical_product_upper);
                Interval _16708 = Interval{ 0.0, 0.0 };
                Interval _16709 = _21243;
                Interval _21298 = jet_mul_derivative(_16708, _16709, intervalFailed, optical_product_upper);
                Interval _16710 = _21298;
                Interval _16711 = Interval{ -6.0, -6.0 };
                Interval _16712 = _21246;
                Interval _21299 = jet_mul_derivative(_16711, _16712, intervalFailed, optical_product_upper);
                Interval _16713 = _21299;
                Interval _21300 = jet_add_derivative(_16710, _16713, intervalFailed);
                Interval _16714 = Interval{ 0.0, 0.0 };
                Interval _16715 = _21243;
                Interval _21301 = jet_mul_derivative(_16714, _16715, intervalFailed, optical_product_upper);
                Interval _16716 = _21301;
                Interval _16717 = Interval{ -6.0, -6.0 };
                Interval _16718 = _21249;
                Interval _21302 = jet_mul_derivative(_16717, _16718, intervalFailed, optical_product_upper);
                Interval _16719 = _21302;
                Interval _21303 = jet_add_derivative(_16716, _16719, intervalFailed);
                Interval _16692 = _21297;
                Interval _16693 = Interval{ _21271, _21272 };
                Interval _21305 = imul(_16692, _16693, intervalFailed, optical_product_upper);
                Interval _16694 = _21300;
                Interval _16695 = Interval{ _21271, _21272 };
                Interval _21307 = jet_mul_derivative(_16694, _16695, intervalFailed, optical_product_upper);
                Interval _16696 = _21307;
                Interval _16697 = _21297;
                Interval _16698 = _21274;
                Interval _21308 = jet_mul_derivative(_16697, _16698, intervalFailed, optical_product_upper);
                Interval _16699 = _21308;
                Interval _21309 = jet_add_derivative(_16696, _16699, intervalFailed);
                Interval _16700 = _21303;
                Interval _16701 = Interval{ _21271, _21272 };
                Interval _21311 = jet_mul_derivative(_16700, _16701, intervalFailed, optical_product_upper);
                Interval _16702 = _21311;
                Interval _16703 = _21297;
                Interval _16704 = _21276;
                Interval _21312 = jet_mul_derivative(_16703, _16704, intervalFailed, optical_product_upper);
                Interval _16705 = _21312;
                Interval _21313 = jet_add_derivative(_16702, _16705, intervalFailed);
                Interval _16678 = Interval{ 128.0, 128.0 };
                Interval _16679 = Interval{ _20977, _20978 };
                Interval _21315 = imul(_16678, _16679, intervalFailed, optical_product_upper);
                Interval _16680 = Interval{ 0.0, 0.0 };
                Interval _16681 = Interval{ _20977, _20978 };
                Interval _21317 = jet_mul_derivative(_16680, _16681, intervalFailed, optical_product_upper);
                Interval _16682 = _21317;
                Interval _16683 = Interval{ 128.0, 128.0 };
                Interval _16684 = _20980;
                Interval _21318 = jet_mul_derivative(_16683, _16684, intervalFailed, optical_product_upper);
                Interval _16685 = _21318;
                Interval _21319 = jet_add_derivative(_16682, _16685, intervalFailed);
                Interval _16686 = Interval{ 0.0, 0.0 };
                Interval _16687 = Interval{ _20977, _20978 };
                Interval _21321 = jet_mul_derivative(_16686, _16687, intervalFailed, optical_product_upper);
                Interval _16688 = _21321;
                Interval _16689 = Interval{ 128.0, 128.0 };
                Interval _16690 = _20982;
                Interval _21322 = jet_mul_derivative(_16689, _16690, intervalFailed, optical_product_upper);
                Interval _16691 = _21322;
                Interval _21323 = jet_add_derivative(_16688, _16691, intervalFailed);
                Interval _16664 = _21305;
                Interval _16665 = _21315;
                Interval _21325 = idiv(_16664, _16665, intervalFailed, interval_divide_upper);
                bool _21334;
                if (!intervalFailed)
                {
                    _21334 = intervalFailed;
                }
                else
                {
                    _21334 = false;
                }
                bool _21339;
                if (_21334)
                {
                    _21339 = jetFailureSite == 0u;
                }
                else
                {
                    _21339 = false;
                }
                if (_21339)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_21305.lo, _21305.hi, _21315.lo, _21315.hi);
                }
                Interval _16666 = _21309;
                Interval _16667 = _21325;
                Interval _16668 = _21319;
                Interval _21343 = jet_mul_derivative(_16667, _16668, intervalFailed, optical_product_upper);
                Interval _16669 = Interval{ as_type<float>(as_type<uint>(_21343.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21343.lo) ^ 2147483648u) };
                Interval _21353 = jet_add_derivative(_16666, _16669, intervalFailed);
                Interval _16670 = _21353;
                Interval _16671 = _21315;
                Interval _21354 = jet_div_derivative(_16670, _16671, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16672 = _21313;
                Interval _16673 = _21325;
                Interval _16674 = _21323;
                Interval _21355 = jet_mul_derivative(_16673, _16674, intervalFailed, optical_product_upper);
                Interval _16675 = Interval{ as_type<float>(as_type<uint>(_21355.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21355.lo) ^ 2147483648u) };
                Interval _21365 = jet_add_derivative(_16672, _16675, intervalFailed);
                Interval _16676 = _21365;
                Interval _16677 = _21315;
                Interval _21366 = jet_div_derivative(_16676, _16677, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16650 = _21325;
                Interval _16651 = _20691;
                Interval _21367 = imul(_16650, _16651, intervalFailed, optical_product_upper);
                Interval _16652 = _21354;
                Interval _16653 = _20691;
                Interval _21368 = jet_mul_derivative(_16652, _16653, intervalFailed, optical_product_upper);
                Interval _16654 = _21368;
                Interval _16655 = _21325;
                Interval _16656 = _20693;
                Interval _21369 = jet_mul_derivative(_16655, _16656, intervalFailed, optical_product_upper);
                Interval _16657 = _21369;
                Interval _21370 = jet_add_derivative(_16654, _16657, intervalFailed);
                Interval _16658 = _21366;
                Interval _16659 = _20691;
                Interval _21371 = jet_mul_derivative(_16658, _16659, intervalFailed, optical_product_upper);
                Interval _16660 = _21371;
                Interval _16661 = _21325;
                Interval _16662 = _20695;
                Interval _21372 = jet_mul_derivative(_16661, _16662, intervalFailed, optical_product_upper);
                Interval _16663 = _21372;
                Interval _21373 = jet_add_derivative(_16660, _16663, intervalFailed);
                Interval _16636 = _21325;
                Interval _16637 = _20802;
                Interval _21374 = imul(_16636, _16637, intervalFailed, optical_product_upper);
                Interval _16638 = _21354;
                Interval _16639 = _20802;
                Interval _21375 = jet_mul_derivative(_16638, _16639, intervalFailed, optical_product_upper);
                Interval _16640 = _21375;
                Interval _16641 = _21325;
                Interval _16642 = _20804;
                Interval _21376 = jet_mul_derivative(_16641, _16642, intervalFailed, optical_product_upper);
                Interval _16643 = _21376;
                Interval _21377 = jet_add_derivative(_16640, _16643, intervalFailed);
                Interval _16644 = _21366;
                Interval _16645 = _20802;
                Interval _21378 = jet_mul_derivative(_16644, _16645, intervalFailed, optical_product_upper);
                Interval _16646 = _21378;
                Interval _16647 = _21325;
                Interval _16648 = _20806;
                Interval _21379 = jet_mul_derivative(_16647, _16648, intervalFailed, optical_product_upper);
                Interval _16649 = _21379;
                Interval _21380 = jet_add_derivative(_16646, _16649, intervalFailed);
                Interval _16630 = _20296;
                Interval _16631 = _21290;
                Interval _21381 = iadd(_16630, _16631, intervalFailed);
                Interval _16632 = _20297;
                Interval _16633 = _21293;
                Interval _21382 = jet_add_derivative(_16632, _16633, intervalFailed);
                Interval _16634 = _20298;
                Interval _16635 = _21296;
                Interval _21383 = jet_add_derivative(_16634, _16635, intervalFailed);
                Interval _16624 = _20359;
                Interval _16625 = _21367;
                Interval _21390 = iadd(_16624, _16625, intervalFailed);
                Interval _16626 = _20360;
                Interval _16627 = _21370;
                Interval _21391 = jet_add_derivative(_16626, _16627, intervalFailed);
                Interval _16628 = _20361;
                Interval _16629 = _21373;
                Interval _21392 = jet_add_derivative(_16628, _16629, intervalFailed);
                Interval _16618 = _20422;
                Interval _16619 = _21374;
                Interval _21399 = iadd(_16618, _16619, intervalFailed);
                Interval _16620 = _20423;
                Interval _16621 = _21377;
                Interval _21400 = jet_add_derivative(_16620, _16621, intervalFailed);
                Interval _16622 = _20424;
                Interval _16623 = _21380;
                Interval _21401 = jet_add_derivative(_16622, _16623, intervalFailed);
                _21408 = _21399.lo;
                _21409 = _21399.hi;
                _21410 = _21400.lo;
                _21411 = _21400.hi;
                _21412 = _21401.lo;
                _21413 = _21401.hi;
                _21414 = _21390.lo;
                _21415 = _21390.hi;
                _21416 = _21391.lo;
                _21417 = _21391.hi;
                _21418 = _21392.lo;
                _21419 = _21392.hi;
                _21420 = _21381.lo;
                _21421 = _21381.hi;
                _21422 = _21382.lo;
                _21423 = _21382.hi;
                _21424 = _21383.lo;
                _21425 = _21383.hi;
            }
            else
            {
                _21408 = _20422.lo;
                _21409 = _20422.hi;
                _21410 = _20423.lo;
                _21411 = _20423.hi;
                _21412 = _20424.lo;
                _21413 = _20424.hi;
                _21414 = _20359.lo;
                _21415 = _20359.hi;
                _21416 = _20360.lo;
                _21417 = _20360.hi;
                _21418 = _20361.lo;
                _21419 = _20361.hi;
                _21420 = _20296.lo;
                _21421 = _20296.hi;
                _21422 = _20297.lo;
                _21423 = _20297.hi;
                _21424 = _20298.lo;
                _21425 = _20298.hi;
            }
        }
        _21430 = _21408;
        _21431 = _21409;
        _21432 = _21410;
        _21433 = _21411;
        _21434 = _21412;
        _21435 = _21413;
        _21436 = _21414;
        _21437 = _21415;
        _21438 = _21416;
        _21439 = _21417;
        _21440 = _21418;
        _21441 = _21419;
        _21442 = _21420;
        _21443 = _21421;
        _21444 = _21422;
        _21445 = _21423;
        _21446 = _21424;
        _21447 = _21425;
    }
    else
    {
        _21430 = _20422.lo;
        _21431 = _20422.hi;
        _21432 = _20423.lo;
        _21433 = _20423.hi;
        _21434 = _20424.lo;
        _21435 = _20424.hi;
        _21436 = _20359.lo;
        _21437 = _20359.hi;
        _21438 = _20360.lo;
        _21439 = _20360.hi;
        _21440 = _20361.lo;
        _21441 = _20361.hi;
        _21442 = _20296.lo;
        _21443 = _20296.hi;
        _21444 = _20297.lo;
        _21445 = _20297.hi;
        _21446 = _20298.lo;
        _21447 = _20298.hi;
    }
    return OpticalJet3{ OpticalJet{ Interval{ _21442, _21443 }, Interval{ _21444, _21445 }, Interval{ _21446, _21447 } }, OpticalJet{ Interval{ _21436, _21437 }, Interval{ _21438, _21439 }, Interval{ _21440, _21441 } }, OpticalJet{ Interval{ _21430, _21431 }, Interval{ _21432, _21433 }, Interval{ _21434, _21435 } } };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_liquid_normal(thread const ReflectionLiquidFrame& f, thread const OpticalJet3& direction, thread const OpticalJet& _distance, thread const OpticalJet& footprint, thread OpticalJet3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    Interval _13439 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13440 = hit.x.v;
    Interval _13468 = imul(_13439, _13440, intervalFailed, optical_product_upper);
    Interval _13441 = Interval{ 0.0, 0.0 };
    Interval _13442 = hit.x.v;
    Interval _13469 = jet_mul_derivative(_13441, _13442, intervalFailed, optical_product_upper);
    Interval _13443 = _13469;
    Interval _13444 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13445 = hit.x.dx;
    Interval _13471 = jet_mul_derivative(_13444, _13445, intervalFailed, optical_product_upper);
    Interval _13446 = _13471;
    Interval _13472 = jet_add_derivative(_13443, _13446, intervalFailed);
    Interval _13447 = Interval{ 0.0, 0.0 };
    Interval _13448 = hit.x.v;
    Interval _13473 = jet_mul_derivative(_13447, _13448, intervalFailed, optical_product_upper);
    Interval _13449 = _13473;
    Interval _13450 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13451 = hit.x.dy;
    Interval _13475 = jet_mul_derivative(_13450, _13451, intervalFailed, optical_product_upper);
    Interval _13452 = _13475;
    Interval _13476 = jet_add_derivative(_13449, _13452, intervalFailed);
    Interval _13425 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13426 = hit.x.v;
    Interval _13481 = imul(_13425, _13426, intervalFailed, optical_product_upper);
    Interval _13427 = Interval{ 0.0, 0.0 };
    Interval _13428 = hit.x.v;
    Interval _13482 = jet_mul_derivative(_13427, _13428, intervalFailed, optical_product_upper);
    Interval _13429 = _13482;
    Interval _13430 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13431 = hit.x.dx;
    Interval _13484 = jet_mul_derivative(_13430, _13431, intervalFailed, optical_product_upper);
    Interval _13432 = _13484;
    Interval _13485 = jet_add_derivative(_13429, _13432, intervalFailed);
    Interval _13433 = Interval{ 0.0, 0.0 };
    Interval _13434 = hit.x.v;
    Interval _13486 = jet_mul_derivative(_13433, _13434, intervalFailed, optical_product_upper);
    Interval _13435 = _13486;
    Interval _13436 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13437 = hit.x.dy;
    Interval _13488 = jet_mul_derivative(_13436, _13437, intervalFailed, optical_product_upper);
    Interval _13438 = _13488;
    Interval _13489 = jet_add_derivative(_13435, _13438, intervalFailed);
    Interval _13411 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13412 = hit.x.v;
    Interval _13494 = imul(_13411, _13412, intervalFailed, optical_product_upper);
    Interval _13413 = Interval{ 0.0, 0.0 };
    Interval _13414 = hit.x.v;
    Interval _13495 = jet_mul_derivative(_13413, _13414, intervalFailed, optical_product_upper);
    Interval _13415 = _13495;
    Interval _13416 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13417 = hit.x.dx;
    Interval _13497 = jet_mul_derivative(_13416, _13417, intervalFailed, optical_product_upper);
    Interval _13418 = _13497;
    Interval _13498 = jet_add_derivative(_13415, _13418, intervalFailed);
    Interval _13419 = Interval{ 0.0, 0.0 };
    Interval _13420 = hit.x.v;
    Interval _13499 = jet_mul_derivative(_13419, _13420, intervalFailed, optical_product_upper);
    Interval _13421 = _13499;
    Interval _13422 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13423 = hit.x.dy;
    Interval _13501 = jet_mul_derivative(_13422, _13423, intervalFailed, optical_product_upper);
    Interval _13424 = _13501;
    Interval _13502 = jet_add_derivative(_13421, _13424, intervalFailed);
    Interval _13397 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13398 = hit.y.v;
    Interval _13510 = imul(_13397, _13398, intervalFailed, optical_product_upper);
    Interval _13399 = Interval{ 0.0, 0.0 };
    Interval _13400 = hit.y.v;
    Interval _13511 = jet_mul_derivative(_13399, _13400, intervalFailed, optical_product_upper);
    Interval _13401 = _13511;
    Interval _13402 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13403 = hit.y.dx;
    Interval _13513 = jet_mul_derivative(_13402, _13403, intervalFailed, optical_product_upper);
    Interval _13404 = _13513;
    Interval _13514 = jet_add_derivative(_13401, _13404, intervalFailed);
    Interval _13405 = Interval{ 0.0, 0.0 };
    Interval _13406 = hit.y.v;
    Interval _13515 = jet_mul_derivative(_13405, _13406, intervalFailed, optical_product_upper);
    Interval _13407 = _13515;
    Interval _13408 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13409 = hit.y.dy;
    Interval _13517 = jet_mul_derivative(_13408, _13409, intervalFailed, optical_product_upper);
    Interval _13410 = _13517;
    Interval _13518 = jet_add_derivative(_13407, _13410, intervalFailed);
    Interval _13383 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13384 = hit.y.v;
    Interval _13523 = imul(_13383, _13384, intervalFailed, optical_product_upper);
    Interval _13385 = Interval{ 0.0, 0.0 };
    Interval _13386 = hit.y.v;
    Interval _13524 = jet_mul_derivative(_13385, _13386, intervalFailed, optical_product_upper);
    Interval _13387 = _13524;
    Interval _13388 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13389 = hit.y.dx;
    Interval _13526 = jet_mul_derivative(_13388, _13389, intervalFailed, optical_product_upper);
    Interval _13390 = _13526;
    Interval _13527 = jet_add_derivative(_13387, _13390, intervalFailed);
    Interval _13391 = Interval{ 0.0, 0.0 };
    Interval _13392 = hit.y.v;
    Interval _13528 = jet_mul_derivative(_13391, _13392, intervalFailed, optical_product_upper);
    Interval _13393 = _13528;
    Interval _13394 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13395 = hit.y.dy;
    Interval _13530 = jet_mul_derivative(_13394, _13395, intervalFailed, optical_product_upper);
    Interval _13396 = _13530;
    Interval _13531 = jet_add_derivative(_13393, _13396, intervalFailed);
    Interval _13369 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13370 = hit.y.v;
    Interval _13536 = imul(_13369, _13370, intervalFailed, optical_product_upper);
    Interval _13371 = Interval{ 0.0, 0.0 };
    Interval _13372 = hit.y.v;
    Interval _13537 = jet_mul_derivative(_13371, _13372, intervalFailed, optical_product_upper);
    Interval _13373 = _13537;
    Interval _13374 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13375 = hit.y.dx;
    Interval _13539 = jet_mul_derivative(_13374, _13375, intervalFailed, optical_product_upper);
    Interval _13376 = _13539;
    Interval _13540 = jet_add_derivative(_13373, _13376, intervalFailed);
    Interval _13377 = Interval{ 0.0, 0.0 };
    Interval _13378 = hit.y.v;
    Interval _13541 = jet_mul_derivative(_13377, _13378, intervalFailed, optical_product_upper);
    Interval _13379 = _13541;
    Interval _13380 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13381 = hit.y.dy;
    Interval _13543 = jet_mul_derivative(_13380, _13381, intervalFailed, optical_product_upper);
    Interval _13382 = _13543;
    Interval _13544 = jet_add_derivative(_13379, _13382, intervalFailed);
    Interval _13363 = _13468;
    Interval _13364 = _13510;
    Interval _13545 = iadd(_13363, _13364, intervalFailed);
    Interval _13365 = _13472;
    Interval _13366 = _13514;
    Interval _13546 = jet_add_derivative(_13365, _13366, intervalFailed);
    Interval _13367 = _13476;
    Interval _13368 = _13518;
    Interval _13547 = jet_add_derivative(_13367, _13368, intervalFailed);
    Interval _13357 = _13481;
    Interval _13358 = _13523;
    Interval _13548 = iadd(_13357, _13358, intervalFailed);
    Interval _13359 = _13485;
    Interval _13360 = _13527;
    Interval _13549 = jet_add_derivative(_13359, _13360, intervalFailed);
    Interval _13361 = _13489;
    Interval _13362 = _13531;
    Interval _13550 = jet_add_derivative(_13361, _13362, intervalFailed);
    Interval _13351 = _13494;
    Interval _13352 = _13536;
    Interval _13551 = iadd(_13351, _13352, intervalFailed);
    Interval _13353 = _13498;
    Interval _13354 = _13540;
    Interval _13552 = jet_add_derivative(_13353, _13354, intervalFailed);
    Interval _13355 = _13502;
    Interval _13356 = _13544;
    Interval _13553 = jet_add_derivative(_13355, _13356, intervalFailed);
    Interval _13337 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13338 = hit.z.v;
    Interval _13561 = imul(_13337, _13338, intervalFailed, optical_product_upper);
    Interval _13339 = Interval{ 0.0, 0.0 };
    Interval _13340 = hit.z.v;
    Interval _13562 = jet_mul_derivative(_13339, _13340, intervalFailed, optical_product_upper);
    Interval _13341 = _13562;
    Interval _13342 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13343 = hit.z.dx;
    Interval _13564 = jet_mul_derivative(_13342, _13343, intervalFailed, optical_product_upper);
    Interval _13344 = _13564;
    Interval _13565 = jet_add_derivative(_13341, _13344, intervalFailed);
    Interval _13345 = Interval{ 0.0, 0.0 };
    Interval _13346 = hit.z.v;
    Interval _13566 = jet_mul_derivative(_13345, _13346, intervalFailed, optical_product_upper);
    Interval _13347 = _13566;
    Interval _13348 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13349 = hit.z.dy;
    Interval _13568 = jet_mul_derivative(_13348, _13349, intervalFailed, optical_product_upper);
    Interval _13350 = _13568;
    Interval _13569 = jet_add_derivative(_13347, _13350, intervalFailed);
    Interval _13323 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13324 = hit.z.v;
    Interval _13574 = imul(_13323, _13324, intervalFailed, optical_product_upper);
    Interval _13325 = Interval{ 0.0, 0.0 };
    Interval _13326 = hit.z.v;
    Interval _13575 = jet_mul_derivative(_13325, _13326, intervalFailed, optical_product_upper);
    Interval _13327 = _13575;
    Interval _13328 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13329 = hit.z.dx;
    Interval _13577 = jet_mul_derivative(_13328, _13329, intervalFailed, optical_product_upper);
    Interval _13330 = _13577;
    Interval _13578 = jet_add_derivative(_13327, _13330, intervalFailed);
    Interval _13331 = Interval{ 0.0, 0.0 };
    Interval _13332 = hit.z.v;
    Interval _13579 = jet_mul_derivative(_13331, _13332, intervalFailed, optical_product_upper);
    Interval _13333 = _13579;
    Interval _13334 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13335 = hit.z.dy;
    Interval _13581 = jet_mul_derivative(_13334, _13335, intervalFailed, optical_product_upper);
    Interval _13336 = _13581;
    Interval _13582 = jet_add_derivative(_13333, _13336, intervalFailed);
    Interval _13309 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13310 = hit.z.v;
    Interval _13587 = imul(_13309, _13310, intervalFailed, optical_product_upper);
    Interval _13311 = Interval{ 0.0, 0.0 };
    Interval _13312 = hit.z.v;
    Interval _13588 = jet_mul_derivative(_13311, _13312, intervalFailed, optical_product_upper);
    Interval _13313 = _13588;
    Interval _13314 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13315 = hit.z.dx;
    Interval _13590 = jet_mul_derivative(_13314, _13315, intervalFailed, optical_product_upper);
    Interval _13316 = _13590;
    Interval _13591 = jet_add_derivative(_13313, _13316, intervalFailed);
    Interval _13317 = Interval{ 0.0, 0.0 };
    Interval _13318 = hit.z.v;
    Interval _13592 = jet_mul_derivative(_13317, _13318, intervalFailed, optical_product_upper);
    Interval _13319 = _13592;
    Interval _13320 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13321 = hit.z.dy;
    Interval _13594 = jet_mul_derivative(_13320, _13321, intervalFailed, optical_product_upper);
    Interval _13322 = _13594;
    Interval _13595 = jet_add_derivative(_13319, _13322, intervalFailed);
    Interval _13303 = _13545;
    Interval _13304 = _13561;
    Interval _13596 = iadd(_13303, _13304, intervalFailed);
    Interval _13305 = _13546;
    Interval _13306 = _13565;
    Interval _13597 = jet_add_derivative(_13305, _13306, intervalFailed);
    Interval _13307 = _13547;
    Interval _13308 = _13569;
    Interval _13598 = jet_add_derivative(_13307, _13308, intervalFailed);
    Interval _13297 = _13548;
    Interval _13298 = _13574;
    Interval _13599 = iadd(_13297, _13298, intervalFailed);
    Interval _13299 = _13549;
    Interval _13300 = _13578;
    Interval _13600 = jet_add_derivative(_13299, _13300, intervalFailed);
    Interval _13301 = _13550;
    Interval _13302 = _13582;
    Interval _13601 = jet_add_derivative(_13301, _13302, intervalFailed);
    Interval _13291 = _13551;
    Interval _13292 = _13587;
    Interval _13602 = iadd(_13291, _13292, intervalFailed);
    Interval _13293 = _13552;
    Interval _13294 = _13591;
    Interval _13603 = jet_add_derivative(_13293, _13294, intervalFailed);
    Interval _13295 = _13553;
    Interval _13296 = _13595;
    Interval _13604 = jet_add_derivative(_13295, _13296, intervalFailed);
    Interval _13285 = _13596;
    Interval _13286 = Interval{ f.rotation0.w, f.rotation0.w };
    Interval _13615 = iadd(_13285, _13286, intervalFailed);
    Interval _13287 = _13597;
    Interval _13288 = Interval{ 0.0, 0.0 };
    Interval _13616 = jet_add_derivative(_13287, _13288, intervalFailed);
    Interval _13289 = _13598;
    Interval _13290 = Interval{ 0.0, 0.0 };
    Interval _13617 = jet_add_derivative(_13289, _13290, intervalFailed);
    Interval _13279 = _13599;
    Interval _13280 = Interval{ f.rotation1.w, f.rotation1.w };
    Interval _13619 = iadd(_13279, _13280, intervalFailed);
    Interval _13281 = _13600;
    Interval _13282 = Interval{ 0.0, 0.0 };
    Interval _13620 = jet_add_derivative(_13281, _13282, intervalFailed);
    Interval _13283 = _13601;
    Interval _13284 = Interval{ 0.0, 0.0 };
    Interval _13621 = jet_add_derivative(_13283, _13284, intervalFailed);
    Interval _13273 = _13602;
    Interval _13274 = Interval{ f.rotation2.w, f.rotation2.w };
    Interval _13623 = iadd(_13273, _13274, intervalFailed);
    Interval _13275 = _13603;
    Interval _13276 = Interval{ 0.0, 0.0 };
    Interval _13624 = jet_add_derivative(_13275, _13276, intervalFailed);
    Interval _13277 = _13604;
    Interval _13278 = Interval{ 0.0, 0.0 };
    Interval _13625 = jet_add_derivative(_13277, _13278, intervalFailed);
    float _15239;
    float _15240;
    float _15241;
    float _15242;
    float _15243;
    float _15244;
    float _15245;
    float _15246;
    float _15247;
    float _15248;
    float _15249;
    float _15250;
    if (f.settings.y == 3.0)
    {
        Interval _13259 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13260 = direction.x.v;
        Interval _14723 = imul(_13259, _13260, intervalFailed, optical_product_upper);
        Interval _13261 = Interval{ 0.0, 0.0 };
        Interval _13262 = direction.x.v;
        Interval _14724 = jet_mul_derivative(_13261, _13262, intervalFailed, optical_product_upper);
        Interval _13263 = _14724;
        Interval _13264 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13265 = direction.x.dx;
        Interval _14726 = jet_mul_derivative(_13264, _13265, intervalFailed, optical_product_upper);
        Interval _13266 = _14726;
        Interval _14727 = jet_add_derivative(_13263, _13266, intervalFailed);
        Interval _13267 = Interval{ 0.0, 0.0 };
        Interval _13268 = direction.x.v;
        Interval _14728 = jet_mul_derivative(_13267, _13268, intervalFailed, optical_product_upper);
        Interval _13269 = _14728;
        Interval _13270 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13271 = direction.x.dy;
        Interval _14730 = jet_mul_derivative(_13270, _13271, intervalFailed, optical_product_upper);
        Interval _13272 = _14730;
        Interval _14731 = jet_add_derivative(_13269, _13272, intervalFailed);
        Interval _13245 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13246 = direction.x.v;
        Interval _14736 = imul(_13245, _13246, intervalFailed, optical_product_upper);
        Interval _13247 = Interval{ 0.0, 0.0 };
        Interval _13248 = direction.x.v;
        Interval _14737 = jet_mul_derivative(_13247, _13248, intervalFailed, optical_product_upper);
        Interval _13249 = _14737;
        Interval _13250 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13251 = direction.x.dx;
        Interval _14739 = jet_mul_derivative(_13250, _13251, intervalFailed, optical_product_upper);
        Interval _13252 = _14739;
        Interval _14740 = jet_add_derivative(_13249, _13252, intervalFailed);
        Interval _13253 = Interval{ 0.0, 0.0 };
        Interval _13254 = direction.x.v;
        Interval _14741 = jet_mul_derivative(_13253, _13254, intervalFailed, optical_product_upper);
        Interval _13255 = _14741;
        Interval _13256 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13257 = direction.x.dy;
        Interval _14743 = jet_mul_derivative(_13256, _13257, intervalFailed, optical_product_upper);
        Interval _13258 = _14743;
        Interval _14744 = jet_add_derivative(_13255, _13258, intervalFailed);
        Interval _13231 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13232 = direction.x.v;
        Interval _14749 = imul(_13231, _13232, intervalFailed, optical_product_upper);
        Interval _13233 = Interval{ 0.0, 0.0 };
        Interval _13234 = direction.x.v;
        Interval _14750 = jet_mul_derivative(_13233, _13234, intervalFailed, optical_product_upper);
        Interval _13235 = _14750;
        Interval _13236 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13237 = direction.x.dx;
        Interval _14752 = jet_mul_derivative(_13236, _13237, intervalFailed, optical_product_upper);
        Interval _13238 = _14752;
        Interval _14753 = jet_add_derivative(_13235, _13238, intervalFailed);
        Interval _13239 = Interval{ 0.0, 0.0 };
        Interval _13240 = direction.x.v;
        Interval _14754 = jet_mul_derivative(_13239, _13240, intervalFailed, optical_product_upper);
        Interval _13241 = _14754;
        Interval _13242 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13243 = direction.x.dy;
        Interval _14756 = jet_mul_derivative(_13242, _13243, intervalFailed, optical_product_upper);
        Interval _13244 = _14756;
        Interval _14757 = jet_add_derivative(_13241, _13244, intervalFailed);
        Interval _13217 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13218 = direction.y.v;
        Interval _14765 = imul(_13217, _13218, intervalFailed, optical_product_upper);
        Interval _13219 = Interval{ 0.0, 0.0 };
        Interval _13220 = direction.y.v;
        Interval _14766 = jet_mul_derivative(_13219, _13220, intervalFailed, optical_product_upper);
        Interval _13221 = _14766;
        Interval _13222 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13223 = direction.y.dx;
        Interval _14768 = jet_mul_derivative(_13222, _13223, intervalFailed, optical_product_upper);
        Interval _13224 = _14768;
        Interval _14769 = jet_add_derivative(_13221, _13224, intervalFailed);
        Interval _13225 = Interval{ 0.0, 0.0 };
        Interval _13226 = direction.y.v;
        Interval _14770 = jet_mul_derivative(_13225, _13226, intervalFailed, optical_product_upper);
        Interval _13227 = _14770;
        Interval _13228 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13229 = direction.y.dy;
        Interval _14772 = jet_mul_derivative(_13228, _13229, intervalFailed, optical_product_upper);
        Interval _13230 = _14772;
        Interval _14773 = jet_add_derivative(_13227, _13230, intervalFailed);
        Interval _13203 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13204 = direction.y.v;
        Interval _14778 = imul(_13203, _13204, intervalFailed, optical_product_upper);
        Interval _13205 = Interval{ 0.0, 0.0 };
        Interval _13206 = direction.y.v;
        Interval _14779 = jet_mul_derivative(_13205, _13206, intervalFailed, optical_product_upper);
        Interval _13207 = _14779;
        Interval _13208 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13209 = direction.y.dx;
        Interval _14781 = jet_mul_derivative(_13208, _13209, intervalFailed, optical_product_upper);
        Interval _13210 = _14781;
        Interval _14782 = jet_add_derivative(_13207, _13210, intervalFailed);
        Interval _13211 = Interval{ 0.0, 0.0 };
        Interval _13212 = direction.y.v;
        Interval _14783 = jet_mul_derivative(_13211, _13212, intervalFailed, optical_product_upper);
        Interval _13213 = _14783;
        Interval _13214 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13215 = direction.y.dy;
        Interval _14785 = jet_mul_derivative(_13214, _13215, intervalFailed, optical_product_upper);
        Interval _13216 = _14785;
        Interval _14786 = jet_add_derivative(_13213, _13216, intervalFailed);
        Interval _13189 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13190 = direction.y.v;
        Interval _14791 = imul(_13189, _13190, intervalFailed, optical_product_upper);
        Interval _13191 = Interval{ 0.0, 0.0 };
        Interval _13192 = direction.y.v;
        Interval _14792 = jet_mul_derivative(_13191, _13192, intervalFailed, optical_product_upper);
        Interval _13193 = _14792;
        Interval _13194 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13195 = direction.y.dx;
        Interval _14794 = jet_mul_derivative(_13194, _13195, intervalFailed, optical_product_upper);
        Interval _13196 = _14794;
        Interval _14795 = jet_add_derivative(_13193, _13196, intervalFailed);
        Interval _13197 = Interval{ 0.0, 0.0 };
        Interval _13198 = direction.y.v;
        Interval _14796 = jet_mul_derivative(_13197, _13198, intervalFailed, optical_product_upper);
        Interval _13199 = _14796;
        Interval _13200 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13201 = direction.y.dy;
        Interval _14798 = jet_mul_derivative(_13200, _13201, intervalFailed, optical_product_upper);
        Interval _13202 = _14798;
        Interval _14799 = jet_add_derivative(_13199, _13202, intervalFailed);
        Interval _13183 = _14723;
        Interval _13184 = _14765;
        Interval _14800 = iadd(_13183, _13184, intervalFailed);
        Interval _13185 = _14727;
        Interval _13186 = _14769;
        Interval _14801 = jet_add_derivative(_13185, _13186, intervalFailed);
        Interval _13187 = _14731;
        Interval _13188 = _14773;
        Interval _14802 = jet_add_derivative(_13187, _13188, intervalFailed);
        Interval _13177 = _14736;
        Interval _13178 = _14778;
        Interval _14803 = iadd(_13177, _13178, intervalFailed);
        Interval _13179 = _14740;
        Interval _13180 = _14782;
        Interval _14804 = jet_add_derivative(_13179, _13180, intervalFailed);
        Interval _13181 = _14744;
        Interval _13182 = _14786;
        Interval _14805 = jet_add_derivative(_13181, _13182, intervalFailed);
        Interval _13171 = _14749;
        Interval _13172 = _14791;
        Interval _14806 = iadd(_13171, _13172, intervalFailed);
        Interval _13173 = _14753;
        Interval _13174 = _14795;
        Interval _14807 = jet_add_derivative(_13173, _13174, intervalFailed);
        Interval _13175 = _14757;
        Interval _13176 = _14799;
        Interval _14808 = jet_add_derivative(_13175, _13176, intervalFailed);
        Interval _13157 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13158 = direction.z.v;
        Interval _14816 = imul(_13157, _13158, intervalFailed, optical_product_upper);
        Interval _13159 = Interval{ 0.0, 0.0 };
        Interval _13160 = direction.z.v;
        Interval _14817 = jet_mul_derivative(_13159, _13160, intervalFailed, optical_product_upper);
        Interval _13161 = _14817;
        Interval _13162 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13163 = direction.z.dx;
        Interval _14819 = jet_mul_derivative(_13162, _13163, intervalFailed, optical_product_upper);
        Interval _13164 = _14819;
        Interval _14820 = jet_add_derivative(_13161, _13164, intervalFailed);
        Interval _13165 = Interval{ 0.0, 0.0 };
        Interval _13166 = direction.z.v;
        Interval _14821 = jet_mul_derivative(_13165, _13166, intervalFailed, optical_product_upper);
        Interval _13167 = _14821;
        Interval _13168 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13169 = direction.z.dy;
        Interval _14823 = jet_mul_derivative(_13168, _13169, intervalFailed, optical_product_upper);
        Interval _13170 = _14823;
        Interval _14824 = jet_add_derivative(_13167, _13170, intervalFailed);
        Interval _13143 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13144 = direction.z.v;
        Interval _14829 = imul(_13143, _13144, intervalFailed, optical_product_upper);
        Interval _13145 = Interval{ 0.0, 0.0 };
        Interval _13146 = direction.z.v;
        Interval _14830 = jet_mul_derivative(_13145, _13146, intervalFailed, optical_product_upper);
        Interval _13147 = _14830;
        Interval _13148 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13149 = direction.z.dx;
        Interval _14832 = jet_mul_derivative(_13148, _13149, intervalFailed, optical_product_upper);
        Interval _13150 = _14832;
        Interval _14833 = jet_add_derivative(_13147, _13150, intervalFailed);
        Interval _13151 = Interval{ 0.0, 0.0 };
        Interval _13152 = direction.z.v;
        Interval _14834 = jet_mul_derivative(_13151, _13152, intervalFailed, optical_product_upper);
        Interval _13153 = _14834;
        Interval _13154 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13155 = direction.z.dy;
        Interval _14836 = jet_mul_derivative(_13154, _13155, intervalFailed, optical_product_upper);
        Interval _13156 = _14836;
        Interval _14837 = jet_add_derivative(_13153, _13156, intervalFailed);
        Interval _13129 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13130 = direction.z.v;
        Interval _14842 = imul(_13129, _13130, intervalFailed, optical_product_upper);
        Interval _13131 = Interval{ 0.0, 0.0 };
        Interval _13132 = direction.z.v;
        Interval _14843 = jet_mul_derivative(_13131, _13132, intervalFailed, optical_product_upper);
        Interval _13133 = _14843;
        Interval _13134 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13135 = direction.z.dx;
        Interval _14845 = jet_mul_derivative(_13134, _13135, intervalFailed, optical_product_upper);
        Interval _13136 = _14845;
        Interval _14846 = jet_add_derivative(_13133, _13136, intervalFailed);
        Interval _13137 = Interval{ 0.0, 0.0 };
        Interval _13138 = direction.z.v;
        Interval _14847 = jet_mul_derivative(_13137, _13138, intervalFailed, optical_product_upper);
        Interval _13139 = _14847;
        Interval _13140 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13141 = direction.z.dy;
        Interval _14849 = jet_mul_derivative(_13140, _13141, intervalFailed, optical_product_upper);
        Interval _13142 = _14849;
        Interval _14850 = jet_add_derivative(_13139, _13142, intervalFailed);
        Interval _13123 = _14800;
        Interval _13124 = _14816;
        Interval _14851 = iadd(_13123, _13124, intervalFailed);
        Interval _13125 = _14801;
        Interval _13126 = _14820;
        Interval _14852 = jet_add_derivative(_13125, _13126, intervalFailed);
        Interval _13127 = _14802;
        Interval _13128 = _14824;
        Interval _14853 = jet_add_derivative(_13127, _13128, intervalFailed);
        Interval _13117 = _14803;
        Interval _13118 = _14829;
        Interval _14854 = iadd(_13117, _13118, intervalFailed);
        Interval _13119 = _14804;
        Interval _13120 = _14833;
        Interval _14855 = jet_add_derivative(_13119, _13120, intervalFailed);
        Interval _13121 = _14805;
        Interval _13122 = _14837;
        Interval _14856 = jet_add_derivative(_13121, _13122, intervalFailed);
        Interval _13111 = _14806;
        Interval _13112 = _14842;
        Interval _14857 = iadd(_13111, _13112, intervalFailed);
        Interval _13113 = _14807;
        Interval _13114 = _14846;
        Interval _14858 = jet_add_derivative(_13113, _13114, intervalFailed);
        Interval _13115 = _14808;
        Interval _13116 = _14850;
        Interval _14859 = jet_add_derivative(_13115, _13116, intervalFailed);
        float _14896;
        float _14898;
        float _14900;
        float _14902;
        float _14904;
        float _14906;
        float _14908;
        float _14910;
        float _14912;
        float _14914;
        float _14916;
        float _14918;
        _14896 = 0.0;
        _14898 = 0.0;
        _14900 = 0.0;
        _14902 = 0.0;
        _14904 = 0.0;
        _14906 = 0.0;
        _14908 = 0.0;
        _14910 = 0.0;
        _14912 = 0.0;
        _14914 = 0.0;
        _14916 = 0.0;
        _14918 = 0.0;
        float _14861;
        float _14863;
        float _14865;
        float _14867;
        float _14869;
        float _14871;
        float _14873;
        float _14875;
        float _14877;
        float _14879;
        float _14881;
        float _14883;
        float _14885;
        float _14887;
        float _14889;
        float _14891;
        float _14893;
        float _14895;
        float _14897;
        float _14899;
        float _14901;
        float _14903;
        float _14905;
        float _14907;
        float _14909;
        float _14911;
        float _14913;
        float _14915;
        float _14917;
        float _14919;
        float _14860 = _13619.lo;
        float _14862 = _13619.hi;
        float _14864 = _13620.lo;
        float _14866 = _13620.hi;
        float _14868 = _13621.lo;
        float _14870 = _13621.hi;
        float _14872 = _13623.lo;
        float _14874 = _13623.hi;
        float _14876 = _13624.lo;
        float _14878 = _13624.hi;
        float _14880 = _13625.lo;
        float _14882 = _13625.hi;
        float _14884 = _13615.lo;
        float _14886 = _13615.hi;
        float _14888 = _13616.lo;
        float _14890 = _13616.hi;
        float _14892 = _13617.lo;
        float _14894 = _13617.hi;
        uint _14920 = 0u;
        for (; _14920 < 2u; _14860 = _14861, _14862 = _14863, _14864 = _14865, _14866 = _14867, _14868 = _14869, _14870 = _14871, _14872 = _14873, _14874 = _14875, _14876 = _14877, _14878 = _14879, _14880 = _14881, _14882 = _14883, _14884 = _14885, _14886 = _14887, _14888 = _14889, _14890 = _14891, _14892 = _14893, _14894 = _14895, _14896 = _14897, _14898 = _14899, _14900 = _14901, _14902 = _14903, _14904 = _14905, _14906 = _14907, _14908 = _14909, _14910 = _14911, _14912 = _14913, _14914 = _14915, _14916 = _14917, _14918 = _14919, _14920++)
        {
            OpticalJet param_var_x = OpticalJet{ Interval{ _14884, _14886 }, Interval{ _14888, _14890 }, Interval{ _14892, _14894 } };
            OpticalJet param_var_z = OpticalJet{ Interval{ _14872, _14874 }, Interval{ _14876, _14878 }, Interval{ _14880, _14882 } };
            OpticalJet param_var_t = OpticalJet{ Interval{ f.settings.x, f.settings.x }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
            OpticalJet param_var_footprint = footprint;
            OpticalJet3 _14933 = jlava(param_var_x, param_var_z, param_var_t, param_var_footprint, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (_14920 == 0u)
            {
                float _13109 = 2.0;
                float _13110 = 10.0;
                Interval _14961 = iratio(_13109, _13110, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _14966;
                if (!intervalFailed)
                {
                    _14966 = intervalFailed;
                }
                else
                {
                    _14966 = false;
                }
                bool _14971;
                if (_14966)
                {
                    _14971 = jetFailureSite == 0u;
                }
                else
                {
                    _14971 = false;
                }
                if (_14971)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(2.0, 2.0, 10.0, 10.0);
                }
                Interval _13095 = _distance.v;
                Interval _13096 = _14961;
                Interval _14974 = imul(_13095, _13096, intervalFailed, optical_product_upper);
                Interval _13097 = _distance.dx;
                Interval _13098 = _14961;
                Interval _14975 = jet_mul_derivative(_13097, _13098, intervalFailed, optical_product_upper);
                Interval _13099 = _14975;
                Interval _13100 = _distance.v;
                Interval _13101 = Interval{ 0.0, 0.0 };
                Interval _14976 = jet_mul_derivative(_13100, _13101, intervalFailed, optical_product_upper);
                Interval _13102 = _14976;
                Interval _14977 = jet_add_derivative(_13099, _13102, intervalFailed);
                Interval _13103 = _distance.dy;
                Interval _13104 = _14961;
                Interval _14978 = jet_mul_derivative(_13103, _13104, intervalFailed, optical_product_upper);
                Interval _13105 = _14978;
                Interval _13106 = _distance.v;
                Interval _13107 = Interval{ 0.0, 0.0 };
                Interval _14979 = jet_mul_derivative(_13106, _13107, intervalFailed, optical_product_upper);
                Interval _13108 = _14979;
                Interval _14980 = jet_add_derivative(_13105, _13108, intervalFailed);
                float _14988 = as_type<float>(as_type<uint>(_14933.x.v.hi) ^ 2147483648u);
                float _14991 = as_type<float>(as_type<uint>(_14933.x.v.lo) ^ 2147483648u);
                OpticalJet param_var_a = OpticalJet{ _14854, _14855, _14856 };
                float _13093 = 12.0;
                float _13094 = 100.0;
                Interval _15010 = iratio(_13093, _13094, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _15015;
                if (!intervalFailed)
                {
                    _15015 = intervalFailed;
                }
                else
                {
                    _15015 = false;
                }
                bool _15020;
                if (_15015)
                {
                    _15020 = jetFailureSite == 0u;
                }
                else
                {
                    _15020 = false;
                }
                if (_15020)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(12.0, 12.0, 100.0, 100.0);
                }
                OpticalJet param_var_b = OpticalJet{ _15010, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _15024 = jmax(param_var_a, param_var_b);
                Interval _15025 = _15024.v;
                Interval _13079 = Interval{ _14988, _14991 };
                Interval _13080 = _15025;
                Interval _15030 = idiv(_13079, _13080, intervalFailed, interval_divide_upper);
                bool _15037;
                if (!intervalFailed)
                {
                    _15037 = intervalFailed;
                }
                else
                {
                    _15037 = false;
                }
                bool _15042;
                if (_15037)
                {
                    _15042 = jetFailureSite == 0u;
                }
                else
                {
                    _15042 = false;
                }
                if (_15042)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_14988, _14991, _15025.lo, _15025.hi);
                }
                Interval _13081 = Interval{ as_type<float>(as_type<uint>(_14933.x.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14933.x.dx.lo) ^ 2147483648u) };
                Interval _13082 = _15030;
                Interval _13083 = _15024.dx;
                Interval _15047 = jet_mul_derivative(_13082, _13083, intervalFailed, optical_product_upper);
                Interval _13084 = Interval{ as_type<float>(as_type<uint>(_15047.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15047.lo) ^ 2147483648u) };
                Interval _15057 = jet_add_derivative(_13081, _13084, intervalFailed);
                Interval _13085 = _15057;
                Interval _13086 = _15025;
                Interval _15058 = jet_div_derivative(_13085, _13086, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _13087 = Interval{ as_type<float>(as_type<uint>(_14933.x.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14933.x.dy.lo) ^ 2147483648u) };
                Interval _13088 = _15030;
                Interval _13089 = _15024.dy;
                Interval _15060 = jet_mul_derivative(_13088, _13089, intervalFailed, optical_product_upper);
                Interval _13090 = Interval{ as_type<float>(as_type<uint>(_15060.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15060.lo) ^ 2147483648u) };
                Interval _15070 = jet_add_derivative(_13087, _13090, intervalFailed);
                Interval _13091 = _15070;
                Interval _13092 = _15025;
                Interval _15071 = jet_div_derivative(_13091, _13092, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                OpticalJet _13075 = OpticalJet{ _15030, _15058, _15071 };
                OpticalJet _13076 = OpticalJet{ Interval{ as_type<float>(as_type<uint>(_14974.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14974.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(_14977.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14977.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(_14980.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14980.lo) ^ 2147483648u) } };
                OpticalJet _13077 = jmax(_13075, _13076);
                OpticalJet _13078 = OpticalJet{ _14974, _14977, _14980 };
                OpticalJet _15103 = jmin(_13077, _13078);
                Interval _15104 = _15103.v;
                Interval _13061 = _14851;
                Interval _13062 = _15104;
                Interval _15107 = imul(_13061, _13062, intervalFailed, optical_product_upper);
                Interval _13063 = _14852;
                Interval _13064 = _15104;
                Interval _15108 = jet_mul_derivative(_13063, _13064, intervalFailed, optical_product_upper);
                Interval _13065 = _15108;
                Interval _13066 = _14851;
                Interval _13067 = _15103.dx;
                Interval _15109 = jet_mul_derivative(_13066, _13067, intervalFailed, optical_product_upper);
                Interval _13068 = _15109;
                Interval _15110 = jet_add_derivative(_13065, _13068, intervalFailed);
                Interval _13069 = _14853;
                Interval _13070 = _15104;
                Interval _15111 = jet_mul_derivative(_13069, _13070, intervalFailed, optical_product_upper);
                Interval _13071 = _15111;
                Interval _13072 = _14851;
                Interval _13073 = _15103.dy;
                Interval _15112 = jet_mul_derivative(_13072, _13073, intervalFailed, optical_product_upper);
                Interval _13074 = _15112;
                Interval _15113 = jet_add_derivative(_13071, _13074, intervalFailed);
                Interval _15114 = _15103.v;
                Interval _13047 = _14854;
                Interval _13048 = _15114;
                Interval _15117 = imul(_13047, _13048, intervalFailed, optical_product_upper);
                Interval _13049 = _14855;
                Interval _13050 = _15114;
                Interval _15118 = jet_mul_derivative(_13049, _13050, intervalFailed, optical_product_upper);
                Interval _13051 = _15118;
                Interval _13052 = _14854;
                Interval _13053 = _15103.dx;
                Interval _15119 = jet_mul_derivative(_13052, _13053, intervalFailed, optical_product_upper);
                Interval _13054 = _15119;
                Interval _15120 = jet_add_derivative(_13051, _13054, intervalFailed);
                Interval _13055 = _14856;
                Interval _13056 = _15114;
                Interval _15121 = jet_mul_derivative(_13055, _13056, intervalFailed, optical_product_upper);
                Interval _13057 = _15121;
                Interval _13058 = _14854;
                Interval _13059 = _15103.dy;
                Interval _15122 = jet_mul_derivative(_13058, _13059, intervalFailed, optical_product_upper);
                Interval _13060 = _15122;
                Interval _15123 = jet_add_derivative(_13057, _13060, intervalFailed);
                Interval _15124 = _15103.v;
                Interval _13033 = _14857;
                Interval _13034 = _15124;
                Interval _15127 = imul(_13033, _13034, intervalFailed, optical_product_upper);
                Interval _13035 = _14858;
                Interval _13036 = _15124;
                Interval _15128 = jet_mul_derivative(_13035, _13036, intervalFailed, optical_product_upper);
                Interval _13037 = _15128;
                Interval _13038 = _14857;
                Interval _13039 = _15103.dx;
                Interval _15129 = jet_mul_derivative(_13038, _13039, intervalFailed, optical_product_upper);
                Interval _13040 = _15129;
                Interval _15130 = jet_add_derivative(_13037, _13040, intervalFailed);
                Interval _13041 = _14859;
                Interval _13042 = _15124;
                Interval _15131 = jet_mul_derivative(_13041, _13042, intervalFailed, optical_product_upper);
                Interval _13043 = _15131;
                Interval _13044 = _14857;
                Interval _13045 = _15103.dy;
                Interval _15132 = jet_mul_derivative(_13044, _13045, intervalFailed, optical_product_upper);
                Interval _13046 = _15132;
                Interval _15133 = jet_add_derivative(_13043, _13046, intervalFailed);
                Interval _13027 = Interval{ _14884, _14886 };
                Interval _13028 = _15107;
                Interval _15135 = iadd(_13027, _13028, intervalFailed);
                Interval _13029 = Interval{ _14888, _14890 };
                Interval _13030 = _15110;
                Interval _15137 = jet_add_derivative(_13029, _13030, intervalFailed);
                Interval _13031 = Interval{ _14892, _14894 };
                Interval _13032 = _15113;
                Interval _15139 = jet_add_derivative(_13031, _13032, intervalFailed);
                Interval _13021 = Interval{ _14860, _14862 };
                Interval _13022 = _15117;
                Interval _15141 = iadd(_13021, _13022, intervalFailed);
                Interval _13023 = Interval{ _14864, _14866 };
                Interval _13024 = _15120;
                Interval _15143 = jet_add_derivative(_13023, _13024, intervalFailed);
                Interval _13025 = Interval{ _14868, _14870 };
                Interval _13026 = _15123;
                Interval _15145 = jet_add_derivative(_13025, _13026, intervalFailed);
                Interval _13015 = Interval{ _14872, _14874 };
                Interval _13016 = _15127;
                Interval _15147 = iadd(_13015, _13016, intervalFailed);
                Interval _13017 = Interval{ _14876, _14878 };
                Interval _13018 = _15130;
                Interval _15149 = jet_add_derivative(_13017, _13018, intervalFailed);
                Interval _13019 = Interval{ _14880, _14882 };
                Interval _13020 = _15133;
                Interval _15151 = jet_add_derivative(_13019, _13020, intervalFailed);
                Interval _15181 = _15103.v;
                Interval _13001 = direction.x.v;
                Interval _13002 = _15181;
                Interval _15184 = imul(_13001, _13002, intervalFailed, optical_product_upper);
                Interval _13003 = direction.x.dx;
                Interval _13004 = _15181;
                Interval _15185 = jet_mul_derivative(_13003, _13004, intervalFailed, optical_product_upper);
                Interval _13005 = _15185;
                Interval _13006 = direction.x.v;
                Interval _13007 = _15103.dx;
                Interval _15186 = jet_mul_derivative(_13006, _13007, intervalFailed, optical_product_upper);
                Interval _13008 = _15186;
                Interval _15187 = jet_add_derivative(_13005, _13008, intervalFailed);
                Interval _13009 = direction.x.dy;
                Interval _13010 = _15181;
                Interval _15188 = jet_mul_derivative(_13009, _13010, intervalFailed, optical_product_upper);
                Interval _13011 = _15188;
                Interval _13012 = direction.x.v;
                Interval _13013 = _15103.dy;
                Interval _15189 = jet_mul_derivative(_13012, _13013, intervalFailed, optical_product_upper);
                Interval _13014 = _15189;
                Interval _15190 = jet_add_derivative(_13011, _13014, intervalFailed);
                Interval _15194 = _15103.v;
                Interval _12987 = direction.y.v;
                Interval _12988 = _15194;
                Interval _15197 = imul(_12987, _12988, intervalFailed, optical_product_upper);
                Interval _12989 = direction.y.dx;
                Interval _12990 = _15194;
                Interval _15198 = jet_mul_derivative(_12989, _12990, intervalFailed, optical_product_upper);
                Interval _12991 = _15198;
                Interval _12992 = direction.y.v;
                Interval _12993 = _15103.dx;
                Interval _15199 = jet_mul_derivative(_12992, _12993, intervalFailed, optical_product_upper);
                Interval _12994 = _15199;
                Interval _15200 = jet_add_derivative(_12991, _12994, intervalFailed);
                Interval _12995 = direction.y.dy;
                Interval _12996 = _15194;
                Interval _15201 = jet_mul_derivative(_12995, _12996, intervalFailed, optical_product_upper);
                Interval _12997 = _15201;
                Interval _12998 = direction.y.v;
                Interval _12999 = _15103.dy;
                Interval _15202 = jet_mul_derivative(_12998, _12999, intervalFailed, optical_product_upper);
                Interval _13000 = _15202;
                Interval _15203 = jet_add_derivative(_12997, _13000, intervalFailed);
                Interval _15207 = _15103.v;
                Interval _12973 = direction.z.v;
                Interval _12974 = _15207;
                Interval _15210 = imul(_12973, _12974, intervalFailed, optical_product_upper);
                Interval _12975 = direction.z.dx;
                Interval _12976 = _15207;
                Interval _15211 = jet_mul_derivative(_12975, _12976, intervalFailed, optical_product_upper);
                Interval _12977 = _15211;
                Interval _12978 = direction.z.v;
                Interval _12979 = _15103.dx;
                Interval _15212 = jet_mul_derivative(_12978, _12979, intervalFailed, optical_product_upper);
                Interval _12980 = _15212;
                Interval _15213 = jet_add_derivative(_12977, _12980, intervalFailed);
                Interval _12981 = direction.z.dy;
                Interval _12982 = _15207;
                Interval _15214 = jet_mul_derivative(_12981, _12982, intervalFailed, optical_product_upper);
                Interval _12983 = _15214;
                Interval _12984 = direction.z.v;
                Interval _12985 = _15103.dy;
                Interval _15215 = jet_mul_derivative(_12984, _12985, intervalFailed, optical_product_upper);
                Interval _12986 = _15215;
                Interval _15216 = jet_add_derivative(_12983, _12986, intervalFailed);
                Interval _12967 = hit.x.v;
                Interval _12968 = _15184;
                Interval _15220 = iadd(_12967, _12968, intervalFailed);
                Interval _12969 = hit.x.dx;
                Interval _12970 = _15187;
                Interval _15221 = jet_add_derivative(_12969, _12970, intervalFailed);
                Interval _12971 = hit.x.dy;
                Interval _12972 = _15190;
                Interval _15222 = jet_add_derivative(_12971, _12972, intervalFailed);
                Interval _12961 = hit.y.v;
                Interval _12962 = _15197;
                Interval _15226 = iadd(_12961, _12962, intervalFailed);
                Interval _12963 = hit.y.dx;
                Interval _12964 = _15200;
                Interval _15227 = jet_add_derivative(_12963, _12964, intervalFailed);
                Interval _12965 = hit.y.dy;
                Interval _12966 = _15203;
                Interval _15228 = jet_add_derivative(_12965, _12966, intervalFailed);
                Interval _12955 = hit.z.v;
                Interval _12956 = _15210;
                Interval _15232 = iadd(_12955, _12956, intervalFailed);
                Interval _12957 = hit.z.dx;
                Interval _12958 = _15213;
                Interval _15233 = jet_add_derivative(_12957, _12958, intervalFailed);
                Interval _12959 = hit.z.dy;
                Interval _12960 = _15216;
                Interval _15234 = jet_add_derivative(_12959, _12960, intervalFailed);
                hit = OpticalJet3{ OpticalJet{ _15220, _15221, _15222 }, OpticalJet{ _15226, _15227, _15228 }, OpticalJet{ _15232, _15233, _15234 } };
                _14861 = _15141.lo;
                _14863 = _15141.hi;
                _14865 = _15143.lo;
                _14867 = _15143.hi;
                _14869 = _15145.lo;
                _14871 = _15145.hi;
                _14873 = _15147.lo;
                _14875 = _15147.hi;
                _14877 = _15149.lo;
                _14879 = _15149.hi;
                _14881 = _15151.lo;
                _14883 = _15151.hi;
                _14885 = _15135.lo;
                _14887 = _15135.hi;
                _14889 = _15137.lo;
                _14891 = _15137.hi;
                _14893 = _15139.lo;
                _14895 = _15139.hi;
                _14897 = _14896;
                _14899 = _14898;
                _14901 = _14900;
                _14903 = _14902;
                _14905 = _14904;
                _14907 = _14906;
                _14909 = _14908;
                _14911 = _14910;
                _14913 = _14912;
                _14915 = _14914;
                _14917 = _14916;
                _14919 = _14918;
            }
            else
            {
                _14861 = _14860;
                _14863 = _14862;
                _14865 = _14864;
                _14867 = _14866;
                _14869 = _14868;
                _14871 = _14870;
                _14873 = _14872;
                _14875 = _14874;
                _14877 = _14876;
                _14879 = _14878;
                _14881 = _14880;
                _14883 = _14882;
                _14885 = _14884;
                _14887 = _14886;
                _14889 = _14888;
                _14891 = _14890;
                _14893 = _14892;
                _14895 = _14894;
                _14897 = _14933.z.v.lo;
                _14899 = _14933.z.v.hi;
                _14901 = _14933.z.dx.lo;
                _14903 = _14933.z.dx.hi;
                _14905 = _14933.z.dy.lo;
                _14907 = _14933.z.dy.hi;
                _14909 = _14933.y.v.lo;
                _14911 = _14933.y.v.hi;
                _14913 = _14933.y.dx.lo;
                _14915 = _14933.y.dx.hi;
                _14917 = _14933.y.dy.lo;
                _14919 = _14933.y.dy.hi;
            }
        }
        _15239 = _14896;
        _15240 = _14898;
        _15241 = _14900;
        _15242 = _14902;
        _15243 = _14904;
        _15244 = _14906;
        _15245 = _14908;
        _15246 = _14910;
        _15247 = _14912;
        _15248 = _14914;
        _15249 = _14916;
        _15250 = _14918;
    }
    else
    {
        float _12953 = 18.0;
        float _12954 = 1000.0;
        Interval _13652 = iratio(_12953, _12954, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13657;
        if (!intervalFailed)
        {
            _13657 = intervalFailed;
        }
        else
        {
            _13657 = false;
        }
        bool _13662;
        if (_13657)
        {
            _13662 = jetFailureSite == 0u;
        }
        else
        {
            _13662 = false;
        }
        if (_13662)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
        }
        Interval _12939 = _13615;
        Interval _12940 = _13652;
        Interval _13665 = imul(_12939, _12940, intervalFailed, optical_product_upper);
        Interval _12941 = _13616;
        Interval _12942 = _13652;
        Interval _13666 = jet_mul_derivative(_12941, _12942, intervalFailed, optical_product_upper);
        Interval _12943 = _13666;
        Interval _12944 = _13615;
        Interval _12945 = Interval{ 0.0, 0.0 };
        Interval _13667 = jet_mul_derivative(_12944, _12945, intervalFailed, optical_product_upper);
        Interval _12946 = _13667;
        Interval _13668 = jet_add_derivative(_12943, _12946, intervalFailed);
        Interval _12947 = _13617;
        Interval _12948 = _13652;
        Interval _13669 = jet_mul_derivative(_12947, _12948, intervalFailed, optical_product_upper);
        Interval _12949 = _13669;
        Interval _12950 = _13615;
        Interval _12951 = Interval{ 0.0, 0.0 };
        Interval _13670 = jet_mul_derivative(_12950, _12951, intervalFailed, optical_product_upper);
        Interval _12952 = _13670;
        Interval _13671 = jet_add_derivative(_12949, _12952, intervalFailed);
        float _12937 = 11.0;
        float _12938 = 1000.0;
        Interval _13673 = iratio(_12937, _12938, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13678;
        if (!intervalFailed)
        {
            _13678 = intervalFailed;
        }
        else
        {
            _13678 = false;
        }
        bool _13683;
        if (_13678)
        {
            _13683 = jetFailureSite == 0u;
        }
        else
        {
            _13683 = false;
        }
        if (_13683)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
        }
        Interval _12923 = _13623;
        Interval _12924 = _13673;
        Interval _13686 = imul(_12923, _12924, intervalFailed, optical_product_upper);
        Interval _12925 = _13624;
        Interval _12926 = _13673;
        Interval _13687 = jet_mul_derivative(_12925, _12926, intervalFailed, optical_product_upper);
        Interval _12927 = _13687;
        Interval _12928 = _13623;
        Interval _12929 = Interval{ 0.0, 0.0 };
        Interval _13688 = jet_mul_derivative(_12928, _12929, intervalFailed, optical_product_upper);
        Interval _12930 = _13688;
        Interval _13689 = jet_add_derivative(_12927, _12930, intervalFailed);
        Interval _12931 = _13625;
        Interval _12932 = _13673;
        Interval _13690 = jet_mul_derivative(_12931, _12932, intervalFailed, optical_product_upper);
        Interval _12933 = _13690;
        Interval _12934 = _13623;
        Interval _12935 = Interval{ 0.0, 0.0 };
        Interval _13691 = jet_mul_derivative(_12934, _12935, intervalFailed, optical_product_upper);
        Interval _12936 = _13691;
        Interval _13692 = jet_add_derivative(_12933, _12936, intervalFailed);
        Interval _12917 = _13665;
        Interval _12918 = _13686;
        Interval _13693 = iadd(_12917, _12918, intervalFailed);
        Interval _12919 = _13668;
        Interval _12920 = _13689;
        Interval _13694 = jet_add_derivative(_12919, _12920, intervalFailed);
        Interval _12921 = _13671;
        Interval _12922 = _13692;
        Interval _13695 = jet_add_derivative(_12921, _12922, intervalFailed);
        float _12915 = 8.0;
        float _12916 = 10.0;
        Interval _13697 = iratio(_12915, _12916, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13702;
        if (!intervalFailed)
        {
            _13702 = intervalFailed;
        }
        else
        {
            _13702 = false;
        }
        bool _13707;
        if (_13702)
        {
            _13707 = jetFailureSite == 0u;
        }
        else
        {
            _13707 = false;
        }
        if (_13707)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(8.0, 8.0, 10.0, 10.0);
        }
        Interval _12901 = Interval{ f.settings.x, f.settings.x };
        Interval _12902 = _13697;
        Interval _13711 = imul(_12901, _12902, intervalFailed, optical_product_upper);
        Interval _12903 = Interval{ 0.0, 0.0 };
        Interval _12904 = _13697;
        Interval _13712 = jet_mul_derivative(_12903, _12904, intervalFailed, optical_product_upper);
        Interval _12905 = _13712;
        Interval _12906 = Interval{ f.settings.x, f.settings.x };
        Interval _12907 = Interval{ 0.0, 0.0 };
        Interval _13714 = jet_mul_derivative(_12906, _12907, intervalFailed, optical_product_upper);
        Interval _12908 = _13714;
        Interval _13715 = jet_add_derivative(_12905, _12908, intervalFailed);
        Interval _12909 = Interval{ 0.0, 0.0 };
        Interval _12910 = _13697;
        Interval _13716 = jet_mul_derivative(_12909, _12910, intervalFailed, optical_product_upper);
        Interval _12911 = _13716;
        Interval _12912 = Interval{ f.settings.x, f.settings.x };
        Interval _12913 = Interval{ 0.0, 0.0 };
        Interval _13718 = jet_mul_derivative(_12912, _12913, intervalFailed, optical_product_upper);
        Interval _12914 = _13718;
        Interval _13719 = jet_add_derivative(_12911, _12914, intervalFailed);
        Interval _12895 = _13693;
        Interval _12896 = Interval{ as_type<float>(as_type<uint>(_13711.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13711.lo) ^ 2147483648u) };
        Interval _13745 = iadd(_12895, _12896, intervalFailed);
        Interval _12897 = _13694;
        Interval _12898 = Interval{ as_type<float>(as_type<uint>(_13715.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13715.lo) ^ 2147483648u) };
        Interval _13747 = jet_add_derivative(_12897, _12898, intervalFailed);
        Interval _12899 = _13695;
        Interval _12900 = Interval{ as_type<float>(as_type<uint>(_13719.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13719.lo) ^ 2147483648u) };
        Interval _13749 = jet_add_derivative(_12899, _12900, intervalFailed);
        float _12893 = 47.0;
        float _12894 = 1000.0;
        Interval _13751 = iratio(_12893, _12894, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13756;
        if (!intervalFailed)
        {
            _13756 = intervalFailed;
        }
        else
        {
            _13756 = false;
        }
        bool _13761;
        if (_13756)
        {
            _13761 = jetFailureSite == 0u;
        }
        else
        {
            _13761 = false;
        }
        if (_13761)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(47.0, 47.0, 1000.0, 1000.0);
        }
        Interval _12879 = _13615;
        Interval _12880 = _13751;
        Interval _13764 = imul(_12879, _12880, intervalFailed, optical_product_upper);
        Interval _12881 = _13616;
        Interval _12882 = _13751;
        Interval _13765 = jet_mul_derivative(_12881, _12882, intervalFailed, optical_product_upper);
        Interval _12883 = _13765;
        Interval _12884 = _13615;
        Interval _12885 = Interval{ 0.0, 0.0 };
        Interval _13766 = jet_mul_derivative(_12884, _12885, intervalFailed, optical_product_upper);
        Interval _12886 = _13766;
        Interval _13767 = jet_add_derivative(_12883, _12886, intervalFailed);
        Interval _12887 = _13617;
        Interval _12888 = _13751;
        Interval _13768 = jet_mul_derivative(_12887, _12888, intervalFailed, optical_product_upper);
        Interval _12889 = _13768;
        Interval _12890 = _13615;
        Interval _12891 = Interval{ 0.0, 0.0 };
        Interval _13769 = jet_mul_derivative(_12890, _12891, intervalFailed, optical_product_upper);
        Interval _12892 = _13769;
        Interval _13770 = jet_add_derivative(_12889, _12892, intervalFailed);
        float _12877 = 25.0;
        float _12878 = 1000.0;
        Interval _13772 = iratio(_12877, _12878, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13777;
        if (!intervalFailed)
        {
            _13777 = intervalFailed;
        }
        else
        {
            _13777 = false;
        }
        bool _13782;
        if (_13777)
        {
            _13782 = jetFailureSite == 0u;
        }
        else
        {
            _13782 = false;
        }
        if (_13782)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
        }
        Interval _12863 = _13623;
        Interval _12864 = _13772;
        Interval _13785 = imul(_12863, _12864, intervalFailed, optical_product_upper);
        Interval _12865 = _13624;
        Interval _12866 = _13772;
        Interval _13786 = jet_mul_derivative(_12865, _12866, intervalFailed, optical_product_upper);
        Interval _12867 = _13786;
        Interval _12868 = _13623;
        Interval _12869 = Interval{ 0.0, 0.0 };
        Interval _13787 = jet_mul_derivative(_12868, _12869, intervalFailed, optical_product_upper);
        Interval _12870 = _13787;
        Interval _13788 = jet_add_derivative(_12867, _12870, intervalFailed);
        Interval _12871 = _13625;
        Interval _12872 = _13772;
        Interval _13789 = jet_mul_derivative(_12871, _12872, intervalFailed, optical_product_upper);
        Interval _12873 = _13789;
        Interval _12874 = _13623;
        Interval _12875 = Interval{ 0.0, 0.0 };
        Interval _13790 = jet_mul_derivative(_12874, _12875, intervalFailed, optical_product_upper);
        Interval _12876 = _13790;
        Interval _13791 = jet_add_derivative(_12873, _12876, intervalFailed);
        Interval _12857 = _13764;
        Interval _12858 = Interval{ as_type<float>(as_type<uint>(_13785.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13785.lo) ^ 2147483648u) };
        Interval _13817 = iadd(_12857, _12858, intervalFailed);
        Interval _12859 = _13767;
        Interval _12860 = Interval{ as_type<float>(as_type<uint>(_13788.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13788.lo) ^ 2147483648u) };
        Interval _13819 = jet_add_derivative(_12859, _12860, intervalFailed);
        Interval _12861 = _13770;
        Interval _12862 = Interval{ as_type<float>(as_type<uint>(_13791.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13791.lo) ^ 2147483648u) };
        Interval _13821 = jet_add_derivative(_12861, _12862, intervalFailed);
        float _12855 = 12.0;
        float _12856 = 10.0;
        Interval _13823 = iratio(_12855, _12856, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13828;
        if (!intervalFailed)
        {
            _13828 = intervalFailed;
        }
        else
        {
            _13828 = false;
        }
        bool _13833;
        if (_13828)
        {
            _13833 = jetFailureSite == 0u;
        }
        else
        {
            _13833 = false;
        }
        if (_13833)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(12.0, 12.0, 10.0, 10.0);
        }
        Interval _12841 = Interval{ f.settings.x, f.settings.x };
        Interval _12842 = _13823;
        Interval _13837 = imul(_12841, _12842, intervalFailed, optical_product_upper);
        Interval _12843 = Interval{ 0.0, 0.0 };
        Interval _12844 = _13823;
        Interval _13838 = jet_mul_derivative(_12843, _12844, intervalFailed, optical_product_upper);
        Interval _12845 = _13838;
        Interval _12846 = Interval{ f.settings.x, f.settings.x };
        Interval _12847 = Interval{ 0.0, 0.0 };
        Interval _13840 = jet_mul_derivative(_12846, _12847, intervalFailed, optical_product_upper);
        Interval _12848 = _13840;
        Interval _13841 = jet_add_derivative(_12845, _12848, intervalFailed);
        Interval _12849 = Interval{ 0.0, 0.0 };
        Interval _12850 = _13823;
        Interval _13842 = jet_mul_derivative(_12849, _12850, intervalFailed, optical_product_upper);
        Interval _12851 = _13842;
        Interval _12852 = Interval{ f.settings.x, f.settings.x };
        Interval _12853 = Interval{ 0.0, 0.0 };
        Interval _13844 = jet_mul_derivative(_12852, _12853, intervalFailed, optical_product_upper);
        Interval _12854 = _13844;
        Interval _13845 = jet_add_derivative(_12851, _12854, intervalFailed);
        Interval _12835 = _13817;
        Interval _12836 = _13837;
        Interval _13846 = iadd(_12835, _12836, intervalFailed);
        Interval _12837 = _13819;
        Interval _12838 = _13841;
        Interval _13847 = jet_add_derivative(_12837, _12838, intervalFailed);
        Interval _12839 = _13821;
        Interval _12840 = _13845;
        Interval _13848 = jet_add_derivative(_12839, _12840, intervalFailed);
        float _12833 = 22.0;
        float _12834 = 1000.0;
        Interval _13850 = iratio(_12833, _12834, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13855;
        if (!intervalFailed)
        {
            _13855 = intervalFailed;
        }
        else
        {
            _13855 = false;
        }
        bool _13860;
        if (_13855)
        {
            _13860 = jetFailureSite == 0u;
        }
        else
        {
            _13860 = false;
        }
        if (_13860)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
        }
        Interval _12819 = _13623;
        Interval _12820 = _13850;
        Interval _13863 = imul(_12819, _12820, intervalFailed, optical_product_upper);
        Interval _12821 = _13624;
        Interval _12822 = _13850;
        Interval _13864 = jet_mul_derivative(_12821, _12822, intervalFailed, optical_product_upper);
        Interval _12823 = _13864;
        Interval _12824 = _13623;
        Interval _12825 = Interval{ 0.0, 0.0 };
        Interval _13865 = jet_mul_derivative(_12824, _12825, intervalFailed, optical_product_upper);
        Interval _12826 = _13865;
        Interval _13866 = jet_add_derivative(_12823, _12826, intervalFailed);
        Interval _12827 = _13625;
        Interval _12828 = _13850;
        Interval _13867 = jet_mul_derivative(_12827, _12828, intervalFailed, optical_product_upper);
        Interval _12829 = _13867;
        Interval _12830 = _13623;
        Interval _12831 = Interval{ 0.0, 0.0 };
        Interval _13868 = jet_mul_derivative(_12830, _12831, intervalFailed, optical_product_upper);
        Interval _12832 = _13868;
        Interval _13869 = jet_add_derivative(_12829, _12832, intervalFailed);
        float _12817 = 9.0;
        float _12818 = 1000.0;
        Interval _13871 = iratio(_12817, _12818, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13876;
        if (!intervalFailed)
        {
            _13876 = intervalFailed;
        }
        else
        {
            _13876 = false;
        }
        bool _13881;
        if (_13876)
        {
            _13881 = jetFailureSite == 0u;
        }
        else
        {
            _13881 = false;
        }
        if (_13881)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(9.0, 9.0, 1000.0, 1000.0);
        }
        Interval _12803 = _13615;
        Interval _12804 = _13871;
        Interval _13884 = imul(_12803, _12804, intervalFailed, optical_product_upper);
        Interval _12805 = _13616;
        Interval _12806 = _13871;
        Interval _13885 = jet_mul_derivative(_12805, _12806, intervalFailed, optical_product_upper);
        Interval _12807 = _13885;
        Interval _12808 = _13615;
        Interval _12809 = Interval{ 0.0, 0.0 };
        Interval _13886 = jet_mul_derivative(_12808, _12809, intervalFailed, optical_product_upper);
        Interval _12810 = _13886;
        Interval _13887 = jet_add_derivative(_12807, _12810, intervalFailed);
        Interval _12811 = _13617;
        Interval _12812 = _13871;
        Interval _13888 = jet_mul_derivative(_12811, _12812, intervalFailed, optical_product_upper);
        Interval _12813 = _13888;
        Interval _12814 = _13615;
        Interval _12815 = Interval{ 0.0, 0.0 };
        Interval _13889 = jet_mul_derivative(_12814, _12815, intervalFailed, optical_product_upper);
        Interval _12816 = _13889;
        Interval _13890 = jet_add_derivative(_12813, _12816, intervalFailed);
        Interval _12797 = _13863;
        Interval _12798 = Interval{ as_type<float>(as_type<uint>(_13884.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13884.lo) ^ 2147483648u) };
        Interval _13916 = iadd(_12797, _12798, intervalFailed);
        Interval _12799 = _13866;
        Interval _12800 = Interval{ as_type<float>(as_type<uint>(_13887.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13887.lo) ^ 2147483648u) };
        Interval _13918 = jet_add_derivative(_12799, _12800, intervalFailed);
        Interval _12801 = _13869;
        Interval _12802 = Interval{ as_type<float>(as_type<uint>(_13890.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13890.lo) ^ 2147483648u) };
        Interval _13920 = jet_add_derivative(_12801, _12802, intervalFailed);
        float _12795 = 65.0;
        float _12796 = 100.0;
        Interval _13922 = iratio(_12795, _12796, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13927;
        if (!intervalFailed)
        {
            _13927 = intervalFailed;
        }
        else
        {
            _13927 = false;
        }
        bool _13932;
        if (_13927)
        {
            _13932 = jetFailureSite == 0u;
        }
        else
        {
            _13932 = false;
        }
        if (_13932)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(65.0, 65.0, 100.0, 100.0);
        }
        Interval _12781 = Interval{ f.settings.x, f.settings.x };
        Interval _12782 = _13922;
        Interval _13936 = imul(_12781, _12782, intervalFailed, optical_product_upper);
        Interval _12783 = Interval{ 0.0, 0.0 };
        Interval _12784 = _13922;
        Interval _13937 = jet_mul_derivative(_12783, _12784, intervalFailed, optical_product_upper);
        Interval _12785 = _13937;
        Interval _12786 = Interval{ f.settings.x, f.settings.x };
        Interval _12787 = Interval{ 0.0, 0.0 };
        Interval _13939 = jet_mul_derivative(_12786, _12787, intervalFailed, optical_product_upper);
        Interval _12788 = _13939;
        Interval _13940 = jet_add_derivative(_12785, _12788, intervalFailed);
        Interval _12789 = Interval{ 0.0, 0.0 };
        Interval _12790 = _13922;
        Interval _13941 = jet_mul_derivative(_12789, _12790, intervalFailed, optical_product_upper);
        Interval _12791 = _13941;
        Interval _12792 = Interval{ f.settings.x, f.settings.x };
        Interval _12793 = Interval{ 0.0, 0.0 };
        Interval _13943 = jet_mul_derivative(_12792, _12793, intervalFailed, optical_product_upper);
        Interval _12794 = _13943;
        Interval _13944 = jet_add_derivative(_12791, _12794, intervalFailed);
        Interval _12775 = _13916;
        Interval _12776 = Interval{ as_type<float>(as_type<uint>(_13936.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13936.lo) ^ 2147483648u) };
        Interval _13970 = iadd(_12775, _12776, intervalFailed);
        Interval _12777 = _13918;
        Interval _12778 = Interval{ as_type<float>(as_type<uint>(_13940.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13940.lo) ^ 2147483648u) };
        Interval _13972 = jet_add_derivative(_12777, _12778, intervalFailed);
        Interval _12779 = _13920;
        Interval _12780 = Interval{ as_type<float>(as_type<uint>(_13944.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13944.lo) ^ 2147483648u) };
        Interval _13974 = jet_add_derivative(_12779, _12780, intervalFailed);
        float _12773 = 55.0;
        float _12774 = 1000.0;
        Interval _13976 = iratio(_12773, _12774, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _13981;
        if (!intervalFailed)
        {
            _13981 = intervalFailed;
        }
        else
        {
            _13981 = false;
        }
        bool _13986;
        if (_13981)
        {
            _13986 = jetFailureSite == 0u;
        }
        else
        {
            _13986 = false;
        }
        if (_13986)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(55.0, 55.0, 1000.0, 1000.0);
        }
        float _12767 = _13745.lo;
        float _12768 = _13745.hi;
        float _13991 = sine_bounds(_12767, _12768, intervalFailed, optical_product_upper, interval_sine_upper);
        float _13995 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _13998 = as_type<float>(as_type<uint>(_13991) ^ 2147483648u);
        Interval _12763 = _13745;
        float _12761 = 3.1415927410125732421875;
        float _13999 = interval_down(_12761, intervalFailed);
        float _12762 = 3.1415927410125732421875;
        float _14000 = interval_up(_12762, intervalFailed);
        Interval _12764 = Interval{ _13999, _14000 };
        Interval _12765 = Interval{ 0.5, 0.5 };
        Interval _14002 = imul(_12764, _12765, intervalFailed, optical_product_upper);
        Interval _12766 = _14002;
        Interval _14003 = iadd(_12763, _12766, intervalFailed);
        float _12759 = _14003.lo;
        float _12760 = _14003.hi;
        float _14006 = sine_bounds(_12759, _12760, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12769 = Interval{ _13995, _13998 };
        Interval _12770 = _13747;
        Interval _14009 = jet_mul_derivative(_12769, _12770, intervalFailed, optical_product_upper);
        Interval _12771 = Interval{ _13995, _13998 };
        Interval _12772 = _13749;
        Interval _14011 = jet_mul_derivative(_12771, _12772, intervalFailed, optical_product_upper);
        Interval _12745 = _13976;
        Interval _12746 = Interval{ _14006, interval_sine_upper };
        Interval _14013 = imul(_12745, _12746, intervalFailed, optical_product_upper);
        Interval _12747 = Interval{ 0.0, 0.0 };
        Interval _12748 = Interval{ _14006, interval_sine_upper };
        Interval _14015 = jet_mul_derivative(_12747, _12748, intervalFailed, optical_product_upper);
        Interval _12749 = _14015;
        Interval _12750 = _13976;
        Interval _12751 = _14009;
        Interval _14016 = jet_mul_derivative(_12750, _12751, intervalFailed, optical_product_upper);
        Interval _12752 = _14016;
        Interval _14017 = jet_add_derivative(_12749, _12752, intervalFailed);
        Interval _12753 = Interval{ 0.0, 0.0 };
        Interval _12754 = Interval{ _14006, interval_sine_upper };
        Interval _14019 = jet_mul_derivative(_12753, _12754, intervalFailed, optical_product_upper);
        Interval _12755 = _14019;
        Interval _12756 = _13976;
        Interval _12757 = _14011;
        Interval _14020 = jet_mul_derivative(_12756, _12757, intervalFailed, optical_product_upper);
        Interval _12758 = _14020;
        Interval _14021 = jet_add_derivative(_12755, _12758, intervalFailed);
        float _12743 = 22.0;
        float _12744 = 1000.0;
        Interval _14024 = iratio(_12743, _12744, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14029;
        if (!intervalFailed)
        {
            _14029 = intervalFailed;
        }
        else
        {
            _14029 = false;
        }
        bool _14034;
        if (_14029)
        {
            _14034 = jetFailureSite == 0u;
        }
        else
        {
            _14034 = false;
        }
        if (_14034)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
        }
        Interval _12729 = footprint.v;
        Interval _12730 = _14024;
        Interval _14040 = imul(_12729, _12730, intervalFailed, optical_product_upper);
        Interval _12731 = footprint.dx;
        Interval _12732 = _14024;
        Interval _14041 = jet_mul_derivative(_12731, _12732, intervalFailed, optical_product_upper);
        Interval _12733 = _14041;
        Interval _12734 = footprint.v;
        Interval _12735 = Interval{ 0.0, 0.0 };
        Interval _14042 = jet_mul_derivative(_12734, _12735, intervalFailed, optical_product_upper);
        Interval _12736 = _14042;
        Interval _14043 = jet_add_derivative(_12733, _12736, intervalFailed);
        Interval _12737 = footprint.dy;
        Interval _12738 = _14024;
        Interval _14044 = jet_mul_derivative(_12737, _12738, intervalFailed, optical_product_upper);
        Interval _12739 = _14044;
        Interval _12740 = footprint.v;
        Interval _12741 = Interval{ 0.0, 0.0 };
        Interval _14045 = jet_mul_derivative(_12740, _12741, intervalFailed, optical_product_upper);
        Interval _12742 = _14045;
        Interval _14046 = jet_add_derivative(_12739, _12742, intervalFailed);
        bool _14053;
        if (_14040.lo <= 0.0)
        {
            _14053 = _14040.hi >= 0.0;
        }
        else
        {
            _14053 = false;
        }
        float _14060;
        if (_14053)
        {
            _14060 = 0.0;
        }
        else
        {
            _14060 = precise::min(abs(_14040.lo), abs(_14040.hi));
        }
        float _14063 = precise::max(abs(_14040.lo), abs(_14040.hi));
        float _12719 = spvFMul(_14060, _14060);
        float _14064 = interval_down(_12719, intervalFailed);
        float _14065 = precise::max(0.0, _14064);
        float _12720 = spvFMul(_14063, _14063);
        float _14066 = interval_up(_12720, intervalFailed);
        Interval _12721 = Interval{ 2.0, 2.0 };
        Interval _12722 = _14040;
        Interval _14067 = imul(_12721, _12722, intervalFailed, optical_product_upper);
        Interval _12723 = _14067;
        Interval _12724 = _14043;
        Interval _14068 = jet_mul_derivative(_12723, _12724, intervalFailed, optical_product_upper);
        Interval _12725 = Interval{ 2.0, 2.0 };
        Interval _12726 = _14040;
        Interval _14069 = imul(_12725, _12726, intervalFailed, optical_product_upper);
        Interval _12727 = _14069;
        Interval _12728 = _14046;
        Interval _14070 = jet_mul_derivative(_12727, _12728, intervalFailed, optical_product_upper);
        bool _14075;
        if (_14065 <= 0.0)
        {
            _14075 = _14066 >= 0.0;
        }
        else
        {
            _14075 = false;
        }
        float _14082;
        if (_14075)
        {
            _14082 = 0.0;
        }
        else
        {
            _14082 = precise::min(abs(_14065), abs(_14066));
        }
        float _14085 = precise::max(abs(_14065), abs(_14066));
        float _12709 = spvFMul(_14082, _14082);
        float _14086 = interval_down(_12709, intervalFailed);
        float _12710 = spvFMul(_14085, _14085);
        float _14088 = interval_up(_12710, intervalFailed);
        Interval _12711 = Interval{ 2.0, 2.0 };
        Interval _12712 = Interval{ _14065, _14066 };
        Interval _14090 = imul(_12711, _12712, intervalFailed, optical_product_upper);
        Interval _12713 = _14090;
        Interval _12714 = _14068;
        Interval _14091 = jet_mul_derivative(_12713, _12714, intervalFailed, optical_product_upper);
        Interval _12715 = Interval{ 2.0, 2.0 };
        Interval _12716 = Interval{ _14065, _14066 };
        Interval _14093 = imul(_12715, _12716, intervalFailed, optical_product_upper);
        Interval _12717 = _14093;
        Interval _12718 = _14070;
        Interval _14094 = jet_mul_derivative(_12717, _12718, intervalFailed, optical_product_upper);
        Interval _12703 = Interval{ 1.0, 1.0 };
        Interval _12704 = Interval{ precise::max(0.0, _14086), _14088 };
        Interval _14096 = iadd(_12703, _12704, intervalFailed);
        Interval _12705 = Interval{ 0.0, 0.0 };
        Interval _12706 = _14091;
        Interval _14097 = jet_add_derivative(_12705, _12706, intervalFailed);
        Interval _12707 = Interval{ 0.0, 0.0 };
        Interval _12708 = _14094;
        Interval _14098 = jet_add_derivative(_12707, _12708, intervalFailed);
        Interval _12689 = Interval{ 1.0, 1.0 };
        Interval _12690 = _14096;
        Interval _14100 = idiv(_12689, _12690, intervalFailed, interval_divide_upper);
        bool _14107;
        if (!intervalFailed)
        {
            _14107 = intervalFailed;
        }
        else
        {
            _14107 = false;
        }
        bool _14112;
        if (_14107)
        {
            _14112 = jetFailureSite == 0u;
        }
        else
        {
            _14112 = false;
        }
        if (_14112)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14096.lo, _14096.hi);
        }
        Interval _12691 = Interval{ 0.0, 0.0 };
        Interval _12692 = _14100;
        Interval _12693 = _14097;
        Interval _14116 = jet_mul_derivative(_12692, _12693, intervalFailed, optical_product_upper);
        Interval _12694 = Interval{ as_type<float>(as_type<uint>(_14116.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14116.lo) ^ 2147483648u) };
        Interval _14126 = jet_add_derivative(_12691, _12694, intervalFailed);
        Interval _12695 = _14126;
        Interval _12696 = _14096;
        Interval _14127 = jet_div_derivative(_12695, _12696, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12697 = Interval{ 0.0, 0.0 };
        Interval _12698 = _14100;
        Interval _12699 = _14098;
        Interval _14128 = jet_mul_derivative(_12698, _12699, intervalFailed, optical_product_upper);
        Interval _12700 = Interval{ as_type<float>(as_type<uint>(_14128.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14128.lo) ^ 2147483648u) };
        Interval _14138 = jet_add_derivative(_12697, _12700, intervalFailed);
        Interval _12701 = _14138;
        Interval _12702 = _14096;
        Interval _14139 = jet_div_derivative(_12701, _12702, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12675 = _14013;
        Interval _12676 = _14100;
        Interval _14140 = imul(_12675, _12676, intervalFailed, optical_product_upper);
        Interval _12677 = _14017;
        Interval _12678 = _14100;
        Interval _14141 = jet_mul_derivative(_12677, _12678, intervalFailed, optical_product_upper);
        Interval _12679 = _14141;
        Interval _12680 = _14013;
        Interval _12681 = _14127;
        Interval _14142 = jet_mul_derivative(_12680, _12681, intervalFailed, optical_product_upper);
        Interval _12682 = _14142;
        Interval _14143 = jet_add_derivative(_12679, _12682, intervalFailed);
        Interval _12683 = _14021;
        Interval _12684 = _14100;
        Interval _14144 = jet_mul_derivative(_12683, _12684, intervalFailed, optical_product_upper);
        Interval _12685 = _14144;
        Interval _12686 = _14013;
        Interval _12687 = _14139;
        Interval _14145 = jet_mul_derivative(_12686, _12687, intervalFailed, optical_product_upper);
        Interval _12688 = _14145;
        Interval _14146 = jet_add_derivative(_12685, _12688, intervalFailed);
        float _12673 = 25.0;
        float _12674 = 1000.0;
        Interval _14148 = iratio(_12673, _12674, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14153;
        if (!intervalFailed)
        {
            _14153 = intervalFailed;
        }
        else
        {
            _14153 = false;
        }
        bool _14158;
        if (_14153)
        {
            _14158 = jetFailureSite == 0u;
        }
        else
        {
            _14158 = false;
        }
        if (_14158)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
        }
        float _12667 = _13846.lo;
        float _12668 = _13846.hi;
        float _14163 = sine_bounds(_12667, _12668, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14167 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14170 = as_type<float>(as_type<uint>(_14163) ^ 2147483648u);
        Interval _12663 = _13846;
        float _12661 = 3.1415927410125732421875;
        float _14171 = interval_down(_12661, intervalFailed);
        float _12662 = 3.1415927410125732421875;
        float _14172 = interval_up(_12662, intervalFailed);
        Interval _12664 = Interval{ _14171, _14172 };
        Interval _12665 = Interval{ 0.5, 0.5 };
        Interval _14174 = imul(_12664, _12665, intervalFailed, optical_product_upper);
        Interval _12666 = _14174;
        Interval _14175 = iadd(_12663, _12666, intervalFailed);
        float _12659 = _14175.lo;
        float _12660 = _14175.hi;
        float _14178 = sine_bounds(_12659, _12660, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12669 = Interval{ _14167, _14170 };
        Interval _12670 = _13847;
        Interval _14181 = jet_mul_derivative(_12669, _12670, intervalFailed, optical_product_upper);
        Interval _12671 = Interval{ _14167, _14170 };
        Interval _12672 = _13848;
        Interval _14183 = jet_mul_derivative(_12671, _12672, intervalFailed, optical_product_upper);
        Interval _12645 = _14148;
        Interval _12646 = Interval{ _14178, interval_sine_upper };
        Interval _14185 = imul(_12645, _12646, intervalFailed, optical_product_upper);
        Interval _12647 = Interval{ 0.0, 0.0 };
        Interval _12648 = Interval{ _14178, interval_sine_upper };
        Interval _14187 = jet_mul_derivative(_12647, _12648, intervalFailed, optical_product_upper);
        Interval _12649 = _14187;
        Interval _12650 = _14148;
        Interval _12651 = _14181;
        Interval _14188 = jet_mul_derivative(_12650, _12651, intervalFailed, optical_product_upper);
        Interval _12652 = _14188;
        Interval _14189 = jet_add_derivative(_12649, _12652, intervalFailed);
        Interval _12653 = Interval{ 0.0, 0.0 };
        Interval _12654 = Interval{ _14178, interval_sine_upper };
        Interval _14191 = jet_mul_derivative(_12653, _12654, intervalFailed, optical_product_upper);
        Interval _12655 = _14191;
        Interval _12656 = _14148;
        Interval _12657 = _14183;
        Interval _14192 = jet_mul_derivative(_12656, _12657, intervalFailed, optical_product_upper);
        Interval _12658 = _14192;
        Interval _14193 = jet_add_derivative(_12655, _12658, intervalFailed);
        float _12643 = 54.0;
        float _12644 = 1000.0;
        Interval _14196 = iratio(_12643, _12644, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14201;
        if (!intervalFailed)
        {
            _14201 = intervalFailed;
        }
        else
        {
            _14201 = false;
        }
        bool _14206;
        if (_14201)
        {
            _14206 = jetFailureSite == 0u;
        }
        else
        {
            _14206 = false;
        }
        if (_14206)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
        }
        Interval _12629 = footprint.v;
        Interval _12630 = _14196;
        Interval _14212 = imul(_12629, _12630, intervalFailed, optical_product_upper);
        Interval _12631 = footprint.dx;
        Interval _12632 = _14196;
        Interval _14213 = jet_mul_derivative(_12631, _12632, intervalFailed, optical_product_upper);
        Interval _12633 = _14213;
        Interval _12634 = footprint.v;
        Interval _12635 = Interval{ 0.0, 0.0 };
        Interval _14214 = jet_mul_derivative(_12634, _12635, intervalFailed, optical_product_upper);
        Interval _12636 = _14214;
        Interval _14215 = jet_add_derivative(_12633, _12636, intervalFailed);
        Interval _12637 = footprint.dy;
        Interval _12638 = _14196;
        Interval _14216 = jet_mul_derivative(_12637, _12638, intervalFailed, optical_product_upper);
        Interval _12639 = _14216;
        Interval _12640 = footprint.v;
        Interval _12641 = Interval{ 0.0, 0.0 };
        Interval _14217 = jet_mul_derivative(_12640, _12641, intervalFailed, optical_product_upper);
        Interval _12642 = _14217;
        Interval _14218 = jet_add_derivative(_12639, _12642, intervalFailed);
        bool _14225;
        if (_14212.lo <= 0.0)
        {
            _14225 = _14212.hi >= 0.0;
        }
        else
        {
            _14225 = false;
        }
        float _14232;
        if (_14225)
        {
            _14232 = 0.0;
        }
        else
        {
            _14232 = precise::min(abs(_14212.lo), abs(_14212.hi));
        }
        float _14235 = precise::max(abs(_14212.lo), abs(_14212.hi));
        float _12619 = spvFMul(_14232, _14232);
        float _14236 = interval_down(_12619, intervalFailed);
        float _14237 = precise::max(0.0, _14236);
        float _12620 = spvFMul(_14235, _14235);
        float _14238 = interval_up(_12620, intervalFailed);
        Interval _12621 = Interval{ 2.0, 2.0 };
        Interval _12622 = _14212;
        Interval _14239 = imul(_12621, _12622, intervalFailed, optical_product_upper);
        Interval _12623 = _14239;
        Interval _12624 = _14215;
        Interval _14240 = jet_mul_derivative(_12623, _12624, intervalFailed, optical_product_upper);
        Interval _12625 = Interval{ 2.0, 2.0 };
        Interval _12626 = _14212;
        Interval _14241 = imul(_12625, _12626, intervalFailed, optical_product_upper);
        Interval _12627 = _14241;
        Interval _12628 = _14218;
        Interval _14242 = jet_mul_derivative(_12627, _12628, intervalFailed, optical_product_upper);
        bool _14247;
        if (_14237 <= 0.0)
        {
            _14247 = _14238 >= 0.0;
        }
        else
        {
            _14247 = false;
        }
        float _14254;
        if (_14247)
        {
            _14254 = 0.0;
        }
        else
        {
            _14254 = precise::min(abs(_14237), abs(_14238));
        }
        float _14257 = precise::max(abs(_14237), abs(_14238));
        float _12609 = spvFMul(_14254, _14254);
        float _14258 = interval_down(_12609, intervalFailed);
        float _12610 = spvFMul(_14257, _14257);
        float _14260 = interval_up(_12610, intervalFailed);
        Interval _12611 = Interval{ 2.0, 2.0 };
        Interval _12612 = Interval{ _14237, _14238 };
        Interval _14262 = imul(_12611, _12612, intervalFailed, optical_product_upper);
        Interval _12613 = _14262;
        Interval _12614 = _14240;
        Interval _14263 = jet_mul_derivative(_12613, _12614, intervalFailed, optical_product_upper);
        Interval _12615 = Interval{ 2.0, 2.0 };
        Interval _12616 = Interval{ _14237, _14238 };
        Interval _14265 = imul(_12615, _12616, intervalFailed, optical_product_upper);
        Interval _12617 = _14265;
        Interval _12618 = _14242;
        Interval _14266 = jet_mul_derivative(_12617, _12618, intervalFailed, optical_product_upper);
        Interval _12603 = Interval{ 1.0, 1.0 };
        Interval _12604 = Interval{ precise::max(0.0, _14258), _14260 };
        Interval _14268 = iadd(_12603, _12604, intervalFailed);
        Interval _12605 = Interval{ 0.0, 0.0 };
        Interval _12606 = _14263;
        Interval _14269 = jet_add_derivative(_12605, _12606, intervalFailed);
        Interval _12607 = Interval{ 0.0, 0.0 };
        Interval _12608 = _14266;
        Interval _14270 = jet_add_derivative(_12607, _12608, intervalFailed);
        Interval _12589 = Interval{ 1.0, 1.0 };
        Interval _12590 = _14268;
        Interval _14272 = idiv(_12589, _12590, intervalFailed, interval_divide_upper);
        bool _14279;
        if (!intervalFailed)
        {
            _14279 = intervalFailed;
        }
        else
        {
            _14279 = false;
        }
        bool _14284;
        if (_14279)
        {
            _14284 = jetFailureSite == 0u;
        }
        else
        {
            _14284 = false;
        }
        if (_14284)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14268.lo, _14268.hi);
        }
        Interval _12591 = Interval{ 0.0, 0.0 };
        Interval _12592 = _14272;
        Interval _12593 = _14269;
        Interval _14288 = jet_mul_derivative(_12592, _12593, intervalFailed, optical_product_upper);
        Interval _12594 = Interval{ as_type<float>(as_type<uint>(_14288.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14288.lo) ^ 2147483648u) };
        Interval _14298 = jet_add_derivative(_12591, _12594, intervalFailed);
        Interval _12595 = _14298;
        Interval _12596 = _14268;
        Interval _14299 = jet_div_derivative(_12595, _12596, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12597 = Interval{ 0.0, 0.0 };
        Interval _12598 = _14272;
        Interval _12599 = _14270;
        Interval _14300 = jet_mul_derivative(_12598, _12599, intervalFailed, optical_product_upper);
        Interval _12600 = Interval{ as_type<float>(as_type<uint>(_14300.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14300.lo) ^ 2147483648u) };
        Interval _14310 = jet_add_derivative(_12597, _12600, intervalFailed);
        Interval _12601 = _14310;
        Interval _12602 = _14268;
        Interval _14311 = jet_div_derivative(_12601, _12602, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12575 = _14185;
        Interval _12576 = _14272;
        Interval _14312 = imul(_12575, _12576, intervalFailed, optical_product_upper);
        Interval _12577 = _14189;
        Interval _12578 = _14272;
        Interval _14313 = jet_mul_derivative(_12577, _12578, intervalFailed, optical_product_upper);
        Interval _12579 = _14313;
        Interval _12580 = _14185;
        Interval _12581 = _14299;
        Interval _14314 = jet_mul_derivative(_12580, _12581, intervalFailed, optical_product_upper);
        Interval _12582 = _14314;
        Interval _14315 = jet_add_derivative(_12579, _12582, intervalFailed);
        Interval _12583 = _14193;
        Interval _12584 = _14272;
        Interval _14316 = jet_mul_derivative(_12583, _12584, intervalFailed, optical_product_upper);
        Interval _12585 = _14316;
        Interval _12586 = _14185;
        Interval _12587 = _14311;
        Interval _14317 = jet_mul_derivative(_12586, _12587, intervalFailed, optical_product_upper);
        Interval _12588 = _14317;
        Interval _14318 = jet_add_derivative(_12585, _12588, intervalFailed);
        Interval _12569 = _14140;
        Interval _12570 = _14312;
        Interval _14319 = iadd(_12569, _12570, intervalFailed);
        Interval _12571 = _14143;
        Interval _12572 = _14315;
        Interval _14320 = jet_add_derivative(_12571, _12572, intervalFailed);
        Interval _12573 = _14146;
        Interval _12574 = _14318;
        Interval _14321 = jet_add_derivative(_12573, _12574, intervalFailed);
        float _12567 = 45.0;
        float _12568 = 1000.0;
        Interval _14329 = iratio(_12567, _12568, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14334;
        if (!intervalFailed)
        {
            _14334 = intervalFailed;
        }
        else
        {
            _14334 = false;
        }
        bool _14339;
        if (_14334)
        {
            _14339 = jetFailureSite == 0u;
        }
        else
        {
            _14339 = false;
        }
        if (_14339)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(45.0, 45.0, 1000.0, 1000.0);
        }
        float _12561 = _13970.lo;
        float _12562 = _13970.hi;
        float _14344 = sine_bounds(_12561, _12562, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14348 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14351 = as_type<float>(as_type<uint>(_14344) ^ 2147483648u);
        Interval _12557 = _13970;
        float _12555 = 3.1415927410125732421875;
        float _14352 = interval_down(_12555, intervalFailed);
        float _12556 = 3.1415927410125732421875;
        float _14353 = interval_up(_12556, intervalFailed);
        Interval _12558 = Interval{ _14352, _14353 };
        Interval _12559 = Interval{ 0.5, 0.5 };
        Interval _14355 = imul(_12558, _12559, intervalFailed, optical_product_upper);
        Interval _12560 = _14355;
        Interval _14356 = iadd(_12557, _12560, intervalFailed);
        float _12553 = _14356.lo;
        float _12554 = _14356.hi;
        float _14359 = sine_bounds(_12553, _12554, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12563 = Interval{ _14348, _14351 };
        Interval _12564 = _13972;
        Interval _14362 = jet_mul_derivative(_12563, _12564, intervalFailed, optical_product_upper);
        Interval _12565 = Interval{ _14348, _14351 };
        Interval _12566 = _13974;
        Interval _14364 = jet_mul_derivative(_12565, _12566, intervalFailed, optical_product_upper);
        Interval _12539 = _14329;
        Interval _12540 = Interval{ _14359, interval_sine_upper };
        Interval _14366 = imul(_12539, _12540, intervalFailed, optical_product_upper);
        Interval _12541 = Interval{ 0.0, 0.0 };
        Interval _12542 = Interval{ _14359, interval_sine_upper };
        Interval _14368 = jet_mul_derivative(_12541, _12542, intervalFailed, optical_product_upper);
        Interval _12543 = _14368;
        Interval _12544 = _14329;
        Interval _12545 = _14362;
        Interval _14369 = jet_mul_derivative(_12544, _12545, intervalFailed, optical_product_upper);
        Interval _12546 = _14369;
        Interval _14370 = jet_add_derivative(_12543, _12546, intervalFailed);
        Interval _12547 = Interval{ 0.0, 0.0 };
        Interval _12548 = Interval{ _14359, interval_sine_upper };
        Interval _14372 = jet_mul_derivative(_12547, _12548, intervalFailed, optical_product_upper);
        Interval _12549 = _14372;
        Interval _12550 = _14329;
        Interval _12551 = _14364;
        Interval _14373 = jet_mul_derivative(_12550, _12551, intervalFailed, optical_product_upper);
        Interval _12552 = _14373;
        Interval _14374 = jet_add_derivative(_12549, _12552, intervalFailed);
        float _12537 = 24.0;
        float _12538 = 1000.0;
        Interval _14377 = iratio(_12537, _12538, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14382;
        if (!intervalFailed)
        {
            _14382 = intervalFailed;
        }
        else
        {
            _14382 = false;
        }
        bool _14387;
        if (_14382)
        {
            _14387 = jetFailureSite == 0u;
        }
        else
        {
            _14387 = false;
        }
        if (_14387)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(24.0, 24.0, 1000.0, 1000.0);
        }
        Interval _12523 = footprint.v;
        Interval _12524 = _14377;
        Interval _14393 = imul(_12523, _12524, intervalFailed, optical_product_upper);
        Interval _12525 = footprint.dx;
        Interval _12526 = _14377;
        Interval _14394 = jet_mul_derivative(_12525, _12526, intervalFailed, optical_product_upper);
        Interval _12527 = _14394;
        Interval _12528 = footprint.v;
        Interval _12529 = Interval{ 0.0, 0.0 };
        Interval _14395 = jet_mul_derivative(_12528, _12529, intervalFailed, optical_product_upper);
        Interval _12530 = _14395;
        Interval _14396 = jet_add_derivative(_12527, _12530, intervalFailed);
        Interval _12531 = footprint.dy;
        Interval _12532 = _14377;
        Interval _14397 = jet_mul_derivative(_12531, _12532, intervalFailed, optical_product_upper);
        Interval _12533 = _14397;
        Interval _12534 = footprint.v;
        Interval _12535 = Interval{ 0.0, 0.0 };
        Interval _14398 = jet_mul_derivative(_12534, _12535, intervalFailed, optical_product_upper);
        Interval _12536 = _14398;
        Interval _14399 = jet_add_derivative(_12533, _12536, intervalFailed);
        bool _14406;
        if (_14393.lo <= 0.0)
        {
            _14406 = _14393.hi >= 0.0;
        }
        else
        {
            _14406 = false;
        }
        float _14413;
        if (_14406)
        {
            _14413 = 0.0;
        }
        else
        {
            _14413 = precise::min(abs(_14393.lo), abs(_14393.hi));
        }
        float _14416 = precise::max(abs(_14393.lo), abs(_14393.hi));
        float _12513 = spvFMul(_14413, _14413);
        float _14417 = interval_down(_12513, intervalFailed);
        float _14418 = precise::max(0.0, _14417);
        float _12514 = spvFMul(_14416, _14416);
        float _14419 = interval_up(_12514, intervalFailed);
        Interval _12515 = Interval{ 2.0, 2.0 };
        Interval _12516 = _14393;
        Interval _14420 = imul(_12515, _12516, intervalFailed, optical_product_upper);
        Interval _12517 = _14420;
        Interval _12518 = _14396;
        Interval _14421 = jet_mul_derivative(_12517, _12518, intervalFailed, optical_product_upper);
        Interval _12519 = Interval{ 2.0, 2.0 };
        Interval _12520 = _14393;
        Interval _14422 = imul(_12519, _12520, intervalFailed, optical_product_upper);
        Interval _12521 = _14422;
        Interval _12522 = _14399;
        Interval _14423 = jet_mul_derivative(_12521, _12522, intervalFailed, optical_product_upper);
        bool _14428;
        if (_14418 <= 0.0)
        {
            _14428 = _14419 >= 0.0;
        }
        else
        {
            _14428 = false;
        }
        float _14435;
        if (_14428)
        {
            _14435 = 0.0;
        }
        else
        {
            _14435 = precise::min(abs(_14418), abs(_14419));
        }
        float _14438 = precise::max(abs(_14418), abs(_14419));
        float _12503 = spvFMul(_14435, _14435);
        float _14439 = interval_down(_12503, intervalFailed);
        float _12504 = spvFMul(_14438, _14438);
        float _14441 = interval_up(_12504, intervalFailed);
        Interval _12505 = Interval{ 2.0, 2.0 };
        Interval _12506 = Interval{ _14418, _14419 };
        Interval _14443 = imul(_12505, _12506, intervalFailed, optical_product_upper);
        Interval _12507 = _14443;
        Interval _12508 = _14421;
        Interval _14444 = jet_mul_derivative(_12507, _12508, intervalFailed, optical_product_upper);
        Interval _12509 = Interval{ 2.0, 2.0 };
        Interval _12510 = Interval{ _14418, _14419 };
        Interval _14446 = imul(_12509, _12510, intervalFailed, optical_product_upper);
        Interval _12511 = _14446;
        Interval _12512 = _14423;
        Interval _14447 = jet_mul_derivative(_12511, _12512, intervalFailed, optical_product_upper);
        Interval _12497 = Interval{ 1.0, 1.0 };
        Interval _12498 = Interval{ precise::max(0.0, _14439), _14441 };
        Interval _14449 = iadd(_12497, _12498, intervalFailed);
        Interval _12499 = Interval{ 0.0, 0.0 };
        Interval _12500 = _14444;
        Interval _14450 = jet_add_derivative(_12499, _12500, intervalFailed);
        Interval _12501 = Interval{ 0.0, 0.0 };
        Interval _12502 = _14447;
        Interval _14451 = jet_add_derivative(_12501, _12502, intervalFailed);
        Interval _12483 = Interval{ 1.0, 1.0 };
        Interval _12484 = _14449;
        Interval _14453 = idiv(_12483, _12484, intervalFailed, interval_divide_upper);
        bool _14460;
        if (!intervalFailed)
        {
            _14460 = intervalFailed;
        }
        else
        {
            _14460 = false;
        }
        bool _14465;
        if (_14460)
        {
            _14465 = jetFailureSite == 0u;
        }
        else
        {
            _14465 = false;
        }
        if (_14465)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14449.lo, _14449.hi);
        }
        Interval _12485 = Interval{ 0.0, 0.0 };
        Interval _12486 = _14453;
        Interval _12487 = _14450;
        Interval _14469 = jet_mul_derivative(_12486, _12487, intervalFailed, optical_product_upper);
        Interval _12488 = Interval{ as_type<float>(as_type<uint>(_14469.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14469.lo) ^ 2147483648u) };
        Interval _14479 = jet_add_derivative(_12485, _12488, intervalFailed);
        Interval _12489 = _14479;
        Interval _12490 = _14449;
        Interval _14480 = jet_div_derivative(_12489, _12490, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12491 = Interval{ 0.0, 0.0 };
        Interval _12492 = _14453;
        Interval _12493 = _14451;
        Interval _14481 = jet_mul_derivative(_12492, _12493, intervalFailed, optical_product_upper);
        Interval _12494 = Interval{ as_type<float>(as_type<uint>(_14481.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14481.lo) ^ 2147483648u) };
        Interval _14491 = jet_add_derivative(_12491, _12494, intervalFailed);
        Interval _12495 = _14491;
        Interval _12496 = _14449;
        Interval _14492 = jet_div_derivative(_12495, _12496, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12469 = _14366;
        Interval _12470 = _14453;
        Interval _14493 = imul(_12469, _12470, intervalFailed, optical_product_upper);
        Interval _12471 = _14370;
        Interval _12472 = _14453;
        Interval _14494 = jet_mul_derivative(_12471, _12472, intervalFailed, optical_product_upper);
        Interval _12473 = _14494;
        Interval _12474 = _14366;
        Interval _12475 = _14480;
        Interval _14495 = jet_mul_derivative(_12474, _12475, intervalFailed, optical_product_upper);
        Interval _12476 = _14495;
        Interval _14496 = jet_add_derivative(_12473, _12476, intervalFailed);
        Interval _12477 = _14374;
        Interval _12478 = _14453;
        Interval _14497 = jet_mul_derivative(_12477, _12478, intervalFailed, optical_product_upper);
        Interval _12479 = _14497;
        Interval _12480 = _14366;
        Interval _12481 = _14492;
        Interval _14498 = jet_mul_derivative(_12480, _12481, intervalFailed, optical_product_upper);
        Interval _12482 = _14498;
        Interval _14499 = jet_add_derivative(_12479, _12482, intervalFailed);
        float _12467 = 20.0;
        float _12468 = 1000.0;
        Interval _14501 = iratio(_12467, _12468, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14506;
        if (!intervalFailed)
        {
            _14506 = intervalFailed;
        }
        else
        {
            _14506 = false;
        }
        bool _14511;
        if (_14506)
        {
            _14511 = jetFailureSite == 0u;
        }
        else
        {
            _14511 = false;
        }
        if (_14511)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(20.0, 20.0, 1000.0, 1000.0);
        }
        float _12461 = _13846.lo;
        float _12462 = _13846.hi;
        float _14516 = sine_bounds(_12461, _12462, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14520 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14523 = as_type<float>(as_type<uint>(_14516) ^ 2147483648u);
        Interval _12457 = _13846;
        float _12455 = 3.1415927410125732421875;
        float _14524 = interval_down(_12455, intervalFailed);
        float _12456 = 3.1415927410125732421875;
        float _14525 = interval_up(_12456, intervalFailed);
        Interval _12458 = Interval{ _14524, _14525 };
        Interval _12459 = Interval{ 0.5, 0.5 };
        Interval _14527 = imul(_12458, _12459, intervalFailed, optical_product_upper);
        Interval _12460 = _14527;
        Interval _14528 = iadd(_12457, _12460, intervalFailed);
        float _12453 = _14528.lo;
        float _12454 = _14528.hi;
        float _14531 = sine_bounds(_12453, _12454, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12463 = Interval{ _14520, _14523 };
        Interval _12464 = _13847;
        Interval _14534 = jet_mul_derivative(_12463, _12464, intervalFailed, optical_product_upper);
        Interval _12465 = Interval{ _14520, _14523 };
        Interval _12466 = _13848;
        Interval _14536 = jet_mul_derivative(_12465, _12466, intervalFailed, optical_product_upper);
        Interval _12439 = _14501;
        Interval _12440 = Interval{ _14531, interval_sine_upper };
        Interval _14538 = imul(_12439, _12440, intervalFailed, optical_product_upper);
        Interval _12441 = Interval{ 0.0, 0.0 };
        Interval _12442 = Interval{ _14531, interval_sine_upper };
        Interval _14540 = jet_mul_derivative(_12441, _12442, intervalFailed, optical_product_upper);
        Interval _12443 = _14540;
        Interval _12444 = _14501;
        Interval _12445 = _14534;
        Interval _14541 = jet_mul_derivative(_12444, _12445, intervalFailed, optical_product_upper);
        Interval _12446 = _14541;
        Interval _14542 = jet_add_derivative(_12443, _12446, intervalFailed);
        Interval _12447 = Interval{ 0.0, 0.0 };
        Interval _12448 = Interval{ _14531, interval_sine_upper };
        Interval _14544 = jet_mul_derivative(_12447, _12448, intervalFailed, optical_product_upper);
        Interval _12449 = _14544;
        Interval _12450 = _14501;
        Interval _12451 = _14536;
        Interval _14545 = jet_mul_derivative(_12450, _12451, intervalFailed, optical_product_upper);
        Interval _12452 = _14545;
        Interval _14546 = jet_add_derivative(_12449, _12452, intervalFailed);
        float _12437 = 54.0;
        float _12438 = 1000.0;
        Interval _14549 = iratio(_12437, _12438, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14554;
        if (!intervalFailed)
        {
            _14554 = intervalFailed;
        }
        else
        {
            _14554 = false;
        }
        bool _14559;
        if (_14554)
        {
            _14559 = jetFailureSite == 0u;
        }
        else
        {
            _14559 = false;
        }
        if (_14559)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
        }
        Interval _12423 = footprint.v;
        Interval _12424 = _14549;
        Interval _14565 = imul(_12423, _12424, intervalFailed, optical_product_upper);
        Interval _12425 = footprint.dx;
        Interval _12426 = _14549;
        Interval _14566 = jet_mul_derivative(_12425, _12426, intervalFailed, optical_product_upper);
        Interval _12427 = _14566;
        Interval _12428 = footprint.v;
        Interval _12429 = Interval{ 0.0, 0.0 };
        Interval _14567 = jet_mul_derivative(_12428, _12429, intervalFailed, optical_product_upper);
        Interval _12430 = _14567;
        Interval _14568 = jet_add_derivative(_12427, _12430, intervalFailed);
        Interval _12431 = footprint.dy;
        Interval _12432 = _14549;
        Interval _14569 = jet_mul_derivative(_12431, _12432, intervalFailed, optical_product_upper);
        Interval _12433 = _14569;
        Interval _12434 = footprint.v;
        Interval _12435 = Interval{ 0.0, 0.0 };
        Interval _14570 = jet_mul_derivative(_12434, _12435, intervalFailed, optical_product_upper);
        Interval _12436 = _14570;
        Interval _14571 = jet_add_derivative(_12433, _12436, intervalFailed);
        bool _14578;
        if (_14565.lo <= 0.0)
        {
            _14578 = _14565.hi >= 0.0;
        }
        else
        {
            _14578 = false;
        }
        float _14585;
        if (_14578)
        {
            _14585 = 0.0;
        }
        else
        {
            _14585 = precise::min(abs(_14565.lo), abs(_14565.hi));
        }
        float _14588 = precise::max(abs(_14565.lo), abs(_14565.hi));
        float _12413 = spvFMul(_14585, _14585);
        float _14589 = interval_down(_12413, intervalFailed);
        float _14590 = precise::max(0.0, _14589);
        float _12414 = spvFMul(_14588, _14588);
        float _14591 = interval_up(_12414, intervalFailed);
        Interval _12415 = Interval{ 2.0, 2.0 };
        Interval _12416 = _14565;
        Interval _14592 = imul(_12415, _12416, intervalFailed, optical_product_upper);
        Interval _12417 = _14592;
        Interval _12418 = _14568;
        Interval _14593 = jet_mul_derivative(_12417, _12418, intervalFailed, optical_product_upper);
        Interval _12419 = Interval{ 2.0, 2.0 };
        Interval _12420 = _14565;
        Interval _14594 = imul(_12419, _12420, intervalFailed, optical_product_upper);
        Interval _12421 = _14594;
        Interval _12422 = _14571;
        Interval _14595 = jet_mul_derivative(_12421, _12422, intervalFailed, optical_product_upper);
        bool _14600;
        if (_14590 <= 0.0)
        {
            _14600 = _14591 >= 0.0;
        }
        else
        {
            _14600 = false;
        }
        float _14607;
        if (_14600)
        {
            _14607 = 0.0;
        }
        else
        {
            _14607 = precise::min(abs(_14590), abs(_14591));
        }
        float _14610 = precise::max(abs(_14590), abs(_14591));
        float _12403 = spvFMul(_14607, _14607);
        float _14611 = interval_down(_12403, intervalFailed);
        float _12404 = spvFMul(_14610, _14610);
        float _14613 = interval_up(_12404, intervalFailed);
        Interval _12405 = Interval{ 2.0, 2.0 };
        Interval _12406 = Interval{ _14590, _14591 };
        Interval _14615 = imul(_12405, _12406, intervalFailed, optical_product_upper);
        Interval _12407 = _14615;
        Interval _12408 = _14593;
        Interval _14616 = jet_mul_derivative(_12407, _12408, intervalFailed, optical_product_upper);
        Interval _12409 = Interval{ 2.0, 2.0 };
        Interval _12410 = Interval{ _14590, _14591 };
        Interval _14618 = imul(_12409, _12410, intervalFailed, optical_product_upper);
        Interval _12411 = _14618;
        Interval _12412 = _14595;
        Interval _14619 = jet_mul_derivative(_12411, _12412, intervalFailed, optical_product_upper);
        Interval _12397 = Interval{ 1.0, 1.0 };
        Interval _12398 = Interval{ precise::max(0.0, _14611), _14613 };
        Interval _14621 = iadd(_12397, _12398, intervalFailed);
        Interval _12399 = Interval{ 0.0, 0.0 };
        Interval _12400 = _14616;
        Interval _14622 = jet_add_derivative(_12399, _12400, intervalFailed);
        Interval _12401 = Interval{ 0.0, 0.0 };
        Interval _12402 = _14619;
        Interval _14623 = jet_add_derivative(_12401, _12402, intervalFailed);
        Interval _12383 = Interval{ 1.0, 1.0 };
        Interval _12384 = _14621;
        Interval _14625 = idiv(_12383, _12384, intervalFailed, interval_divide_upper);
        bool _14632;
        if (!intervalFailed)
        {
            _14632 = intervalFailed;
        }
        else
        {
            _14632 = false;
        }
        bool _14637;
        if (_14632)
        {
            _14637 = jetFailureSite == 0u;
        }
        else
        {
            _14637 = false;
        }
        if (_14637)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14621.lo, _14621.hi);
        }
        Interval _12385 = Interval{ 0.0, 0.0 };
        Interval _12386 = _14625;
        Interval _12387 = _14622;
        Interval _14641 = jet_mul_derivative(_12386, _12387, intervalFailed, optical_product_upper);
        Interval _12388 = Interval{ as_type<float>(as_type<uint>(_14641.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14641.lo) ^ 2147483648u) };
        Interval _14651 = jet_add_derivative(_12385, _12388, intervalFailed);
        Interval _12389 = _14651;
        Interval _12390 = _14621;
        Interval _14652 = jet_div_derivative(_12389, _12390, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12391 = Interval{ 0.0, 0.0 };
        Interval _12392 = _14625;
        Interval _12393 = _14623;
        Interval _14653 = jet_mul_derivative(_12392, _12393, intervalFailed, optical_product_upper);
        Interval _12394 = Interval{ as_type<float>(as_type<uint>(_14653.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14653.lo) ^ 2147483648u) };
        Interval _14663 = jet_add_derivative(_12391, _12394, intervalFailed);
        Interval _12395 = _14663;
        Interval _12396 = _14621;
        Interval _14664 = jet_div_derivative(_12395, _12396, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12369 = _14538;
        Interval _12370 = _14625;
        Interval _14665 = imul(_12369, _12370, intervalFailed, optical_product_upper);
        Interval _12371 = _14542;
        Interval _12372 = _14625;
        Interval _14666 = jet_mul_derivative(_12371, _12372, intervalFailed, optical_product_upper);
        Interval _12373 = _14666;
        Interval _12374 = _14538;
        Interval _12375 = _14652;
        Interval _14667 = jet_mul_derivative(_12374, _12375, intervalFailed, optical_product_upper);
        Interval _12376 = _14667;
        Interval _14668 = jet_add_derivative(_12373, _12376, intervalFailed);
        Interval _12377 = _14546;
        Interval _12378 = _14625;
        Interval _14669 = jet_mul_derivative(_12377, _12378, intervalFailed, optical_product_upper);
        Interval _12379 = _14669;
        Interval _12380 = _14538;
        Interval _12381 = _14664;
        Interval _14670 = jet_mul_derivative(_12380, _12381, intervalFailed, optical_product_upper);
        Interval _12382 = _14670;
        Interval _14671 = jet_add_derivative(_12379, _12382, intervalFailed);
        Interval _12363 = _14493;
        Interval _12364 = Interval{ as_type<float>(as_type<uint>(_14665.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14665.lo) ^ 2147483648u) };
        Interval _14697 = iadd(_12363, _12364, intervalFailed);
        Interval _12365 = _14496;
        Interval _12366 = Interval{ as_type<float>(as_type<uint>(_14668.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14668.lo) ^ 2147483648u) };
        Interval _14699 = jet_add_derivative(_12365, _12366, intervalFailed);
        Interval _12367 = _14499;
        Interval _12368 = Interval{ as_type<float>(as_type<uint>(_14671.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14671.lo) ^ 2147483648u) };
        Interval _14701 = jet_add_derivative(_12367, _12368, intervalFailed);
        _15239 = _14697.lo;
        _15240 = _14697.hi;
        _15241 = _14699.lo;
        _15242 = _14699.hi;
        _15243 = _14701.lo;
        _15244 = _14701.hi;
        _15245 = _14319.lo;
        _15246 = _14319.hi;
        _15247 = _14320.lo;
        _15248 = _14320.hi;
        _15249 = _14321.lo;
        _15250 = _14321.hi;
    }
    Interval _12349 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12350 = Interval{ _15245, _15246 };
    Interval _15260 = imul(_12349, _12350, intervalFailed, optical_product_upper);
    Interval _12351 = Interval{ 0.0, 0.0 };
    Interval _12352 = Interval{ _15245, _15246 };
    Interval _15262 = jet_mul_derivative(_12351, _12352, intervalFailed, optical_product_upper);
    Interval _12353 = _15262;
    Interval _12354 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12355 = Interval{ _15247, _15248 };
    Interval _15265 = jet_mul_derivative(_12354, _12355, intervalFailed, optical_product_upper);
    Interval _12356 = _15265;
    Interval _15266 = jet_add_derivative(_12353, _12356, intervalFailed);
    Interval _12357 = Interval{ 0.0, 0.0 };
    Interval _12358 = Interval{ _15245, _15246 };
    Interval _15268 = jet_mul_derivative(_12357, _12358, intervalFailed, optical_product_upper);
    Interval _12359 = _15268;
    Interval _12360 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12361 = Interval{ _15249, _15250 };
    Interval _15271 = jet_mul_derivative(_12360, _12361, intervalFailed, optical_product_upper);
    Interval _12362 = _15271;
    Interval _15272 = jet_add_derivative(_12359, _12362, intervalFailed);
    Interval _12335 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12336 = Interval{ -1.0, -1.0 };
    Interval _15274 = imul(_12335, _12336, intervalFailed, optical_product_upper);
    Interval _12337 = Interval{ 0.0, 0.0 };
    Interval _12338 = Interval{ -1.0, -1.0 };
    Interval _15275 = jet_mul_derivative(_12337, _12338, intervalFailed, optical_product_upper);
    Interval _12339 = _15275;
    Interval _12340 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12341 = Interval{ 0.0, 0.0 };
    Interval _15277 = jet_mul_derivative(_12340, _12341, intervalFailed, optical_product_upper);
    Interval _12342 = _15277;
    Interval _15278 = jet_add_derivative(_12339, _12342, intervalFailed);
    Interval _12343 = Interval{ 0.0, 0.0 };
    Interval _12344 = Interval{ -1.0, -1.0 };
    Interval _15279 = jet_mul_derivative(_12343, _12344, intervalFailed, optical_product_upper);
    Interval _12345 = _15279;
    Interval _12346 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12347 = Interval{ 0.0, 0.0 };
    Interval _15281 = jet_mul_derivative(_12346, _12347, intervalFailed, optical_product_upper);
    Interval _12348 = _15281;
    Interval _15282 = jet_add_derivative(_12345, _12348, intervalFailed);
    Interval _12329 = _15260;
    Interval _12330 = _15274;
    Interval _15283 = iadd(_12329, _12330, intervalFailed);
    Interval _12331 = _15266;
    Interval _12332 = _15278;
    Interval _15284 = jet_add_derivative(_12331, _12332, intervalFailed);
    Interval _12333 = _15272;
    Interval _12334 = _15282;
    Interval _15285 = jet_add_derivative(_12333, _12334, intervalFailed);
    Interval _12315 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12316 = Interval{ _15239, _15240 };
    Interval _15288 = imul(_12315, _12316, intervalFailed, optical_product_upper);
    Interval _12317 = Interval{ 0.0, 0.0 };
    Interval _12318 = Interval{ _15239, _15240 };
    Interval _15290 = jet_mul_derivative(_12317, _12318, intervalFailed, optical_product_upper);
    Interval _12319 = _15290;
    Interval _12320 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12321 = Interval{ _15241, _15242 };
    Interval _15293 = jet_mul_derivative(_12320, _12321, intervalFailed, optical_product_upper);
    Interval _12322 = _15293;
    Interval _15294 = jet_add_derivative(_12319, _12322, intervalFailed);
    Interval _12323 = Interval{ 0.0, 0.0 };
    Interval _12324 = Interval{ _15239, _15240 };
    Interval _15296 = jet_mul_derivative(_12323, _12324, intervalFailed, optical_product_upper);
    Interval _12325 = _15296;
    Interval _12326 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12327 = Interval{ _15243, _15244 };
    Interval _15299 = jet_mul_derivative(_12326, _12327, intervalFailed, optical_product_upper);
    Interval _12328 = _15299;
    Interval _15300 = jet_add_derivative(_12325, _12328, intervalFailed);
    Interval _12309 = _15283;
    Interval _12310 = _15288;
    Interval _15301 = iadd(_12309, _12310, intervalFailed);
    Interval _12311 = _15284;
    Interval _12312 = _15294;
    Interval _15302 = jet_add_derivative(_12311, _12312, intervalFailed);
    Interval _12313 = _15285;
    Interval _12314 = _15300;
    Interval _15303 = jet_add_derivative(_12313, _12314, intervalFailed);
    Interval _12295 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12296 = Interval{ _15245, _15246 };
    Interval _15309 = imul(_12295, _12296, intervalFailed, optical_product_upper);
    Interval _12297 = Interval{ 0.0, 0.0 };
    Interval _12298 = Interval{ _15245, _15246 };
    Interval _15311 = jet_mul_derivative(_12297, _12298, intervalFailed, optical_product_upper);
    Interval _12299 = _15311;
    Interval _12300 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12301 = Interval{ _15247, _15248 };
    Interval _15314 = jet_mul_derivative(_12300, _12301, intervalFailed, optical_product_upper);
    Interval _12302 = _15314;
    Interval _15315 = jet_add_derivative(_12299, _12302, intervalFailed);
    Interval _12303 = Interval{ 0.0, 0.0 };
    Interval _12304 = Interval{ _15245, _15246 };
    Interval _15317 = jet_mul_derivative(_12303, _12304, intervalFailed, optical_product_upper);
    Interval _12305 = _15317;
    Interval _12306 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12307 = Interval{ _15249, _15250 };
    Interval _15320 = jet_mul_derivative(_12306, _12307, intervalFailed, optical_product_upper);
    Interval _12308 = _15320;
    Interval _15321 = jet_add_derivative(_12305, _12308, intervalFailed);
    Interval _12281 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12282 = Interval{ -1.0, -1.0 };
    Interval _15323 = imul(_12281, _12282, intervalFailed, optical_product_upper);
    Interval _12283 = Interval{ 0.0, 0.0 };
    Interval _12284 = Interval{ -1.0, -1.0 };
    Interval _15324 = jet_mul_derivative(_12283, _12284, intervalFailed, optical_product_upper);
    Interval _12285 = _15324;
    Interval _12286 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12287 = Interval{ 0.0, 0.0 };
    Interval _15326 = jet_mul_derivative(_12286, _12287, intervalFailed, optical_product_upper);
    Interval _12288 = _15326;
    Interval _15327 = jet_add_derivative(_12285, _12288, intervalFailed);
    Interval _12289 = Interval{ 0.0, 0.0 };
    Interval _12290 = Interval{ -1.0, -1.0 };
    Interval _15328 = jet_mul_derivative(_12289, _12290, intervalFailed, optical_product_upper);
    Interval _12291 = _15328;
    Interval _12292 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12293 = Interval{ 0.0, 0.0 };
    Interval _15330 = jet_mul_derivative(_12292, _12293, intervalFailed, optical_product_upper);
    Interval _12294 = _15330;
    Interval _15331 = jet_add_derivative(_12291, _12294, intervalFailed);
    Interval _12275 = _15309;
    Interval _12276 = _15323;
    Interval _15332 = iadd(_12275, _12276, intervalFailed);
    Interval _12277 = _15315;
    Interval _12278 = _15327;
    Interval _15333 = jet_add_derivative(_12277, _12278, intervalFailed);
    Interval _12279 = _15321;
    Interval _12280 = _15331;
    Interval _15334 = jet_add_derivative(_12279, _12280, intervalFailed);
    Interval _12261 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12262 = Interval{ _15239, _15240 };
    Interval _15337 = imul(_12261, _12262, intervalFailed, optical_product_upper);
    Interval _12263 = Interval{ 0.0, 0.0 };
    Interval _12264 = Interval{ _15239, _15240 };
    Interval _15339 = jet_mul_derivative(_12263, _12264, intervalFailed, optical_product_upper);
    Interval _12265 = _15339;
    Interval _12266 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12267 = Interval{ _15241, _15242 };
    Interval _15342 = jet_mul_derivative(_12266, _12267, intervalFailed, optical_product_upper);
    Interval _12268 = _15342;
    Interval _15343 = jet_add_derivative(_12265, _12268, intervalFailed);
    Interval _12269 = Interval{ 0.0, 0.0 };
    Interval _12270 = Interval{ _15239, _15240 };
    Interval _15345 = jet_mul_derivative(_12269, _12270, intervalFailed, optical_product_upper);
    Interval _12271 = _15345;
    Interval _12272 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12273 = Interval{ _15243, _15244 };
    Interval _15348 = jet_mul_derivative(_12272, _12273, intervalFailed, optical_product_upper);
    Interval _12274 = _15348;
    Interval _15349 = jet_add_derivative(_12271, _12274, intervalFailed);
    Interval _12255 = _15332;
    Interval _12256 = _15337;
    Interval _15350 = iadd(_12255, _12256, intervalFailed);
    Interval _12257 = _15333;
    Interval _12258 = _15343;
    Interval _15351 = jet_add_derivative(_12257, _12258, intervalFailed);
    Interval _12259 = _15334;
    Interval _12260 = _15349;
    Interval _15352 = jet_add_derivative(_12259, _12260, intervalFailed);
    Interval _12241 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12242 = Interval{ _15245, _15246 };
    Interval _15358 = imul(_12241, _12242, intervalFailed, optical_product_upper);
    Interval _12243 = Interval{ 0.0, 0.0 };
    Interval _12244 = Interval{ _15245, _15246 };
    Interval _15360 = jet_mul_derivative(_12243, _12244, intervalFailed, optical_product_upper);
    Interval _12245 = _15360;
    Interval _12246 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12247 = Interval{ _15247, _15248 };
    Interval _15363 = jet_mul_derivative(_12246, _12247, intervalFailed, optical_product_upper);
    Interval _12248 = _15363;
    Interval _15364 = jet_add_derivative(_12245, _12248, intervalFailed);
    Interval _12249 = Interval{ 0.0, 0.0 };
    Interval _12250 = Interval{ _15245, _15246 };
    Interval _15366 = jet_mul_derivative(_12249, _12250, intervalFailed, optical_product_upper);
    Interval _12251 = _15366;
    Interval _12252 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12253 = Interval{ _15249, _15250 };
    Interval _15369 = jet_mul_derivative(_12252, _12253, intervalFailed, optical_product_upper);
    Interval _12254 = _15369;
    Interval _15370 = jet_add_derivative(_12251, _12254, intervalFailed);
    Interval _12227 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12228 = Interval{ -1.0, -1.0 };
    Interval _15372 = imul(_12227, _12228, intervalFailed, optical_product_upper);
    Interval _12229 = Interval{ 0.0, 0.0 };
    Interval _12230 = Interval{ -1.0, -1.0 };
    Interval _15373 = jet_mul_derivative(_12229, _12230, intervalFailed, optical_product_upper);
    Interval _12231 = _15373;
    Interval _12232 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12233 = Interval{ 0.0, 0.0 };
    Interval _15375 = jet_mul_derivative(_12232, _12233, intervalFailed, optical_product_upper);
    Interval _12234 = _15375;
    Interval _15376 = jet_add_derivative(_12231, _12234, intervalFailed);
    Interval _12235 = Interval{ 0.0, 0.0 };
    Interval _12236 = Interval{ -1.0, -1.0 };
    Interval _15377 = jet_mul_derivative(_12235, _12236, intervalFailed, optical_product_upper);
    Interval _12237 = _15377;
    Interval _12238 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12239 = Interval{ 0.0, 0.0 };
    Interval _15379 = jet_mul_derivative(_12238, _12239, intervalFailed, optical_product_upper);
    Interval _12240 = _15379;
    Interval _15380 = jet_add_derivative(_12237, _12240, intervalFailed);
    Interval _12221 = _15358;
    Interval _12222 = _15372;
    Interval _15381 = iadd(_12221, _12222, intervalFailed);
    Interval _12223 = _15364;
    Interval _12224 = _15376;
    Interval _15382 = jet_add_derivative(_12223, _12224, intervalFailed);
    Interval _12225 = _15370;
    Interval _12226 = _15380;
    Interval _15383 = jet_add_derivative(_12225, _12226, intervalFailed);
    Interval _12207 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12208 = Interval{ _15239, _15240 };
    Interval _15386 = imul(_12207, _12208, intervalFailed, optical_product_upper);
    Interval _12209 = Interval{ 0.0, 0.0 };
    Interval _12210 = Interval{ _15239, _15240 };
    Interval _15388 = jet_mul_derivative(_12209, _12210, intervalFailed, optical_product_upper);
    Interval _12211 = _15388;
    Interval _12212 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12213 = Interval{ _15241, _15242 };
    Interval _15391 = jet_mul_derivative(_12212, _12213, intervalFailed, optical_product_upper);
    Interval _12214 = _15391;
    Interval _15392 = jet_add_derivative(_12211, _12214, intervalFailed);
    Interval _12215 = Interval{ 0.0, 0.0 };
    Interval _12216 = Interval{ _15239, _15240 };
    Interval _15394 = jet_mul_derivative(_12215, _12216, intervalFailed, optical_product_upper);
    Interval _12217 = _15394;
    Interval _12218 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12219 = Interval{ _15243, _15244 };
    Interval _15397 = jet_mul_derivative(_12218, _12219, intervalFailed, optical_product_upper);
    Interval _12220 = _15397;
    Interval _15398 = jet_add_derivative(_12217, _12220, intervalFailed);
    Interval _12201 = _15381;
    Interval _12202 = _15386;
    Interval _15399 = iadd(_12201, _12202, intervalFailed);
    Interval _12203 = _15382;
    Interval _12204 = _15392;
    Interval _15400 = jet_add_derivative(_12203, _12204, intervalFailed);
    Interval _12205 = _15383;
    Interval _12206 = _15398;
    Interval _15401 = jet_add_derivative(_12205, _12206, intervalFailed);
    bool _15408;
    if (_15301.lo <= 0.0)
    {
        _15408 = _15301.hi >= 0.0;
    }
    else
    {
        _15408 = false;
    }
    float _15415;
    if (_15408)
    {
        _15415 = 0.0;
    }
    else
    {
        _15415 = precise::min(abs(_15301.lo), abs(_15301.hi));
    }
    float _15418 = precise::max(abs(_15301.lo), abs(_15301.hi));
    float _12191 = spvFMul(_15415, _15415);
    float _15419 = interval_down(_12191, intervalFailed);
    float _12192 = spvFMul(_15418, _15418);
    float _15421 = interval_up(_12192, intervalFailed);
    Interval _12193 = Interval{ 2.0, 2.0 };
    Interval _12194 = _15301;
    Interval _15422 = imul(_12193, _12194, intervalFailed, optical_product_upper);
    Interval _12195 = _15422;
    Interval _12196 = _15302;
    Interval _15423 = jet_mul_derivative(_12195, _12196, intervalFailed, optical_product_upper);
    Interval _12197 = Interval{ 2.0, 2.0 };
    Interval _12198 = _15301;
    Interval _15424 = imul(_12197, _12198, intervalFailed, optical_product_upper);
    Interval _12199 = _15424;
    Interval _12200 = _15303;
    Interval _15425 = jet_mul_derivative(_12199, _12200, intervalFailed, optical_product_upper);
    bool _15432;
    if (_15350.lo <= 0.0)
    {
        _15432 = _15350.hi >= 0.0;
    }
    else
    {
        _15432 = false;
    }
    float _15439;
    if (_15432)
    {
        _15439 = 0.0;
    }
    else
    {
        _15439 = precise::min(abs(_15350.lo), abs(_15350.hi));
    }
    float _15442 = precise::max(abs(_15350.lo), abs(_15350.hi));
    float _12181 = spvFMul(_15439, _15439);
    float _15443 = interval_down(_12181, intervalFailed);
    float _12182 = spvFMul(_15442, _15442);
    float _15445 = interval_up(_12182, intervalFailed);
    Interval _12183 = Interval{ 2.0, 2.0 };
    Interval _12184 = _15350;
    Interval _15446 = imul(_12183, _12184, intervalFailed, optical_product_upper);
    Interval _12185 = _15446;
    Interval _12186 = _15351;
    Interval _15447 = jet_mul_derivative(_12185, _12186, intervalFailed, optical_product_upper);
    Interval _12187 = Interval{ 2.0, 2.0 };
    Interval _12188 = _15350;
    Interval _15448 = imul(_12187, _12188, intervalFailed, optical_product_upper);
    Interval _12189 = _15448;
    Interval _12190 = _15352;
    Interval _15449 = jet_mul_derivative(_12189, _12190, intervalFailed, optical_product_upper);
    Interval _12175 = Interval{ precise::max(0.0, _15419), _15421 };
    Interval _12176 = Interval{ precise::max(0.0, _15443), _15445 };
    Interval _15452 = iadd(_12175, _12176, intervalFailed);
    Interval _12177 = _15423;
    Interval _12178 = _15447;
    Interval _15453 = jet_add_derivative(_12177, _12178, intervalFailed);
    Interval _12179 = _15425;
    Interval _12180 = _15449;
    Interval _15454 = jet_add_derivative(_12179, _12180, intervalFailed);
    bool _15461;
    if (_15399.lo <= 0.0)
    {
        _15461 = _15399.hi >= 0.0;
    }
    else
    {
        _15461 = false;
    }
    float _15468;
    if (_15461)
    {
        _15468 = 0.0;
    }
    else
    {
        _15468 = precise::min(abs(_15399.lo), abs(_15399.hi));
    }
    float _15471 = precise::max(abs(_15399.lo), abs(_15399.hi));
    float _12165 = spvFMul(_15468, _15468);
    float _15472 = interval_down(_12165, intervalFailed);
    float _12166 = spvFMul(_15471, _15471);
    float _15474 = interval_up(_12166, intervalFailed);
    Interval _12167 = Interval{ 2.0, 2.0 };
    Interval _12168 = _15399;
    Interval _15475 = imul(_12167, _12168, intervalFailed, optical_product_upper);
    Interval _12169 = _15475;
    Interval _12170 = _15400;
    Interval _15476 = jet_mul_derivative(_12169, _12170, intervalFailed, optical_product_upper);
    Interval _12171 = Interval{ 2.0, 2.0 };
    Interval _12172 = _15399;
    Interval _15477 = imul(_12171, _12172, intervalFailed, optical_product_upper);
    Interval _12173 = _15477;
    Interval _12174 = _15401;
    Interval _15478 = jet_mul_derivative(_12173, _12174, intervalFailed, optical_product_upper);
    Interval _12159 = _15452;
    Interval _12160 = Interval{ precise::max(0.0, _15472), _15474 };
    Interval _15480 = iadd(_12159, _12160, intervalFailed);
    Interval _12161 = _15453;
    Interval _12162 = _15476;
    Interval _15481 = jet_add_derivative(_12161, _12162, intervalFailed);
    Interval _12163 = _15454;
    Interval _12164 = _15478;
    Interval _15482 = jet_add_derivative(_12163, _12164, intervalFailed);
    Interval _12150 = _15480;
    Interval _15484 = isqrt(_12150, intervalFailed);
    bool _15492;
    if (!intervalFailed)
    {
        _15492 = intervalFailed;
    }
    else
    {
        _15492 = false;
    }
    bool _15497;
    if (_15492)
    {
        _15497 = jetFailureSite == 0u;
    }
    else
    {
        _15497 = false;
    }
    if (_15497)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_15480.lo, _15480.hi, 0.0, 0.0);
    }
    if (_15484.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _12151 = Interval{ 2.0, 2.0 };
    Interval _12152 = _15484;
    Interval _15504 = imul(_12151, _12152, intervalFailed, optical_product_upper);
    Interval _12153 = Interval{ 1.0, 1.0 };
    Interval _12154 = _15504;
    Interval _15506 = idiv(_12153, _12154, intervalFailed, interval_divide_upper);
    bool _15513;
    if (!intervalFailed)
    {
        _15513 = intervalFailed;
    }
    else
    {
        _15513 = false;
    }
    bool _15518;
    if (_15513)
    {
        _15518 = jetFailureSite == 0u;
    }
    else
    {
        _15518 = false;
    }
    if (_15518)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _15504.lo, _15504.hi);
    }
    Interval _12155 = _15506;
    Interval _12156 = _15481;
    Interval _15522 = jet_mul_derivative(_12155, _12156, intervalFailed, optical_product_upper);
    Interval _12157 = _15506;
    Interval _12158 = _15482;
    Interval _15523 = jet_mul_derivative(_12157, _12158, intervalFailed, optical_product_upper);
    Interval _12136 = Interval{ 1.0, 1.0 };
    Interval _12137 = _15484;
    Interval _15525 = idiv(_12136, _12137, intervalFailed, interval_divide_upper);
    bool _15532;
    if (!intervalFailed)
    {
        _15532 = intervalFailed;
    }
    else
    {
        _15532 = false;
    }
    bool _15537;
    if (_15532)
    {
        _15537 = jetFailureSite == 0u;
    }
    else
    {
        _15537 = false;
    }
    if (_15537)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _15484.lo, _15484.hi);
    }
    Interval _12138 = Interval{ 0.0, 0.0 };
    Interval _12139 = _15525;
    Interval _12140 = _15522;
    Interval _15541 = jet_mul_derivative(_12139, _12140, intervalFailed, optical_product_upper);
    Interval _12141 = Interval{ as_type<float>(as_type<uint>(_15541.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15541.lo) ^ 2147483648u) };
    Interval _15551 = jet_add_derivative(_12138, _12141, intervalFailed);
    Interval _12142 = _15551;
    Interval _12143 = _15484;
    Interval _15552 = jet_div_derivative(_12142, _12143, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _12144 = Interval{ 0.0, 0.0 };
    Interval _12145 = _15525;
    Interval _12146 = _15523;
    Interval _15553 = jet_mul_derivative(_12145, _12146, intervalFailed, optical_product_upper);
    Interval _12147 = Interval{ as_type<float>(as_type<uint>(_15553.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15553.lo) ^ 2147483648u) };
    Interval _15563 = jet_add_derivative(_12144, _12147, intervalFailed);
    Interval _12148 = _15563;
    Interval _12149 = _15484;
    Interval _15564 = jet_div_derivative(_12148, _12149, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _12122 = _15301;
    Interval _12123 = _15525;
    Interval _15565 = imul(_12122, _12123, intervalFailed, optical_product_upper);
    Interval _12124 = _15302;
    Interval _12125 = _15525;
    Interval _15566 = jet_mul_derivative(_12124, _12125, intervalFailed, optical_product_upper);
    Interval _12126 = _15566;
    Interval _12127 = _15301;
    Interval _12128 = _15552;
    Interval _15567 = jet_mul_derivative(_12127, _12128, intervalFailed, optical_product_upper);
    Interval _12129 = _15567;
    Interval _15568 = jet_add_derivative(_12126, _12129, intervalFailed);
    Interval _12130 = _15303;
    Interval _12131 = _15525;
    Interval _15569 = jet_mul_derivative(_12130, _12131, intervalFailed, optical_product_upper);
    Interval _12132 = _15569;
    Interval _12133 = _15301;
    Interval _12134 = _15564;
    Interval _15570 = jet_mul_derivative(_12133, _12134, intervalFailed, optical_product_upper);
    Interval _12135 = _15570;
    Interval _15571 = jet_add_derivative(_12132, _12135, intervalFailed);
    Interval _12108 = _15350;
    Interval _12109 = _15525;
    Interval _15572 = imul(_12108, _12109, intervalFailed, optical_product_upper);
    Interval _12110 = _15351;
    Interval _12111 = _15525;
    Interval _15573 = jet_mul_derivative(_12110, _12111, intervalFailed, optical_product_upper);
    Interval _12112 = _15573;
    Interval _12113 = _15350;
    Interval _12114 = _15552;
    Interval _15574 = jet_mul_derivative(_12113, _12114, intervalFailed, optical_product_upper);
    Interval _12115 = _15574;
    Interval _15575 = jet_add_derivative(_12112, _12115, intervalFailed);
    Interval _12116 = _15352;
    Interval _12117 = _15525;
    Interval _15576 = jet_mul_derivative(_12116, _12117, intervalFailed, optical_product_upper);
    Interval _12118 = _15576;
    Interval _12119 = _15350;
    Interval _12120 = _15564;
    Interval _15577 = jet_mul_derivative(_12119, _12120, intervalFailed, optical_product_upper);
    Interval _12121 = _15577;
    Interval _15578 = jet_add_derivative(_12118, _12121, intervalFailed);
    Interval _12094 = _15399;
    Interval _12095 = _15525;
    Interval _15579 = imul(_12094, _12095, intervalFailed, optical_product_upper);
    Interval _12096 = _15400;
    Interval _12097 = _15525;
    Interval _15580 = jet_mul_derivative(_12096, _12097, intervalFailed, optical_product_upper);
    Interval _12098 = _15580;
    Interval _12099 = _15399;
    Interval _12100 = _15552;
    Interval _15581 = jet_mul_derivative(_12099, _12100, intervalFailed, optical_product_upper);
    Interval _12101 = _15581;
    Interval _15582 = jet_add_derivative(_12098, _12101, intervalFailed);
    Interval _12102 = _15401;
    Interval _12103 = _15525;
    Interval _15583 = jet_mul_derivative(_12102, _12103, intervalFailed, optical_product_upper);
    Interval _12104 = _15583;
    Interval _12105 = _15399;
    Interval _12106 = _15564;
    Interval _15584 = jet_mul_derivative(_12105, _12106, intervalFailed, optical_product_upper);
    Interval _12107 = _15584;
    Interval _15585 = jet_add_derivative(_12104, _12107, intervalFailed);
    OpticalJet3 param_var_n = OpticalJet3{ OpticalJet{ _15565, _15568, _15571 }, OpticalJet{ _15572, _15575, _15578 }, OpticalJet{ _15579, _15582, _15585 } };
    OpticalJet3 param_var_direction = direction;
    OpticalJet3 _15591 = jet_oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper, jetBranchKnown);
    return _15591;
}

static inline __attribute__((always_inline))
bool optical_jet_forward(thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread OpticalJetRay& ray, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    ray.outgoing = OpticalJet3{ OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
    ray.origin = OpticalJet3{ OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
    ray.bias0 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    ray.depth = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    bool4 _6282 = isnan(box);
    bool4 _6283 = isinf(box);
    bool _6293;
    if (all(not(bool4(_6282.x || _6283.x, _6282.y || _6283.y, _6282.z || _6283.z, _6282.w || _6283.w))))
    {
        _6293 = any(box.xy > box.zw);
    }
    else
    {
        _6293 = true;
    }
    bool _6299;
    if (!_6293)
    {
        _6299 = any(box.xy < float2(0.0));
    }
    else
    {
        _6299 = true;
    }
    bool _6307;
    if (!_6299)
    {
        _6307 = box.z >= receiver.extentClip.x;
    }
    else
    {
        _6307 = true;
    }
    bool _6315;
    if (!_6307)
    {
        _6315 = box.w >= receiver.extentClip.y;
    }
    else
    {
        _6315 = true;
    }
    bool _6320;
    if (!_6315)
    {
        _6320 = control.x > 4u;
    }
    else
    {
        _6320 = true;
    }
    bool _6329;
    if (!_6320)
    {
        _6329 = (control.y >> (control.x & 31u)) != 0u;
    }
    else
    {
        _6329 = true;
    }
    if (_6329)
    {
        return false;
    }
    Interval _6271 = Interval{ box.x, box.z };
    Interval _6272 = Interval{ as_type<float>(as_type<uint>(receiver.projection.z) ^ 2147483648u), as_type<float>(as_type<uint>(receiver.projection.z) ^ 2147483648u) };
    Interval _6353 = iadd(_6271, _6272, intervalFailed);
    Interval _6273 = Interval{ 1.0, 1.0 };
    Interval _6274 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6355 = jet_add_derivative(_6273, _6274, intervalFailed);
    Interval _6275 = Interval{ 0.0, 0.0 };
    Interval _6276 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6357 = jet_add_derivative(_6275, _6276, intervalFailed);
    Interval _6257 = _6353;
    Interval _6258 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6363 = idiv(_6257, _6258, intervalFailed, interval_divide_upper);
    bool _6370;
    if (!intervalFailed)
    {
        _6370 = intervalFailed;
    }
    else
    {
        _6370 = false;
    }
    bool _6375;
    if (_6370)
    {
        _6375 = jetFailureSite == 0u;
    }
    else
    {
        _6375 = false;
    }
    if (_6375)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(_6353.lo, _6353.hi, receiver.projection.x, receiver.projection.x);
    }
    Interval _6259 = _6355;
    Interval _6260 = _6363;
    Interval _6261 = Interval{ 0.0, 0.0 };
    Interval _6379 = jet_mul_derivative(_6260, _6261, intervalFailed, optical_product_upper);
    Interval _6262 = Interval{ as_type<float>(as_type<uint>(_6379.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6379.lo) ^ 2147483648u) };
    Interval _6389 = jet_add_derivative(_6259, _6262, intervalFailed);
    Interval _6263 = _6389;
    Interval _6264 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6391 = jet_div_derivative(_6263, _6264, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6265 = _6357;
    Interval _6266 = _6363;
    Interval _6267 = Interval{ 0.0, 0.0 };
    Interval _6392 = jet_mul_derivative(_6266, _6267, intervalFailed, optical_product_upper);
    Interval _6268 = Interval{ as_type<float>(as_type<uint>(_6392.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6392.lo) ^ 2147483648u) };
    Interval _6402 = jet_add_derivative(_6265, _6268, intervalFailed);
    Interval _6269 = _6402;
    Interval _6270 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6404 = jet_div_derivative(_6269, _6270, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6251 = Interval{ box.y, box.w };
    Interval _6252 = Interval{ as_type<float>(as_type<uint>(receiver.projection.w) ^ 2147483648u), as_type<float>(as_type<uint>(receiver.projection.w) ^ 2147483648u) };
    Interval _6420 = iadd(_6251, _6252, intervalFailed);
    Interval _6253 = Interval{ 0.0, 0.0 };
    Interval _6254 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6422 = jet_add_derivative(_6253, _6254, intervalFailed);
    Interval _6255 = Interval{ 1.0, 1.0 };
    Interval _6256 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6424 = jet_add_derivative(_6255, _6256, intervalFailed);
    Interval _6237 = _6420;
    Interval _6238 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6430 = idiv(_6237, _6238, intervalFailed, interval_divide_upper);
    bool _6437;
    if (!intervalFailed)
    {
        _6437 = intervalFailed;
    }
    else
    {
        _6437 = false;
    }
    bool _6442;
    if (_6437)
    {
        _6442 = jetFailureSite == 0u;
    }
    else
    {
        _6442 = false;
    }
    if (_6442)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(_6420.lo, _6420.hi, receiver.projection.y, receiver.projection.y);
    }
    Interval _6239 = _6422;
    Interval _6240 = _6430;
    Interval _6241 = Interval{ 0.0, 0.0 };
    Interval _6446 = jet_mul_derivative(_6240, _6241, intervalFailed, optical_product_upper);
    Interval _6242 = Interval{ as_type<float>(as_type<uint>(_6446.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6446.lo) ^ 2147483648u) };
    Interval _6456 = jet_add_derivative(_6239, _6242, intervalFailed);
    Interval _6243 = _6456;
    Interval _6244 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6458 = jet_div_derivative(_6243, _6244, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6245 = _6424;
    Interval _6246 = _6430;
    Interval _6247 = Interval{ 0.0, 0.0 };
    Interval _6459 = jet_mul_derivative(_6246, _6247, intervalFailed, optical_product_upper);
    Interval _6248 = Interval{ as_type<float>(as_type<uint>(_6459.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6459.lo) ^ 2147483648u) };
    Interval _6469 = jet_add_derivative(_6245, _6248, intervalFailed);
    Interval _6249 = _6469;
    Interval _6250 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6471 = jet_div_derivative(_6249, _6250, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    bool _6478;
    if (_6363.lo <= 0.0)
    {
        _6478 = _6363.hi >= 0.0;
    }
    else
    {
        _6478 = false;
    }
    float _6485;
    if (_6478)
    {
        _6485 = 0.0;
    }
    else
    {
        _6485 = precise::min(abs(_6363.lo), abs(_6363.hi));
    }
    float _6488 = precise::max(abs(_6363.lo), abs(_6363.hi));
    float _6227 = spvFMul(_6485, _6485);
    float _6489 = interval_down(_6227, intervalFailed);
    float _6228 = spvFMul(_6488, _6488);
    float _6491 = interval_up(_6228, intervalFailed);
    Interval _6229 = Interval{ 2.0, 2.0 };
    Interval _6230 = _6363;
    Interval _6492 = imul(_6229, _6230, intervalFailed, optical_product_upper);
    Interval _6231 = _6492;
    Interval _6232 = _6391;
    Interval _6493 = jet_mul_derivative(_6231, _6232, intervalFailed, optical_product_upper);
    Interval _6233 = Interval{ 2.0, 2.0 };
    Interval _6234 = _6363;
    Interval _6494 = imul(_6233, _6234, intervalFailed, optical_product_upper);
    Interval _6235 = _6494;
    Interval _6236 = _6404;
    Interval _6495 = jet_mul_derivative(_6235, _6236, intervalFailed, optical_product_upper);
    bool _6502;
    if (_6430.lo <= 0.0)
    {
        _6502 = _6430.hi >= 0.0;
    }
    else
    {
        _6502 = false;
    }
    float _6509;
    if (_6502)
    {
        _6509 = 0.0;
    }
    else
    {
        _6509 = precise::min(abs(_6430.lo), abs(_6430.hi));
    }
    float _6512 = precise::max(abs(_6430.lo), abs(_6430.hi));
    float _6217 = spvFMul(_6509, _6509);
    float _6513 = interval_down(_6217, intervalFailed);
    float _6218 = spvFMul(_6512, _6512);
    float _6515 = interval_up(_6218, intervalFailed);
    Interval _6219 = Interval{ 2.0, 2.0 };
    Interval _6220 = _6430;
    Interval _6516 = imul(_6219, _6220, intervalFailed, optical_product_upper);
    Interval _6221 = _6516;
    Interval _6222 = _6458;
    Interval _6517 = jet_mul_derivative(_6221, _6222, intervalFailed, optical_product_upper);
    Interval _6223 = Interval{ 2.0, 2.0 };
    Interval _6224 = _6430;
    Interval _6518 = imul(_6223, _6224, intervalFailed, optical_product_upper);
    Interval _6225 = _6518;
    Interval _6226 = _6471;
    Interval _6519 = jet_mul_derivative(_6225, _6226, intervalFailed, optical_product_upper);
    Interval _6211 = Interval{ precise::max(0.0, _6489), _6491 };
    Interval _6212 = Interval{ precise::max(0.0, _6513), _6515 };
    Interval _6522 = iadd(_6211, _6212, intervalFailed);
    Interval _6213 = _6493;
    Interval _6214 = _6517;
    Interval _6523 = jet_add_derivative(_6213, _6214, intervalFailed);
    Interval _6215 = _6495;
    Interval _6216 = _6519;
    Interval _6524 = jet_add_derivative(_6215, _6216, intervalFailed);
    bool _6529;
    if (1.0 <= 0.0)
    {
        _6529 = 1.0 >= 0.0;
    }
    else
    {
        _6529 = false;
    }
    float _6536;
    if (_6529)
    {
        _6536 = 0.0;
    }
    else
    {
        _6536 = precise::min(abs(1.0), abs(1.0));
    }
    float _6539 = precise::max(abs(1.0), abs(1.0));
    float _6201 = spvFMul(_6536, _6536);
    float _6540 = interval_down(_6201, intervalFailed);
    float _6202 = spvFMul(_6539, _6539);
    float _6542 = interval_up(_6202, intervalFailed);
    Interval _6203 = Interval{ 2.0, 2.0 };
    Interval _6204 = Interval{ 1.0, 1.0 };
    Interval _6543 = imul(_6203, _6204, intervalFailed, optical_product_upper);
    Interval _6205 = _6543;
    Interval _6206 = Interval{ 0.0, 0.0 };
    Interval _6544 = jet_mul_derivative(_6205, _6206, intervalFailed, optical_product_upper);
    Interval _6207 = Interval{ 2.0, 2.0 };
    Interval _6208 = Interval{ 1.0, 1.0 };
    Interval _6545 = imul(_6207, _6208, intervalFailed, optical_product_upper);
    Interval _6209 = _6545;
    Interval _6210 = Interval{ 0.0, 0.0 };
    Interval _6546 = jet_mul_derivative(_6209, _6210, intervalFailed, optical_product_upper);
    Interval _6195 = _6522;
    Interval _6196 = Interval{ precise::max(0.0, _6540), _6542 };
    Interval _6548 = iadd(_6195, _6196, intervalFailed);
    Interval _6197 = _6523;
    Interval _6198 = _6544;
    Interval _6549 = jet_add_derivative(_6197, _6198, intervalFailed);
    Interval _6199 = _6524;
    Interval _6200 = _6546;
    Interval _6550 = jet_add_derivative(_6199, _6200, intervalFailed);
    Interval _6186 = _6548;
    Interval _6552 = isqrt(_6186, intervalFailed);
    bool _6560;
    if (!intervalFailed)
    {
        _6560 = intervalFailed;
    }
    else
    {
        _6560 = false;
    }
    bool _6565;
    if (_6560)
    {
        _6565 = jetFailureSite == 0u;
    }
    else
    {
        _6565 = false;
    }
    if (_6565)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_6548.lo, _6548.hi, 0.0, 0.0);
    }
    if (_6552.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _6187 = Interval{ 2.0, 2.0 };
    Interval _6188 = _6552;
    Interval _6572 = imul(_6187, _6188, intervalFailed, optical_product_upper);
    Interval _6189 = Interval{ 1.0, 1.0 };
    Interval _6190 = _6572;
    Interval _6574 = idiv(_6189, _6190, intervalFailed, interval_divide_upper);
    bool _6581;
    if (!intervalFailed)
    {
        _6581 = intervalFailed;
    }
    else
    {
        _6581 = false;
    }
    bool _6586;
    if (_6581)
    {
        _6586 = jetFailureSite == 0u;
    }
    else
    {
        _6586 = false;
    }
    if (_6586)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _6572.lo, _6572.hi);
    }
    Interval _6191 = _6574;
    Interval _6192 = _6549;
    Interval _6590 = jet_mul_derivative(_6191, _6192, intervalFailed, optical_product_upper);
    Interval _6193 = _6574;
    Interval _6194 = _6550;
    Interval _6591 = jet_mul_derivative(_6193, _6194, intervalFailed, optical_product_upper);
    Interval _6172 = Interval{ 1.0, 1.0 };
    Interval _6173 = _6552;
    Interval _6593 = idiv(_6172, _6173, intervalFailed, interval_divide_upper);
    bool _6600;
    if (!intervalFailed)
    {
        _6600 = intervalFailed;
    }
    else
    {
        _6600 = false;
    }
    bool _6605;
    if (_6600)
    {
        _6605 = jetFailureSite == 0u;
    }
    else
    {
        _6605 = false;
    }
    if (_6605)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _6552.lo, _6552.hi);
    }
    Interval _6174 = Interval{ 0.0, 0.0 };
    Interval _6175 = _6593;
    Interval _6176 = _6590;
    Interval _6609 = jet_mul_derivative(_6175, _6176, intervalFailed, optical_product_upper);
    Interval _6177 = Interval{ as_type<float>(as_type<uint>(_6609.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6609.lo) ^ 2147483648u) };
    Interval _6619 = jet_add_derivative(_6174, _6177, intervalFailed);
    Interval _6178 = _6619;
    Interval _6179 = _6552;
    Interval _6620 = jet_div_derivative(_6178, _6179, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6180 = Interval{ 0.0, 0.0 };
    Interval _6181 = _6593;
    Interval _6182 = _6591;
    Interval _6621 = jet_mul_derivative(_6181, _6182, intervalFailed, optical_product_upper);
    Interval _6183 = Interval{ as_type<float>(as_type<uint>(_6621.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6621.lo) ^ 2147483648u) };
    Interval _6631 = jet_add_derivative(_6180, _6183, intervalFailed);
    Interval _6184 = _6631;
    Interval _6185 = _6552;
    Interval _6632 = jet_div_derivative(_6184, _6185, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6158 = _6363;
    Interval _6159 = _6593;
    Interval _6633 = imul(_6158, _6159, intervalFailed, optical_product_upper);
    Interval _6160 = _6391;
    Interval _6161 = _6593;
    Interval _6634 = jet_mul_derivative(_6160, _6161, intervalFailed, optical_product_upper);
    Interval _6162 = _6634;
    Interval _6163 = _6363;
    Interval _6164 = _6620;
    Interval _6635 = jet_mul_derivative(_6163, _6164, intervalFailed, optical_product_upper);
    Interval _6165 = _6635;
    Interval _6636 = jet_add_derivative(_6162, _6165, intervalFailed);
    Interval _6166 = _6404;
    Interval _6167 = _6593;
    Interval _6637 = jet_mul_derivative(_6166, _6167, intervalFailed, optical_product_upper);
    Interval _6168 = _6637;
    Interval _6169 = _6363;
    Interval _6170 = _6632;
    Interval _6638 = jet_mul_derivative(_6169, _6170, intervalFailed, optical_product_upper);
    Interval _6171 = _6638;
    Interval _6639 = jet_add_derivative(_6168, _6171, intervalFailed);
    Interval _6144 = _6430;
    Interval _6145 = _6593;
    Interval _6640 = imul(_6144, _6145, intervalFailed, optical_product_upper);
    Interval _6146 = _6458;
    Interval _6147 = _6593;
    Interval _6641 = jet_mul_derivative(_6146, _6147, intervalFailed, optical_product_upper);
    Interval _6148 = _6641;
    Interval _6149 = _6430;
    Interval _6150 = _6620;
    Interval _6642 = jet_mul_derivative(_6149, _6150, intervalFailed, optical_product_upper);
    Interval _6151 = _6642;
    Interval _6643 = jet_add_derivative(_6148, _6151, intervalFailed);
    Interval _6152 = _6471;
    Interval _6153 = _6593;
    Interval _6644 = jet_mul_derivative(_6152, _6153, intervalFailed, optical_product_upper);
    Interval _6154 = _6644;
    Interval _6155 = _6430;
    Interval _6156 = _6632;
    Interval _6645 = jet_mul_derivative(_6155, _6156, intervalFailed, optical_product_upper);
    Interval _6157 = _6645;
    Interval _6646 = jet_add_derivative(_6154, _6157, intervalFailed);
    Interval _6130 = Interval{ 1.0, 1.0 };
    Interval _6131 = _6593;
    Interval _6647 = imul(_6130, _6131, intervalFailed, optical_product_upper);
    Interval _6132 = Interval{ 0.0, 0.0 };
    Interval _6133 = _6593;
    Interval _6648 = jet_mul_derivative(_6132, _6133, intervalFailed, optical_product_upper);
    Interval _6134 = _6648;
    Interval _6135 = Interval{ 1.0, 1.0 };
    Interval _6136 = _6620;
    Interval _6649 = jet_mul_derivative(_6135, _6136, intervalFailed, optical_product_upper);
    Interval _6137 = _6649;
    Interval _6650 = jet_add_derivative(_6134, _6137, intervalFailed);
    Interval _6138 = Interval{ 0.0, 0.0 };
    Interval _6139 = _6593;
    Interval _6651 = jet_mul_derivative(_6138, _6139, intervalFailed, optical_product_upper);
    Interval _6140 = _6651;
    Interval _6141 = Interval{ 1.0, 1.0 };
    Interval _6142 = _6632;
    Interval _6652 = jet_mul_derivative(_6141, _6142, intervalFailed, optical_product_upper);
    Interval _6143 = _6652;
    Interval _6653 = jet_add_derivative(_6140, _6143, intervalFailed);
    float _6672;
    float _6674;
    float _6676;
    float _6678;
    float _6680;
    float _6682;
    float _6684;
    float _6686;
    float _6688;
    float _6690;
    float _6692;
    float _6694;
    float _6696;
    float _6698;
    float _6700;
    float _6702;
    float _6704;
    float _6706;
    float _6708;
    float _6710;
    float _6712;
    float _6714;
    float _6716;
    float _6718;
    float _6720;
    float _6722;
    float _6724;
    float _6726;
    float _6728;
    float _6730;
    float _6732;
    float _6734;
    float _6736;
    float _6738;
    float _6740;
    float _6742;
    float _6744;
    float _6746;
    float _6748;
    float _6750;
    float _6752;
    float _6754;
    _6672 = 0.0;
    _6674 = 0.0;
    _6676 = 0.0;
    _6678 = 0.0;
    _6680 = 0.0;
    _6682 = 0.0;
    _6684 = _6633.lo;
    _6686 = _6633.hi;
    _6688 = _6636.lo;
    _6690 = _6636.hi;
    _6692 = _6639.lo;
    _6694 = _6639.hi;
    _6696 = _6640.lo;
    _6698 = _6640.hi;
    _6700 = _6643.lo;
    _6702 = _6643.hi;
    _6704 = _6646.lo;
    _6706 = _6646.hi;
    _6708 = _6647.lo;
    _6710 = _6647.hi;
    _6712 = _6650.lo;
    _6714 = _6650.hi;
    _6716 = _6653.lo;
    _6718 = _6653.hi;
    _6720 = 0.0;
    _6722 = 0.0;
    _6724 = 0.0;
    _6726 = 0.0;
    _6728 = 0.0;
    _6730 = 0.0;
    _6732 = 0.0;
    _6734 = 0.0;
    _6736 = 0.0;
    _6738 = 0.0;
    _6740 = 0.0;
    _6742 = 0.0;
    _6744 = 0.0;
    _6746 = 0.0;
    _6748 = 0.0;
    _6750 = 0.0;
    _6752 = 0.0;
    _6754 = 0.0;
    float _6673;
    float _6675;
    float _6677;
    float _6679;
    float _6681;
    float _6683;
    float _6721;
    float _6723;
    float _6725;
    float _6727;
    float _6729;
    float _6731;
    float _6733;
    float _6735;
    float _6737;
    float _6739;
    float _6741;
    float _6743;
    float _6745;
    float _6747;
    float _6749;
    float _6751;
    float _6753;
    float _6755;
    OpticalJet3 hit;
    float _6685;
    float _6687;
    float _6689;
    float _6691;
    float _6693;
    float _6695;
    float _6697;
    float _6699;
    float _6701;
    float _6703;
    float _6705;
    float _6707;
    float _6709;
    float _6711;
    float _6713;
    float _6715;
    float _6717;
    float _6719;
    for (uint _6756 = 0u; _6756 <= control.x; _6672 = _6673, _6674 = _6675, _6676 = _6677, _6678 = _6679, _6680 = _6681, _6682 = _6683, _6684 = _6685, _6686 = _6687, _6688 = _6689, _6690 = _6691, _6692 = _6693, _6694 = _6695, _6696 = _6697, _6698 = _6699, _6700 = _6701, _6702 = _6703, _6704 = _6705, _6706 = _6707, _6708 = _6709, _6710 = _6711, _6712 = _6713, _6714 = _6715, _6716 = _6717, _6718 = _6719, _6720 = _6721, _6722 = _6723, _6724 = _6725, _6726 = _6727, _6728 = _6729, _6730 = _6731, _6732 = _6733, _6734 = _6735, _6736 = _6737, _6738 = _6739, _6740 = _6741, _6742 = _6743, _6744 = _6745, _6746 = _6747, _6748 = _6749, _6750 = _6751, _6752 = _6753, _6754 = _6755, _6756++)
    {
        bool _6760 = _6756 == 0u;
        bool _6770;
        if (_6760)
        {
            _6770 = control.z != 0u;
        }
        else
        {
            _6770 = (control.y & (1u << ((_6756 - 1u) & 31u))) != 0u;
        }
        float4 _6782;
        float4 _6783;
        float4 _6784;
        if (_6760)
        {
            _6782 = receiver.a;
            _6783 = receiver.b;
            _6784 = receiver.c;
        }
        else
        {
            uint _1682 = _6756 - 1u;
            _6782 = planes[_1682].a;
            _6783 = planes[_1682].b;
            _6784 = planes[_1682].c;
        }
        float _6836;
        float _6837;
        float _6838;
        float _6839;
        float _6840;
        float _6841;
        float _6842;
        float _6843;
        float _6844;
        float _6845;
        float _6846;
        float _6847;
        float _6848;
        float _6849;
        float _6850;
        float _6851;
        float _6852;
        float _6853;
        if (_6770)
        {
            _6836 = liquid.planeNormal.x;
            _6837 = liquid.planeNormal.x;
            _6838 = 0.0;
            _6839 = 0.0;
            _6840 = 0.0;
            _6841 = 0.0;
            _6842 = liquid.planeNormal.y;
            _6843 = liquid.planeNormal.y;
            _6844 = 0.0;
            _6845 = 0.0;
            _6846 = 0.0;
            _6847 = 0.0;
            _6848 = liquid.planeNormal.z;
            _6849 = liquid.planeNormal.z;
            _6850 = 0.0;
            _6851 = 0.0;
            _6852 = 0.0;
            _6853 = 0.0;
        }
        else
        {
            ReflectionSpecularPlane param_var_plane = ReflectionSpecularPlane{ _6782, _6783, _6784 };
            OpticalJet3 _6786 = jet_plane_normal(param_var_plane, intervalFailed, optical_product_upper, interval_divide_upper);
            OpticalJet3 param_var_n = _6786;
            OpticalJet3 param_var_direction = OpticalJet3{ OpticalJet{ Interval{ _6684, _6686 }, Interval{ _6688, _6690 }, Interval{ _6692, _6694 } }, OpticalJet{ Interval{ _6696, _6698 }, Interval{ _6700, _6702 }, Interval{ _6704, _6706 } }, OpticalJet{ Interval{ _6708, _6710 }, Interval{ _6712, _6714 }, Interval{ _6716, _6718 } } };
            OpticalJet3 _6800 = jet_oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper, jetBranchKnown);
            _6836 = _6800.x.v.lo;
            _6837 = _6800.x.v.hi;
            _6838 = _6800.x.dx.lo;
            _6839 = _6800.x.dx.hi;
            _6840 = _6800.x.dy.lo;
            _6841 = _6800.x.dy.hi;
            _6842 = _6800.y.v.lo;
            _6843 = _6800.y.v.hi;
            _6844 = _6800.y.dx.lo;
            _6845 = _6800.y.dx.hi;
            _6846 = _6800.y.dy.lo;
            _6847 = _6800.y.dy.hi;
            _6848 = _6800.z.v.lo;
            _6849 = _6800.z.v.hi;
            _6850 = _6800.z.dx.lo;
            _6851 = _6800.z.dx.hi;
            _6852 = _6800.z.dy.lo;
            _6853 = _6800.z.dy.hi;
        }
        Interval _6116 = Interval{ _6684, _6686 };
        Interval _6117 = Interval{ _6836, _6837 };
        Interval _6856 = imul(_6116, _6117, intervalFailed, optical_product_upper);
        Interval _6118 = Interval{ _6688, _6690 };
        Interval _6119 = Interval{ _6836, _6837 };
        Interval _6859 = jet_mul_derivative(_6118, _6119, intervalFailed, optical_product_upper);
        Interval _6120 = _6859;
        Interval _6121 = Interval{ _6684, _6686 };
        Interval _6122 = Interval{ _6838, _6839 };
        Interval _6862 = jet_mul_derivative(_6121, _6122, intervalFailed, optical_product_upper);
        Interval _6123 = _6862;
        Interval _6863 = jet_add_derivative(_6120, _6123, intervalFailed);
        Interval _6124 = Interval{ _6692, _6694 };
        Interval _6125 = Interval{ _6836, _6837 };
        Interval _6866 = jet_mul_derivative(_6124, _6125, intervalFailed, optical_product_upper);
        Interval _6126 = _6866;
        Interval _6127 = Interval{ _6684, _6686 };
        Interval _6128 = Interval{ _6840, _6841 };
        Interval _6869 = jet_mul_derivative(_6127, _6128, intervalFailed, optical_product_upper);
        Interval _6129 = _6869;
        Interval _6870 = jet_add_derivative(_6126, _6129, intervalFailed);
        Interval _6102 = Interval{ _6696, _6698 };
        Interval _6103 = Interval{ _6842, _6843 };
        Interval _6873 = imul(_6102, _6103, intervalFailed, optical_product_upper);
        Interval _6104 = Interval{ _6700, _6702 };
        Interval _6105 = Interval{ _6842, _6843 };
        Interval _6876 = jet_mul_derivative(_6104, _6105, intervalFailed, optical_product_upper);
        Interval _6106 = _6876;
        Interval _6107 = Interval{ _6696, _6698 };
        Interval _6108 = Interval{ _6844, _6845 };
        Interval _6879 = jet_mul_derivative(_6107, _6108, intervalFailed, optical_product_upper);
        Interval _6109 = _6879;
        Interval _6880 = jet_add_derivative(_6106, _6109, intervalFailed);
        Interval _6110 = Interval{ _6704, _6706 };
        Interval _6111 = Interval{ _6842, _6843 };
        Interval _6883 = jet_mul_derivative(_6110, _6111, intervalFailed, optical_product_upper);
        Interval _6112 = _6883;
        Interval _6113 = Interval{ _6696, _6698 };
        Interval _6114 = Interval{ _6846, _6847 };
        Interval _6886 = jet_mul_derivative(_6113, _6114, intervalFailed, optical_product_upper);
        Interval _6115 = _6886;
        Interval _6887 = jet_add_derivative(_6112, _6115, intervalFailed);
        Interval _6096 = _6856;
        Interval _6097 = _6873;
        Interval _6888 = iadd(_6096, _6097, intervalFailed);
        Interval _6098 = _6863;
        Interval _6099 = _6880;
        Interval _6889 = jet_add_derivative(_6098, _6099, intervalFailed);
        Interval _6100 = _6870;
        Interval _6101 = _6887;
        Interval _6890 = jet_add_derivative(_6100, _6101, intervalFailed);
        Interval _6082 = Interval{ _6708, _6710 };
        Interval _6083 = Interval{ _6848, _6849 };
        Interval _6893 = imul(_6082, _6083, intervalFailed, optical_product_upper);
        Interval _6084 = Interval{ _6712, _6714 };
        Interval _6085 = Interval{ _6848, _6849 };
        Interval _6896 = jet_mul_derivative(_6084, _6085, intervalFailed, optical_product_upper);
        Interval _6086 = _6896;
        Interval _6087 = Interval{ _6708, _6710 };
        Interval _6088 = Interval{ _6850, _6851 };
        Interval _6899 = jet_mul_derivative(_6087, _6088, intervalFailed, optical_product_upper);
        Interval _6089 = _6899;
        Interval _6900 = jet_add_derivative(_6086, _6089, intervalFailed);
        Interval _6090 = Interval{ _6716, _6718 };
        Interval _6091 = Interval{ _6848, _6849 };
        Interval _6903 = jet_mul_derivative(_6090, _6091, intervalFailed, optical_product_upper);
        Interval _6092 = _6903;
        Interval _6093 = Interval{ _6708, _6710 };
        Interval _6094 = Interval{ _6852, _6853 };
        Interval _6906 = jet_mul_derivative(_6093, _6094, intervalFailed, optical_product_upper);
        Interval _6095 = _6906;
        Interval _6907 = jet_add_derivative(_6092, _6095, intervalFailed);
        Interval _6076 = _6888;
        Interval _6077 = _6893;
        Interval _6908 = iadd(_6076, _6077, intervalFailed);
        Interval _6078 = _6889;
        Interval _6079 = _6900;
        Interval _6909 = jet_add_derivative(_6078, _6079, intervalFailed);
        Interval _6080 = _6890;
        Interval _6081 = _6907;
        Interval _6910 = jet_add_derivative(_6080, _6081, intervalFailed);
        bool _6917;
        if (_6908.lo <= 0.0)
        {
            _6917 = _6908.hi >= 0.0;
        }
        else
        {
            _6917 = false;
        }
        float _6924;
        if (_6917)
        {
            _6924 = 0.0;
        }
        else
        {
            _6924 = precise::min(abs(_6908.lo), abs(_6908.hi));
        }
        bool _6926;
        if (!_6760)
        {
            _6926 = _6770;
        }
        else
        {
            _6926 = false;
        }
        float _6927;
        if (_6926)
        {
            _6927 = 9.9999999392252902907785028219223e-09;
        }
        else
        {
            _6927 = 9.9999999600419720025001879548654e-13;
        }
        float _6074 = _6927;
        float _6928 = interval_down(_6074, intervalFailed);
        float _6075 = _6927;
        float _6929 = interval_up(_6075, intervalFailed);
        if (_6924 <= _6929)
        {
            return false;
        }
        float _7524;
        float _7525;
        float _7526;
        float _7527;
        float _7528;
        float _7529;
        if (_6760)
        {
            float3 _7170;
            if (_6770)
            {
                _7170 = liquid.planePoint.xyz;
            }
            else
            {
                _7170 = _6782.xyz;
            }
            Interval _6060 = Interval{ _7170.x, _7170.x };
            Interval _6061 = Interval{ _6836, _6837 };
            Interval _7176 = imul(_6060, _6061, intervalFailed, optical_product_upper);
            Interval _6062 = Interval{ 0.0, 0.0 };
            Interval _6063 = Interval{ _6836, _6837 };
            Interval _7178 = jet_mul_derivative(_6062, _6063, intervalFailed, optical_product_upper);
            Interval _6064 = _7178;
            Interval _6065 = Interval{ _7170.x, _7170.x };
            Interval _6066 = Interval{ _6838, _6839 };
            Interval _7181 = jet_mul_derivative(_6065, _6066, intervalFailed, optical_product_upper);
            Interval _6067 = _7181;
            Interval _7182 = jet_add_derivative(_6064, _6067, intervalFailed);
            Interval _6068 = Interval{ 0.0, 0.0 };
            Interval _6069 = Interval{ _6836, _6837 };
            Interval _7184 = jet_mul_derivative(_6068, _6069, intervalFailed, optical_product_upper);
            Interval _6070 = _7184;
            Interval _6071 = Interval{ _7170.x, _7170.x };
            Interval _6072 = Interval{ _6840, _6841 };
            Interval _7187 = jet_mul_derivative(_6071, _6072, intervalFailed, optical_product_upper);
            Interval _6073 = _7187;
            Interval _7188 = jet_add_derivative(_6070, _6073, intervalFailed);
            Interval _6046 = Interval{ _7170.y, _7170.y };
            Interval _6047 = Interval{ _6842, _6843 };
            Interval _7191 = imul(_6046, _6047, intervalFailed, optical_product_upper);
            Interval _6048 = Interval{ 0.0, 0.0 };
            Interval _6049 = Interval{ _6842, _6843 };
            Interval _7193 = jet_mul_derivative(_6048, _6049, intervalFailed, optical_product_upper);
            Interval _6050 = _7193;
            Interval _6051 = Interval{ _7170.y, _7170.y };
            Interval _6052 = Interval{ _6844, _6845 };
            Interval _7196 = jet_mul_derivative(_6051, _6052, intervalFailed, optical_product_upper);
            Interval _6053 = _7196;
            Interval _7197 = jet_add_derivative(_6050, _6053, intervalFailed);
            Interval _6054 = Interval{ 0.0, 0.0 };
            Interval _6055 = Interval{ _6842, _6843 };
            Interval _7199 = jet_mul_derivative(_6054, _6055, intervalFailed, optical_product_upper);
            Interval _6056 = _7199;
            Interval _6057 = Interval{ _7170.y, _7170.y };
            Interval _6058 = Interval{ _6846, _6847 };
            Interval _7202 = jet_mul_derivative(_6057, _6058, intervalFailed, optical_product_upper);
            Interval _6059 = _7202;
            Interval _7203 = jet_add_derivative(_6056, _6059, intervalFailed);
            Interval _6040 = _7176;
            Interval _6041 = _7191;
            Interval _7204 = iadd(_6040, _6041, intervalFailed);
            Interval _6042 = _7182;
            Interval _6043 = _7197;
            Interval _7205 = jet_add_derivative(_6042, _6043, intervalFailed);
            Interval _6044 = _7188;
            Interval _6045 = _7203;
            Interval _7206 = jet_add_derivative(_6044, _6045, intervalFailed);
            Interval _6026 = Interval{ _7170.z, _7170.z };
            Interval _6027 = Interval{ _6848, _6849 };
            Interval _7209 = imul(_6026, _6027, intervalFailed, optical_product_upper);
            Interval _6028 = Interval{ 0.0, 0.0 };
            Interval _6029 = Interval{ _6848, _6849 };
            Interval _7211 = jet_mul_derivative(_6028, _6029, intervalFailed, optical_product_upper);
            Interval _6030 = _7211;
            Interval _6031 = Interval{ _7170.z, _7170.z };
            Interval _6032 = Interval{ _6850, _6851 };
            Interval _7214 = jet_mul_derivative(_6031, _6032, intervalFailed, optical_product_upper);
            Interval _6033 = _7214;
            Interval _7215 = jet_add_derivative(_6030, _6033, intervalFailed);
            Interval _6034 = Interval{ 0.0, 0.0 };
            Interval _6035 = Interval{ _6848, _6849 };
            Interval _7217 = jet_mul_derivative(_6034, _6035, intervalFailed, optical_product_upper);
            Interval _6036 = _7217;
            Interval _6037 = Interval{ _7170.z, _7170.z };
            Interval _6038 = Interval{ _6852, _6853 };
            Interval _7220 = jet_mul_derivative(_6037, _6038, intervalFailed, optical_product_upper);
            Interval _6039 = _7220;
            Interval _7221 = jet_add_derivative(_6036, _6039, intervalFailed);
            Interval _6020 = _7204;
            Interval _6021 = _7209;
            Interval _7222 = iadd(_6020, _6021, intervalFailed);
            Interval _6022 = _7205;
            Interval _6023 = _7215;
            Interval _7223 = jet_add_derivative(_6022, _6023, intervalFailed);
            Interval _6024 = _7206;
            Interval _6025 = _7221;
            Interval _7224 = jet_add_derivative(_6024, _6025, intervalFailed);
            Interval _6006 = _6363;
            Interval _6007 = Interval{ _6836, _6837 };
            Interval _7226 = imul(_6006, _6007, intervalFailed, optical_product_upper);
            Interval _6008 = _6391;
            Interval _6009 = Interval{ _6836, _6837 };
            Interval _7228 = jet_mul_derivative(_6008, _6009, intervalFailed, optical_product_upper);
            Interval _6010 = _7228;
            Interval _6011 = _6363;
            Interval _6012 = Interval{ _6838, _6839 };
            Interval _7230 = jet_mul_derivative(_6011, _6012, intervalFailed, optical_product_upper);
            Interval _6013 = _7230;
            Interval _7231 = jet_add_derivative(_6010, _6013, intervalFailed);
            Interval _6014 = _6404;
            Interval _6015 = Interval{ _6836, _6837 };
            Interval _7233 = jet_mul_derivative(_6014, _6015, intervalFailed, optical_product_upper);
            Interval _6016 = _7233;
            Interval _6017 = _6363;
            Interval _6018 = Interval{ _6840, _6841 };
            Interval _7235 = jet_mul_derivative(_6017, _6018, intervalFailed, optical_product_upper);
            Interval _6019 = _7235;
            Interval _7236 = jet_add_derivative(_6016, _6019, intervalFailed);
            Interval _5992 = _6430;
            Interval _5993 = Interval{ _6842, _6843 };
            Interval _7238 = imul(_5992, _5993, intervalFailed, optical_product_upper);
            Interval _5994 = _6458;
            Interval _5995 = Interval{ _6842, _6843 };
            Interval _7240 = jet_mul_derivative(_5994, _5995, intervalFailed, optical_product_upper);
            Interval _5996 = _7240;
            Interval _5997 = _6430;
            Interval _5998 = Interval{ _6844, _6845 };
            Interval _7242 = jet_mul_derivative(_5997, _5998, intervalFailed, optical_product_upper);
            Interval _5999 = _7242;
            Interval _7243 = jet_add_derivative(_5996, _5999, intervalFailed);
            Interval _6000 = _6471;
            Interval _6001 = Interval{ _6842, _6843 };
            Interval _7245 = jet_mul_derivative(_6000, _6001, intervalFailed, optical_product_upper);
            Interval _6002 = _7245;
            Interval _6003 = _6430;
            Interval _6004 = Interval{ _6846, _6847 };
            Interval _7247 = jet_mul_derivative(_6003, _6004, intervalFailed, optical_product_upper);
            Interval _6005 = _7247;
            Interval _7248 = jet_add_derivative(_6002, _6005, intervalFailed);
            Interval _5986 = _7226;
            Interval _5987 = _7238;
            Interval _7249 = iadd(_5986, _5987, intervalFailed);
            Interval _5988 = _7231;
            Interval _5989 = _7243;
            Interval _7250 = jet_add_derivative(_5988, _5989, intervalFailed);
            Interval _5990 = _7236;
            Interval _5991 = _7248;
            Interval _7251 = jet_add_derivative(_5990, _5991, intervalFailed);
            Interval _5972 = Interval{ 1.0, 1.0 };
            Interval _5973 = Interval{ _6848, _6849 };
            Interval _7253 = imul(_5972, _5973, intervalFailed, optical_product_upper);
            Interval _5974 = Interval{ 0.0, 0.0 };
            Interval _5975 = Interval{ _6848, _6849 };
            Interval _7255 = jet_mul_derivative(_5974, _5975, intervalFailed, optical_product_upper);
            Interval _5976 = _7255;
            Interval _5977 = Interval{ 1.0, 1.0 };
            Interval _5978 = Interval{ _6850, _6851 };
            Interval _7257 = jet_mul_derivative(_5977, _5978, intervalFailed, optical_product_upper);
            Interval _5979 = _7257;
            Interval _7258 = jet_add_derivative(_5976, _5979, intervalFailed);
            Interval _5980 = Interval{ 0.0, 0.0 };
            Interval _5981 = Interval{ _6848, _6849 };
            Interval _7260 = jet_mul_derivative(_5980, _5981, intervalFailed, optical_product_upper);
            Interval _5982 = _7260;
            Interval _5983 = Interval{ 1.0, 1.0 };
            Interval _5984 = Interval{ _6852, _6853 };
            Interval _7262 = jet_mul_derivative(_5983, _5984, intervalFailed, optical_product_upper);
            Interval _5985 = _7262;
            Interval _7263 = jet_add_derivative(_5982, _5985, intervalFailed);
            Interval _5966 = _7249;
            Interval _5967 = _7253;
            Interval _7264 = iadd(_5966, _5967, intervalFailed);
            Interval _5968 = _7250;
            Interval _5969 = _7258;
            Interval _7265 = jet_add_derivative(_5968, _5969, intervalFailed);
            Interval _5970 = _7251;
            Interval _5971 = _7263;
            Interval _7266 = jet_add_derivative(_5970, _5971, intervalFailed);
            Interval _5952 = _7222;
            Interval _5953 = _7264;
            Interval _7268 = idiv(_5952, _5953, intervalFailed, interval_divide_upper);
            bool _7277;
            if (!intervalFailed)
            {
                _7277 = intervalFailed;
            }
            else
            {
                _7277 = false;
            }
            bool _7282;
            if (_7277)
            {
                _7282 = jetFailureSite == 0u;
            }
            else
            {
                _7282 = false;
            }
            if (_7282)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7222.lo, _7222.hi, _7264.lo, _7264.hi);
            }
            Interval _5954 = _7223;
            Interval _5955 = _7268;
            Interval _5956 = _7265;
            Interval _7286 = jet_mul_derivative(_5955, _5956, intervalFailed, optical_product_upper);
            Interval _5957 = Interval{ as_type<float>(as_type<uint>(_7286.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7286.lo) ^ 2147483648u) };
            Interval _7296 = jet_add_derivative(_5954, _5957, intervalFailed);
            Interval _5958 = _7296;
            Interval _5959 = _7264;
            Interval _7297 = jet_div_derivative(_5958, _5959, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5960 = _7224;
            Interval _5961 = _7268;
            Interval _5962 = _7266;
            Interval _7298 = jet_mul_derivative(_5961, _5962, intervalFailed, optical_product_upper);
            Interval _5963 = Interval{ as_type<float>(as_type<uint>(_7298.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7298.lo) ^ 2147483648u) };
            Interval _7308 = jet_add_derivative(_5960, _5963, intervalFailed);
            Interval _5964 = _7308;
            Interval _5965 = _7264;
            Interval _7309 = jet_div_derivative(_5964, _5965, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            ray.depth = OpticalJet{ _7268, _7297, _7309 };
            Interval _5938 = ray.depth.v;
            Interval _5939 = _6552;
            Interval _7317 = imul(_5938, _5939, intervalFailed, optical_product_upper);
            Interval _5940 = ray.depth.dx;
            Interval _5941 = _6552;
            Interval _7318 = jet_mul_derivative(_5940, _5941, intervalFailed, optical_product_upper);
            Interval _5942 = _7318;
            Interval _5943 = ray.depth.v;
            Interval _5944 = _6590;
            Interval _7319 = jet_mul_derivative(_5943, _5944, intervalFailed, optical_product_upper);
            Interval _5945 = _7319;
            Interval _7320 = jet_add_derivative(_5942, _5945, intervalFailed);
            Interval _5946 = ray.depth.dy;
            Interval _5947 = _6552;
            Interval _7321 = jet_mul_derivative(_5946, _5947, intervalFailed, optical_product_upper);
            Interval _5948 = _7321;
            Interval _5949 = ray.depth.v;
            Interval _5950 = _6591;
            Interval _7322 = jet_mul_derivative(_5949, _5950, intervalFailed, optical_product_upper);
            Interval _5951 = _7322;
            Interval _7323 = jet_add_derivative(_5948, _5951, intervalFailed);
            Interval _5924 = _6363;
            Interval _5925 = ray.depth.v;
            Interval _7335 = imul(_5924, _5925, intervalFailed, optical_product_upper);
            Interval _5926 = _6391;
            Interval _5927 = ray.depth.v;
            Interval _7336 = jet_mul_derivative(_5926, _5927, intervalFailed, optical_product_upper);
            Interval _5928 = _7336;
            Interval _5929 = _6363;
            Interval _5930 = ray.depth.dx;
            Interval _7337 = jet_mul_derivative(_5929, _5930, intervalFailed, optical_product_upper);
            Interval _5931 = _7337;
            Interval _7338 = jet_add_derivative(_5928, _5931, intervalFailed);
            Interval _5932 = _6404;
            Interval _5933 = ray.depth.v;
            Interval _7339 = jet_mul_derivative(_5932, _5933, intervalFailed, optical_product_upper);
            Interval _5934 = _7339;
            Interval _5935 = _6363;
            Interval _5936 = ray.depth.dy;
            Interval _7340 = jet_mul_derivative(_5935, _5936, intervalFailed, optical_product_upper);
            Interval _5937 = _7340;
            Interval _7341 = jet_add_derivative(_5934, _5937, intervalFailed);
            Interval _5910 = _6430;
            Interval _5911 = ray.depth.v;
            Interval _7345 = imul(_5910, _5911, intervalFailed, optical_product_upper);
            Interval _5912 = _6458;
            Interval _5913 = ray.depth.v;
            Interval _7346 = jet_mul_derivative(_5912, _5913, intervalFailed, optical_product_upper);
            Interval _5914 = _7346;
            Interval _5915 = _6430;
            Interval _5916 = ray.depth.dx;
            Interval _7347 = jet_mul_derivative(_5915, _5916, intervalFailed, optical_product_upper);
            Interval _5917 = _7347;
            Interval _7348 = jet_add_derivative(_5914, _5917, intervalFailed);
            Interval _5918 = _6471;
            Interval _5919 = ray.depth.v;
            Interval _7349 = jet_mul_derivative(_5918, _5919, intervalFailed, optical_product_upper);
            Interval _5920 = _7349;
            Interval _5921 = _6430;
            Interval _5922 = ray.depth.dy;
            Interval _7350 = jet_mul_derivative(_5921, _5922, intervalFailed, optical_product_upper);
            Interval _5923 = _7350;
            Interval _7351 = jet_add_derivative(_5920, _5923, intervalFailed);
            Interval _5896 = Interval{ 1.0, 1.0 };
            Interval _5897 = ray.depth.v;
            Interval _7355 = imul(_5896, _5897, intervalFailed, optical_product_upper);
            Interval _5898 = Interval{ 0.0, 0.0 };
            Interval _5899 = ray.depth.v;
            Interval _7356 = jet_mul_derivative(_5898, _5899, intervalFailed, optical_product_upper);
            Interval _5900 = _7356;
            Interval _5901 = Interval{ 1.0, 1.0 };
            Interval _5902 = ray.depth.dx;
            Interval _7357 = jet_mul_derivative(_5901, _5902, intervalFailed, optical_product_upper);
            Interval _5903 = _7357;
            Interval _7358 = jet_add_derivative(_5900, _5903, intervalFailed);
            Interval _5904 = Interval{ 0.0, 0.0 };
            Interval _5905 = ray.depth.v;
            Interval _7359 = jet_mul_derivative(_5904, _5905, intervalFailed, optical_product_upper);
            Interval _5906 = _7359;
            Interval _5907 = Interval{ 1.0, 1.0 };
            Interval _5908 = ray.depth.dy;
            Interval _7360 = jet_mul_derivative(_5907, _5908, intervalFailed, optical_product_upper);
            Interval _5909 = _7360;
            Interval _7361 = jet_add_derivative(_5906, _5909, intervalFailed);
            hit = OpticalJet3{ OpticalJet{ _7335, _7338, _7341 }, OpticalJet{ _7345, _7348, _7351 }, OpticalJet{ _7355, _7358, _7361 } };
            float3 _7370;
            if (_6770)
            {
                _7370 = liquid.planePoint.xyz;
            }
            else
            {
                _7370 = _6782.xyz;
            }
            Interval _5882 = Interval{ _7370.x, _7370.x };
            Interval _5883 = Interval{ _6836, _6837 };
            Interval _7376 = imul(_5882, _5883, intervalFailed, optical_product_upper);
            Interval _5884 = Interval{ 0.0, 0.0 };
            Interval _5885 = Interval{ _6836, _6837 };
            Interval _7378 = jet_mul_derivative(_5884, _5885, intervalFailed, optical_product_upper);
            Interval _5886 = _7378;
            Interval _5887 = Interval{ _7370.x, _7370.x };
            Interval _5888 = Interval{ _6838, _6839 };
            Interval _7381 = jet_mul_derivative(_5887, _5888, intervalFailed, optical_product_upper);
            Interval _5889 = _7381;
            Interval _7382 = jet_add_derivative(_5886, _5889, intervalFailed);
            Interval _5890 = Interval{ 0.0, 0.0 };
            Interval _5891 = Interval{ _6836, _6837 };
            Interval _7384 = jet_mul_derivative(_5890, _5891, intervalFailed, optical_product_upper);
            Interval _5892 = _7384;
            Interval _5893 = Interval{ _7370.x, _7370.x };
            Interval _5894 = Interval{ _6840, _6841 };
            Interval _7387 = jet_mul_derivative(_5893, _5894, intervalFailed, optical_product_upper);
            Interval _5895 = _7387;
            Interval _7388 = jet_add_derivative(_5892, _5895, intervalFailed);
            Interval _5868 = Interval{ _7370.y, _7370.y };
            Interval _5869 = Interval{ _6842, _6843 };
            Interval _7391 = imul(_5868, _5869, intervalFailed, optical_product_upper);
            Interval _5870 = Interval{ 0.0, 0.0 };
            Interval _5871 = Interval{ _6842, _6843 };
            Interval _7393 = jet_mul_derivative(_5870, _5871, intervalFailed, optical_product_upper);
            Interval _5872 = _7393;
            Interval _5873 = Interval{ _7370.y, _7370.y };
            Interval _5874 = Interval{ _6844, _6845 };
            Interval _7396 = jet_mul_derivative(_5873, _5874, intervalFailed, optical_product_upper);
            Interval _5875 = _7396;
            Interval _7397 = jet_add_derivative(_5872, _5875, intervalFailed);
            Interval _5876 = Interval{ 0.0, 0.0 };
            Interval _5877 = Interval{ _6842, _6843 };
            Interval _7399 = jet_mul_derivative(_5876, _5877, intervalFailed, optical_product_upper);
            Interval _5878 = _7399;
            Interval _5879 = Interval{ _7370.y, _7370.y };
            Interval _5880 = Interval{ _6846, _6847 };
            Interval _7402 = jet_mul_derivative(_5879, _5880, intervalFailed, optical_product_upper);
            Interval _5881 = _7402;
            Interval _7403 = jet_add_derivative(_5878, _5881, intervalFailed);
            Interval _5862 = _7376;
            Interval _5863 = _7391;
            Interval _7404 = iadd(_5862, _5863, intervalFailed);
            Interval _5864 = _7382;
            Interval _5865 = _7397;
            Interval _7405 = jet_add_derivative(_5864, _5865, intervalFailed);
            Interval _5866 = _7388;
            Interval _5867 = _7403;
            Interval _7406 = jet_add_derivative(_5866, _5867, intervalFailed);
            Interval _5848 = Interval{ _7370.z, _7370.z };
            Interval _5849 = Interval{ _6848, _6849 };
            Interval _7409 = imul(_5848, _5849, intervalFailed, optical_product_upper);
            Interval _5850 = Interval{ 0.0, 0.0 };
            Interval _5851 = Interval{ _6848, _6849 };
            Interval _7411 = jet_mul_derivative(_5850, _5851, intervalFailed, optical_product_upper);
            Interval _5852 = _7411;
            Interval _5853 = Interval{ _7370.z, _7370.z };
            Interval _5854 = Interval{ _6850, _6851 };
            Interval _7414 = jet_mul_derivative(_5853, _5854, intervalFailed, optical_product_upper);
            Interval _5855 = _7414;
            Interval _7415 = jet_add_derivative(_5852, _5855, intervalFailed);
            Interval _5856 = Interval{ 0.0, 0.0 };
            Interval _5857 = Interval{ _6848, _6849 };
            Interval _7417 = jet_mul_derivative(_5856, _5857, intervalFailed, optical_product_upper);
            Interval _5858 = _7417;
            Interval _5859 = Interval{ _7370.z, _7370.z };
            Interval _5860 = Interval{ _6852, _6853 };
            Interval _7420 = jet_mul_derivative(_5859, _5860, intervalFailed, optical_product_upper);
            Interval _5861 = _7420;
            Interval _7421 = jet_add_derivative(_5858, _5861, intervalFailed);
            Interval _5842 = _7404;
            Interval _5843 = _7409;
            Interval _7422 = iadd(_5842, _5843, intervalFailed);
            Interval _5844 = _7405;
            Interval _5845 = _7415;
            Interval _7423 = jet_add_derivative(_5844, _5845, intervalFailed);
            Interval _5846 = _7406;
            Interval _5847 = _7421;
            Interval _7424 = jet_add_derivative(_5846, _5847, intervalFailed);
            Interval _5828 = _7422;
            Interval _5829 = _6908;
            Interval _7426 = idiv(_5828, _5829, intervalFailed, interval_divide_upper);
            bool _7435;
            if (!intervalFailed)
            {
                _7435 = intervalFailed;
            }
            else
            {
                _7435 = false;
            }
            bool _7440;
            if (_7435)
            {
                _7440 = jetFailureSite == 0u;
            }
            else
            {
                _7440 = false;
            }
            if (_7440)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7422.lo, _7422.hi, _6908.lo, _6908.hi);
            }
            Interval _5830 = _7423;
            Interval _5831 = _7426;
            Interval _5832 = _6909;
            Interval _7444 = jet_mul_derivative(_5831, _5832, intervalFailed, optical_product_upper);
            Interval _5833 = Interval{ as_type<float>(as_type<uint>(_7444.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7444.lo) ^ 2147483648u) };
            Interval _7454 = jet_add_derivative(_5830, _5833, intervalFailed);
            Interval _5834 = _7454;
            Interval _5835 = _6908;
            Interval _7455 = jet_div_derivative(_5834, _5835, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5836 = _7424;
            Interval _5837 = _7426;
            Interval _5838 = _6910;
            Interval _7456 = jet_mul_derivative(_5837, _5838, intervalFailed, optical_product_upper);
            Interval _5839 = Interval{ as_type<float>(as_type<uint>(_7456.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7456.lo) ^ 2147483648u) };
            Interval _7466 = jet_add_derivative(_5836, _5839, intervalFailed);
            Interval _5840 = _7466;
            Interval _5841 = _6908;
            Interval _7467 = jet_div_derivative(_5840, _5841, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5814 = _7426;
            Interval _5815 = Interval{ _6708, _6710 };
            Interval _7474 = imul(_5814, _5815, intervalFailed, optical_product_upper);
            Interval _5816 = _7455;
            Interval _5817 = Interval{ _6708, _6710 };
            Interval _7476 = jet_mul_derivative(_5816, _5817, intervalFailed, optical_product_upper);
            Interval _5818 = _7476;
            Interval _5819 = _7426;
            Interval _5820 = Interval{ _6712, _6714 };
            Interval _7478 = jet_mul_derivative(_5819, _5820, intervalFailed, optical_product_upper);
            Interval _5821 = _7478;
            Interval _7479 = jet_add_derivative(_5818, _5821, intervalFailed);
            Interval _5822 = _7467;
            Interval _5823 = Interval{ _6708, _6710 };
            Interval _7481 = jet_mul_derivative(_5822, _5823, intervalFailed, optical_product_upper);
            Interval _5824 = _7481;
            Interval _5825 = _7426;
            Interval _5826 = Interval{ _6716, _6718 };
            Interval _7483 = jet_mul_derivative(_5825, _5826, intervalFailed, optical_product_upper);
            Interval _5827 = _7483;
            Interval _7484 = jet_add_derivative(_5824, _5827, intervalFailed);
            ray.depth = OpticalJet{ Interval{ precise::min(ray.depth.v.lo, _7474.lo), precise::max(ray.depth.v.hi, _7474.hi) }, Interval{ precise::min(ray.depth.dx.lo, _7479.lo), precise::max(ray.depth.dx.hi, _7479.hi) }, Interval{ precise::min(ray.depth.dy.lo, _7484.lo), precise::max(ray.depth.dy.hi, _7484.hi) } };
            bool _7515;
            if ((isunordered(_7317.lo, 0.0) || _7317.lo > 0.0))
            {
                _7515 = ray.depth.v.lo < receiver.extentClip.z;
            }
            else
            {
                _7515 = true;
            }
            bool _7523;
            if (!_7515)
            {
                _7523 = ray.depth.v.hi > receiver.extentClip.w;
            }
            else
            {
                _7523 = true;
            }
            if (_7523)
            {
                return false;
            }
            _7524 = _7317.lo;
            _7525 = _7317.hi;
            _7526 = _7320.lo;
            _7527 = _7320.hi;
            _7528 = _7323.lo;
            _7529 = _7323.hi;
        }
        else
        {
            float3 _6935;
            if (_6770)
            {
                _6935 = liquid.planePoint.xyz;
            }
            else
            {
                _6935 = _6782.xyz;
            }
            Interval _5808 = Interval{ _6935.x, _6935.x };
            Interval _5809 = Interval{ as_type<float>(as_type<uint>(_6722) ^ 2147483648u), as_type<float>(as_type<uint>(_6720) ^ 2147483648u) };
            Interval _6995 = iadd(_5808, _5809, intervalFailed);
            Interval _5810 = Interval{ 0.0, 0.0 };
            Interval _5811 = Interval{ as_type<float>(as_type<uint>(_6726) ^ 2147483648u), as_type<float>(as_type<uint>(_6724) ^ 2147483648u) };
            Interval _6997 = jet_add_derivative(_5810, _5811, intervalFailed);
            Interval _5812 = Interval{ 0.0, 0.0 };
            Interval _5813 = Interval{ as_type<float>(as_type<uint>(_6730) ^ 2147483648u), as_type<float>(as_type<uint>(_6728) ^ 2147483648u) };
            Interval _6999 = jet_add_derivative(_5812, _5813, intervalFailed);
            Interval _5802 = Interval{ _6935.y, _6935.y };
            Interval _5803 = Interval{ as_type<float>(as_type<uint>(_6734) ^ 2147483648u), as_type<float>(as_type<uint>(_6732) ^ 2147483648u) };
            Interval _7002 = iadd(_5802, _5803, intervalFailed);
            Interval _5804 = Interval{ 0.0, 0.0 };
            Interval _5805 = Interval{ as_type<float>(as_type<uint>(_6738) ^ 2147483648u), as_type<float>(as_type<uint>(_6736) ^ 2147483648u) };
            Interval _7004 = jet_add_derivative(_5804, _5805, intervalFailed);
            Interval _5806 = Interval{ 0.0, 0.0 };
            Interval _5807 = Interval{ as_type<float>(as_type<uint>(_6742) ^ 2147483648u), as_type<float>(as_type<uint>(_6740) ^ 2147483648u) };
            Interval _7006 = jet_add_derivative(_5806, _5807, intervalFailed);
            Interval _5796 = Interval{ _6935.z, _6935.z };
            Interval _5797 = Interval{ as_type<float>(as_type<uint>(_6746) ^ 2147483648u), as_type<float>(as_type<uint>(_6744) ^ 2147483648u) };
            Interval _7009 = iadd(_5796, _5797, intervalFailed);
            Interval _5798 = Interval{ 0.0, 0.0 };
            Interval _5799 = Interval{ as_type<float>(as_type<uint>(_6750) ^ 2147483648u), as_type<float>(as_type<uint>(_6748) ^ 2147483648u) };
            Interval _7011 = jet_add_derivative(_5798, _5799, intervalFailed);
            Interval _5800 = Interval{ 0.0, 0.0 };
            Interval _5801 = Interval{ as_type<float>(as_type<uint>(_6754) ^ 2147483648u), as_type<float>(as_type<uint>(_6752) ^ 2147483648u) };
            Interval _7013 = jet_add_derivative(_5800, _5801, intervalFailed);
            Interval _5782 = _6995;
            Interval _5783 = Interval{ _6836, _6837 };
            Interval _7015 = imul(_5782, _5783, intervalFailed, optical_product_upper);
            Interval _5784 = _6997;
            Interval _5785 = Interval{ _6836, _6837 };
            Interval _7017 = jet_mul_derivative(_5784, _5785, intervalFailed, optical_product_upper);
            Interval _5786 = _7017;
            Interval _5787 = _6995;
            Interval _5788 = Interval{ _6838, _6839 };
            Interval _7019 = jet_mul_derivative(_5787, _5788, intervalFailed, optical_product_upper);
            Interval _5789 = _7019;
            Interval _7020 = jet_add_derivative(_5786, _5789, intervalFailed);
            Interval _5790 = _6999;
            Interval _5791 = Interval{ _6836, _6837 };
            Interval _7022 = jet_mul_derivative(_5790, _5791, intervalFailed, optical_product_upper);
            Interval _5792 = _7022;
            Interval _5793 = _6995;
            Interval _5794 = Interval{ _6840, _6841 };
            Interval _7024 = jet_mul_derivative(_5793, _5794, intervalFailed, optical_product_upper);
            Interval _5795 = _7024;
            Interval _7025 = jet_add_derivative(_5792, _5795, intervalFailed);
            Interval _5768 = _7002;
            Interval _5769 = Interval{ _6842, _6843 };
            Interval _7027 = imul(_5768, _5769, intervalFailed, optical_product_upper);
            Interval _5770 = _7004;
            Interval _5771 = Interval{ _6842, _6843 };
            Interval _7029 = jet_mul_derivative(_5770, _5771, intervalFailed, optical_product_upper);
            Interval _5772 = _7029;
            Interval _5773 = _7002;
            Interval _5774 = Interval{ _6844, _6845 };
            Interval _7031 = jet_mul_derivative(_5773, _5774, intervalFailed, optical_product_upper);
            Interval _5775 = _7031;
            Interval _7032 = jet_add_derivative(_5772, _5775, intervalFailed);
            Interval _5776 = _7006;
            Interval _5777 = Interval{ _6842, _6843 };
            Interval _7034 = jet_mul_derivative(_5776, _5777, intervalFailed, optical_product_upper);
            Interval _5778 = _7034;
            Interval _5779 = _7002;
            Interval _5780 = Interval{ _6846, _6847 };
            Interval _7036 = jet_mul_derivative(_5779, _5780, intervalFailed, optical_product_upper);
            Interval _5781 = _7036;
            Interval _7037 = jet_add_derivative(_5778, _5781, intervalFailed);
            Interval _5762 = _7015;
            Interval _5763 = _7027;
            Interval _7038 = iadd(_5762, _5763, intervalFailed);
            Interval _5764 = _7020;
            Interval _5765 = _7032;
            Interval _7039 = jet_add_derivative(_5764, _5765, intervalFailed);
            Interval _5766 = _7025;
            Interval _5767 = _7037;
            Interval _7040 = jet_add_derivative(_5766, _5767, intervalFailed);
            Interval _5748 = _7009;
            Interval _5749 = Interval{ _6848, _6849 };
            Interval _7042 = imul(_5748, _5749, intervalFailed, optical_product_upper);
            Interval _5750 = _7011;
            Interval _5751 = Interval{ _6848, _6849 };
            Interval _7044 = jet_mul_derivative(_5750, _5751, intervalFailed, optical_product_upper);
            Interval _5752 = _7044;
            Interval _5753 = _7009;
            Interval _5754 = Interval{ _6850, _6851 };
            Interval _7046 = jet_mul_derivative(_5753, _5754, intervalFailed, optical_product_upper);
            Interval _5755 = _7046;
            Interval _7047 = jet_add_derivative(_5752, _5755, intervalFailed);
            Interval _5756 = _7013;
            Interval _5757 = Interval{ _6848, _6849 };
            Interval _7049 = jet_mul_derivative(_5756, _5757, intervalFailed, optical_product_upper);
            Interval _5758 = _7049;
            Interval _5759 = _7009;
            Interval _5760 = Interval{ _6852, _6853 };
            Interval _7051 = jet_mul_derivative(_5759, _5760, intervalFailed, optical_product_upper);
            Interval _5761 = _7051;
            Interval _7052 = jet_add_derivative(_5758, _5761, intervalFailed);
            Interval _5742 = _7038;
            Interval _5743 = _7042;
            Interval _7053 = iadd(_5742, _5743, intervalFailed);
            Interval _5744 = _7039;
            Interval _5745 = _7047;
            Interval _7054 = jet_add_derivative(_5744, _5745, intervalFailed);
            Interval _5746 = _7040;
            Interval _5747 = _7052;
            Interval _7055 = jet_add_derivative(_5746, _5747, intervalFailed);
            Interval _5728 = _7053;
            Interval _5729 = _6908;
            Interval _7057 = idiv(_5728, _5729, intervalFailed, interval_divide_upper);
            bool _7066;
            if (!intervalFailed)
            {
                _7066 = intervalFailed;
            }
            else
            {
                _7066 = false;
            }
            bool _7071;
            if (_7066)
            {
                _7071 = jetFailureSite == 0u;
            }
            else
            {
                _7071 = false;
            }
            if (_7071)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7053.lo, _7053.hi, _6908.lo, _6908.hi);
            }
            Interval _5730 = _7054;
            Interval _5731 = _7057;
            Interval _5732 = _6909;
            Interval _7075 = jet_mul_derivative(_5731, _5732, intervalFailed, optical_product_upper);
            Interval _5733 = Interval{ as_type<float>(as_type<uint>(_7075.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7075.lo) ^ 2147483648u) };
            Interval _7085 = jet_add_derivative(_5730, _5733, intervalFailed);
            Interval _5734 = _7085;
            Interval _5735 = _6908;
            Interval _7086 = jet_div_derivative(_5734, _5735, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5736 = _7055;
            Interval _5737 = _7057;
            Interval _5738 = _6910;
            Interval _7087 = jet_mul_derivative(_5737, _5738, intervalFailed, optical_product_upper);
            Interval _5739 = Interval{ as_type<float>(as_type<uint>(_7087.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7087.lo) ^ 2147483648u) };
            Interval _7097 = jet_add_derivative(_5736, _5739, intervalFailed);
            Interval _5740 = _7097;
            Interval _5741 = _6908;
            Interval _7098 = jet_div_derivative(_5740, _5741, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            bool _7107;
            if ((isunordered(_7057.lo, _6674) || _7057.lo > _6674))
            {
                _7107 = _7057.hi >= 65536.0;
            }
            else
            {
                _7107 = true;
            }
            if (_7107)
            {
                return false;
            }
            Interval _5714 = Interval{ _6684, _6686 };
            Interval _5715 = _7057;
            Interval _7109 = imul(_5714, _5715, intervalFailed, optical_product_upper);
            Interval _5716 = Interval{ _6688, _6690 };
            Interval _5717 = _7057;
            Interval _7111 = jet_mul_derivative(_5716, _5717, intervalFailed, optical_product_upper);
            Interval _5718 = _7111;
            Interval _5719 = Interval{ _6684, _6686 };
            Interval _5720 = _7086;
            Interval _7113 = jet_mul_derivative(_5719, _5720, intervalFailed, optical_product_upper);
            Interval _5721 = _7113;
            Interval _7114 = jet_add_derivative(_5718, _5721, intervalFailed);
            Interval _5722 = Interval{ _6692, _6694 };
            Interval _5723 = _7057;
            Interval _7116 = jet_mul_derivative(_5722, _5723, intervalFailed, optical_product_upper);
            Interval _5724 = _7116;
            Interval _5725 = Interval{ _6684, _6686 };
            Interval _5726 = _7098;
            Interval _7118 = jet_mul_derivative(_5725, _5726, intervalFailed, optical_product_upper);
            Interval _5727 = _7118;
            Interval _7119 = jet_add_derivative(_5724, _5727, intervalFailed);
            Interval _5700 = Interval{ _6696, _6698 };
            Interval _5701 = _7057;
            Interval _7121 = imul(_5700, _5701, intervalFailed, optical_product_upper);
            Interval _5702 = Interval{ _6700, _6702 };
            Interval _5703 = _7057;
            Interval _7123 = jet_mul_derivative(_5702, _5703, intervalFailed, optical_product_upper);
            Interval _5704 = _7123;
            Interval _5705 = Interval{ _6696, _6698 };
            Interval _5706 = _7086;
            Interval _7125 = jet_mul_derivative(_5705, _5706, intervalFailed, optical_product_upper);
            Interval _5707 = _7125;
            Interval _7126 = jet_add_derivative(_5704, _5707, intervalFailed);
            Interval _5708 = Interval{ _6704, _6706 };
            Interval _5709 = _7057;
            Interval _7128 = jet_mul_derivative(_5708, _5709, intervalFailed, optical_product_upper);
            Interval _5710 = _7128;
            Interval _5711 = Interval{ _6696, _6698 };
            Interval _5712 = _7098;
            Interval _7130 = jet_mul_derivative(_5711, _5712, intervalFailed, optical_product_upper);
            Interval _5713 = _7130;
            Interval _7131 = jet_add_derivative(_5710, _5713, intervalFailed);
            Interval _5686 = Interval{ _6708, _6710 };
            Interval _5687 = _7057;
            Interval _7133 = imul(_5686, _5687, intervalFailed, optical_product_upper);
            Interval _5688 = Interval{ _6712, _6714 };
            Interval _5689 = _7057;
            Interval _7135 = jet_mul_derivative(_5688, _5689, intervalFailed, optical_product_upper);
            Interval _5690 = _7135;
            Interval _5691 = Interval{ _6708, _6710 };
            Interval _5692 = _7086;
            Interval _7137 = jet_mul_derivative(_5691, _5692, intervalFailed, optical_product_upper);
            Interval _5693 = _7137;
            Interval _7138 = jet_add_derivative(_5690, _5693, intervalFailed);
            Interval _5694 = Interval{ _6716, _6718 };
            Interval _5695 = _7057;
            Interval _7140 = jet_mul_derivative(_5694, _5695, intervalFailed, optical_product_upper);
            Interval _5696 = _7140;
            Interval _5697 = Interval{ _6708, _6710 };
            Interval _5698 = _7098;
            Interval _7142 = jet_mul_derivative(_5697, _5698, intervalFailed, optical_product_upper);
            Interval _5699 = _7142;
            Interval _7143 = jet_add_derivative(_5696, _5699, intervalFailed);
            Interval _5680 = Interval{ _6720, _6722 };
            Interval _5681 = _7109;
            Interval _7145 = iadd(_5680, _5681, intervalFailed);
            Interval _5682 = Interval{ _6724, _6726 };
            Interval _5683 = _7114;
            Interval _7147 = jet_add_derivative(_5682, _5683, intervalFailed);
            Interval _5684 = Interval{ _6728, _6730 };
            Interval _5685 = _7119;
            Interval _7149 = jet_add_derivative(_5684, _5685, intervalFailed);
            Interval _5674 = Interval{ _6732, _6734 };
            Interval _5675 = _7121;
            Interval _7151 = iadd(_5674, _5675, intervalFailed);
            Interval _5676 = Interval{ _6736, _6738 };
            Interval _5677 = _7126;
            Interval _7153 = jet_add_derivative(_5676, _5677, intervalFailed);
            Interval _5678 = Interval{ _6740, _6742 };
            Interval _5679 = _7131;
            Interval _7155 = jet_add_derivative(_5678, _5679, intervalFailed);
            Interval _5668 = Interval{ _6744, _6746 };
            Interval _5669 = _7133;
            Interval _7157 = iadd(_5668, _5669, intervalFailed);
            Interval _5670 = Interval{ _6748, _6750 };
            Interval _5671 = _7138;
            Interval _7159 = jet_add_derivative(_5670, _5671, intervalFailed);
            Interval _5672 = Interval{ _6752, _6754 };
            Interval _5673 = _7143;
            Interval _7161 = jet_add_derivative(_5672, _5673, intervalFailed);
            hit = OpticalJet3{ OpticalJet{ _7145, _7147, _7149 }, OpticalJet{ _7151, _7153, _7155 }, OpticalJet{ _7157, _7159, _7161 } };
            _7524 = _7057.lo;
            _7525 = _7057.hi;
            _7526 = _7086.lo;
            _7527 = _7086.hi;
            _7528 = _7098.lo;
            _7529 = _7098.hi;
        }
        float _7844;
        float _7845;
        float _7846;
        float _7847;
        float _7848;
        float _7849;
        float _7850;
        float _7851;
        float _7852;
        float _7853;
        float _7854;
        float _7855;
        float _7856;
        float _7857;
        float _7858;
        float _7859;
        float _7860;
        float _7861;
        if (_6770)
        {
            float _7675;
            float _7676;
            float _7677;
            float _7678;
            float _7679;
            float _7680;
            if (_6760)
            {
                _7675 = _7524;
                _7676 = _7525;
                _7677 = _7526;
                _7678 = _7527;
                _7679 = _7528;
                _7680 = _7529;
            }
            else
            {
                bool _7547;
                if (hit.x.v.lo <= 0.0)
                {
                    _7547 = hit.x.v.hi >= 0.0;
                }
                else
                {
                    _7547 = false;
                }
                float _7554;
                if (_7547)
                {
                    _7554 = 0.0;
                }
                else
                {
                    _7554 = precise::min(abs(hit.x.v.lo), abs(hit.x.v.hi));
                }
                float _7557 = precise::max(abs(hit.x.v.lo), abs(hit.x.v.hi));
                float _5658 = spvFMul(_7554, _7554);
                float _7558 = interval_down(_5658, intervalFailed);
                float _5659 = spvFMul(_7557, _7557);
                float _7560 = interval_up(_5659, intervalFailed);
                Interval _5660 = Interval{ 2.0, 2.0 };
                Interval _5661 = hit.x.v;
                Interval _7561 = imul(_5660, _5661, intervalFailed, optical_product_upper);
                Interval _5662 = _7561;
                Interval _5663 = hit.x.dx;
                Interval _7562 = jet_mul_derivative(_5662, _5663, intervalFailed, optical_product_upper);
                Interval _5664 = Interval{ 2.0, 2.0 };
                Interval _5665 = hit.x.v;
                Interval _7563 = imul(_5664, _5665, intervalFailed, optical_product_upper);
                Interval _5666 = _7563;
                Interval _5667 = hit.x.dy;
                Interval _7564 = jet_mul_derivative(_5666, _5667, intervalFailed, optical_product_upper);
                bool _7574;
                if (hit.y.v.lo <= 0.0)
                {
                    _7574 = hit.y.v.hi >= 0.0;
                }
                else
                {
                    _7574 = false;
                }
                float _7581;
                if (_7574)
                {
                    _7581 = 0.0;
                }
                else
                {
                    _7581 = precise::min(abs(hit.y.v.lo), abs(hit.y.v.hi));
                }
                float _7584 = precise::max(abs(hit.y.v.lo), abs(hit.y.v.hi));
                float _5648 = spvFMul(_7581, _7581);
                float _7585 = interval_down(_5648, intervalFailed);
                float _5649 = spvFMul(_7584, _7584);
                float _7587 = interval_up(_5649, intervalFailed);
                Interval _5650 = Interval{ 2.0, 2.0 };
                Interval _5651 = hit.y.v;
                Interval _7588 = imul(_5650, _5651, intervalFailed, optical_product_upper);
                Interval _5652 = _7588;
                Interval _5653 = hit.y.dx;
                Interval _7589 = jet_mul_derivative(_5652, _5653, intervalFailed, optical_product_upper);
                Interval _5654 = Interval{ 2.0, 2.0 };
                Interval _5655 = hit.y.v;
                Interval _7590 = imul(_5654, _5655, intervalFailed, optical_product_upper);
                Interval _5656 = _7590;
                Interval _5657 = hit.y.dy;
                Interval _7591 = jet_mul_derivative(_5656, _5657, intervalFailed, optical_product_upper);
                Interval _5642 = Interval{ precise::max(0.0, _7558), _7560 };
                Interval _5643 = Interval{ precise::max(0.0, _7585), _7587 };
                Interval _7594 = iadd(_5642, _5643, intervalFailed);
                Interval _5644 = _7562;
                Interval _5645 = _7589;
                Interval _7595 = jet_add_derivative(_5644, _5645, intervalFailed);
                Interval _5646 = _7564;
                Interval _5647 = _7591;
                Interval _7596 = jet_add_derivative(_5646, _5647, intervalFailed);
                bool _7606;
                if (hit.z.v.lo <= 0.0)
                {
                    _7606 = hit.z.v.hi >= 0.0;
                }
                else
                {
                    _7606 = false;
                }
                float _7613;
                if (_7606)
                {
                    _7613 = 0.0;
                }
                else
                {
                    _7613 = precise::min(abs(hit.z.v.lo), abs(hit.z.v.hi));
                }
                float _7616 = precise::max(abs(hit.z.v.lo), abs(hit.z.v.hi));
                float _5632 = spvFMul(_7613, _7613);
                float _7617 = interval_down(_5632, intervalFailed);
                float _5633 = spvFMul(_7616, _7616);
                float _7619 = interval_up(_5633, intervalFailed);
                Interval _5634 = Interval{ 2.0, 2.0 };
                Interval _5635 = hit.z.v;
                Interval _7620 = imul(_5634, _5635, intervalFailed, optical_product_upper);
                Interval _5636 = _7620;
                Interval _5637 = hit.z.dx;
                Interval _7621 = jet_mul_derivative(_5636, _5637, intervalFailed, optical_product_upper);
                Interval _5638 = Interval{ 2.0, 2.0 };
                Interval _5639 = hit.z.v;
                Interval _7622 = imul(_5638, _5639, intervalFailed, optical_product_upper);
                Interval _5640 = _7622;
                Interval _5641 = hit.z.dy;
                Interval _7623 = jet_mul_derivative(_5640, _5641, intervalFailed, optical_product_upper);
                Interval _5626 = _7594;
                Interval _5627 = Interval{ precise::max(0.0, _7617), _7619 };
                Interval _7625 = iadd(_5626, _5627, intervalFailed);
                Interval _5628 = _7595;
                Interval _5629 = _7621;
                Interval _7626 = jet_add_derivative(_5628, _5629, intervalFailed);
                Interval _5630 = _7596;
                Interval _5631 = _7623;
                Interval _7627 = jet_add_derivative(_5630, _5631, intervalFailed);
                Interval _5617 = _7625;
                Interval _7629 = isqrt(_5617, intervalFailed);
                bool _7637;
                if (!intervalFailed)
                {
                    _7637 = intervalFailed;
                }
                else
                {
                    _7637 = false;
                }
                bool _7642;
                if (_7637)
                {
                    _7642 = jetFailureSite == 0u;
                }
                else
                {
                    _7642 = false;
                }
                if (_7642)
                {
                    jetFailureSite = 3u;
                    jetFailureArguments = float4(_7625.lo, _7625.hi, 0.0, 0.0);
                }
                if (_7629.lo <= 0.0)
                {
                    jetBranchKnown = false;
                }
                Interval _5618 = Interval{ 2.0, 2.0 };
                Interval _5619 = _7629;
                Interval _7649 = imul(_5618, _5619, intervalFailed, optical_product_upper);
                Interval _5620 = Interval{ 1.0, 1.0 };
                Interval _5621 = _7649;
                Interval _7651 = idiv(_5620, _5621, intervalFailed, interval_divide_upper);
                bool _7658;
                if (!intervalFailed)
                {
                    _7658 = intervalFailed;
                }
                else
                {
                    _7658 = false;
                }
                bool _7663;
                if (_7658)
                {
                    _7663 = jetFailureSite == 0u;
                }
                else
                {
                    _7663 = false;
                }
                if (_7663)
                {
                    jetFailureSite = 4u;
                    jetFailureArguments = float4(1.0, 1.0, _7649.lo, _7649.hi);
                }
                Interval _5622 = _7651;
                Interval _5623 = _7626;
                Interval _7667 = jet_mul_derivative(_5622, _5623, intervalFailed, optical_product_upper);
                Interval _5624 = _7651;
                Interval _5625 = _7627;
                Interval _7668 = jet_mul_derivative(_5624, _5625, intervalFailed, optical_product_upper);
                _7675 = _7629.lo;
                _7676 = _7629.hi;
                _7677 = _7667.lo;
                _7678 = _7667.hi;
                _7679 = _7668.lo;
                _7680 = _7668.hi;
            }
            float _7684 = precise::max(liquid.projection.x, 1.0);
            Interval _5603 = Interval{ _7675, _7676 };
            Interval _5604 = Interval{ _7684, _7684 };
            Interval _7688 = idiv(_5603, _5604, intervalFailed, interval_divide_upper);
            bool _7693;
            if (!intervalFailed)
            {
                _7693 = intervalFailed;
            }
            else
            {
                _7693 = false;
            }
            bool _7698;
            if (_7693)
            {
                _7698 = jetFailureSite == 0u;
            }
            else
            {
                _7698 = false;
            }
            if (_7698)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7675, _7676, _7684, _7684);
            }
            Interval _5605 = Interval{ _7677, _7678 };
            Interval _5606 = _7688;
            Interval _5607 = Interval{ 0.0, 0.0 };
            Interval _7703 = jet_mul_derivative(_5606, _5607, intervalFailed, optical_product_upper);
            Interval _5608 = Interval{ as_type<float>(as_type<uint>(_7703.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7703.lo) ^ 2147483648u) };
            Interval _7713 = jet_add_derivative(_5605, _5608, intervalFailed);
            Interval _5609 = _7713;
            Interval _5610 = Interval{ _7684, _7684 };
            Interval _7715 = jet_div_derivative(_5609, _5610, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5611 = Interval{ _7679, _7680 };
            Interval _5612 = _7688;
            Interval _5613 = Interval{ 0.0, 0.0 };
            Interval _7717 = jet_mul_derivative(_5612, _5613, intervalFailed, optical_product_upper);
            Interval _5614 = Interval{ as_type<float>(as_type<uint>(_7717.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7717.lo) ^ 2147483648u) };
            Interval _7727 = jet_add_derivative(_5611, _5614, intervalFailed);
            Interval _5615 = _7727;
            Interval _5616 = Interval{ _7684, _7684 };
            Interval _7729 = jet_div_derivative(_5615, _5616, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            OpticalJet param_var_a = OpticalJet{ _6908, _6909, _6910 };
            OpticalJet param_var_a_1 = jabs(param_var_a);
            float _5601 = 4.0;
            float _5602 = 100.0;
            Interval _7733 = iratio(_5601, _5602, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _7738;
            if (!intervalFailed)
            {
                _7738 = intervalFailed;
            }
            else
            {
                _7738 = false;
            }
            bool _7743;
            if (_7738)
            {
                _7743 = jetFailureSite == 0u;
            }
            else
            {
                _7743 = false;
            }
            if (_7743)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(4.0, 4.0, 100.0, 100.0);
            }
            OpticalJet param_var_b = OpticalJet{ _7733, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
            OpticalJet _7747 = jmax(param_var_a_1, param_var_b);
            Interval _7748 = _7747.v;
            Interval _5587 = _7688;
            Interval _5588 = _7748;
            Interval _7752 = idiv(_5587, _5588, intervalFailed, interval_divide_upper);
            bool _7761;
            if (!intervalFailed)
            {
                _7761 = intervalFailed;
            }
            else
            {
                _7761 = false;
            }
            bool _7766;
            if (_7761)
            {
                _7766 = jetFailureSite == 0u;
            }
            else
            {
                _7766 = false;
            }
            if (_7766)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7688.lo, _7688.hi, _7748.lo, _7748.hi);
            }
            Interval _5589 = _7715;
            Interval _5590 = _7752;
            Interval _5591 = _7747.dx;
            Interval _7770 = jet_mul_derivative(_5590, _5591, intervalFailed, optical_product_upper);
            Interval _5592 = Interval{ as_type<float>(as_type<uint>(_7770.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7770.lo) ^ 2147483648u) };
            Interval _7780 = jet_add_derivative(_5589, _5592, intervalFailed);
            Interval _5593 = _7780;
            Interval _5594 = _7748;
            Interval _7781 = jet_div_derivative(_5593, _5594, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5595 = _7729;
            Interval _5596 = _7752;
            Interval _5597 = _7747.dy;
            Interval _7782 = jet_mul_derivative(_5596, _5597, intervalFailed, optical_product_upper);
            Interval _5598 = Interval{ as_type<float>(as_type<uint>(_7782.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7782.lo) ^ 2147483648u) };
            Interval _7792 = jet_add_derivative(_5595, _5598, intervalFailed);
            Interval _5599 = _7792;
            Interval _5600 = _7748;
            Interval _7793 = jet_div_derivative(_5599, _5600, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            ReflectionLiquidFrame param_var_f = liquid;
            OpticalJet3 param_var_direction_1 = OpticalJet3{ OpticalJet{ Interval{ _6684, _6686 }, Interval{ _6688, _6690 }, Interval{ _6692, _6694 } }, OpticalJet{ Interval{ _6696, _6698 }, Interval{ _6700, _6702 }, Interval{ _6704, _6706 } }, OpticalJet{ Interval{ _6708, _6710 }, Interval{ _6712, _6714 }, Interval{ _6716, _6718 } } };
            OpticalJet param_var_distance = OpticalJet{ Interval{ _7524, _7525 }, Interval{ _7526, _7527 }, Interval{ _7528, _7529 } };
            OpticalJet param_var_footprint = OpticalJet{ _7752, _7781, _7793 };
            OpticalJet3 _7813 = jet_liquid_normal(param_var_f, param_var_direction_1, param_var_distance, param_var_footprint, hit, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            _7844 = _7813.x.v.lo;
            _7845 = _7813.x.v.hi;
            _7846 = _7813.x.dx.lo;
            _7847 = _7813.x.dx.hi;
            _7848 = _7813.x.dy.lo;
            _7849 = _7813.x.dy.hi;
            _7850 = _7813.y.v.lo;
            _7851 = _7813.y.v.hi;
            _7852 = _7813.y.dx.lo;
            _7853 = _7813.y.dx.hi;
            _7854 = _7813.y.dy.lo;
            _7855 = _7813.y.dy.hi;
            _7856 = _7813.z.v.lo;
            _7857 = _7813.z.v.hi;
            _7858 = _7813.z.dx.lo;
            _7859 = _7813.z.dx.hi;
            _7860 = _7813.z.dy.lo;
            _7861 = _7813.z.dy.hi;
        }
        else
        {
            ReflectionSpecularPlane param_var_plane_1 = ReflectionSpecularPlane{ _6782, _6783, _6784 };
            OpticalJet3 param_var_hit = hit;
            bool _7532 = jet_inside_face(param_var_plane_1, param_var_hit, intervalFailed, optical_product_upper, interval_divide_upper);
            if (!_7532)
            {
                return false;
            }
            _7844 = _6836;
            _7845 = _6837;
            _7846 = _6838;
            _7847 = _6839;
            _7848 = _6840;
            _7849 = _6841;
            _7850 = _6842;
            _7851 = _6843;
            _7852 = _6844;
            _7853 = _6845;
            _7854 = _6846;
            _7855 = _6847;
            _7856 = _6848;
            _7857 = _6849;
            _7858 = _6850;
            _7859 = _6851;
            _7860 = _6852;
            _7861 = _6853;
        }
        bool _7863;
        if (_6760)
        {
            _7863 = !_6770;
        }
        else
        {
            _7863 = false;
        }
        bool _7877;
        if (_7863)
        {
            bool _7870;
            if (_6782.w == 2.0)
            {
                _7870 = _6783.w == 2.0;
            }
            else
            {
                _7870 = false;
            }
            bool _7875;
            if (_7870)
            {
                _7875 = _6784.w == 2.0;
            }
            else
            {
                _7875 = false;
            }
            _7877 = !_7875;
        }
        else
        {
            _7877 = false;
        }
        int _7878;
        if (_7877)
        {
            _7878 = 1;
        }
        else
        {
            _7878 = 5;
        }
        float _7879 = float(_7878);
        float _5585 = _7879;
        float _5586 = 100.0;
        Interval _7881 = iratio(_5585, _5586, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _7886;
        if (!intervalFailed)
        {
            _7886 = intervalFailed;
        }
        else
        {
            _7886 = false;
        }
        bool _7891;
        if (_7886)
        {
            _7891 = jetFailureSite == 0u;
        }
        else
        {
            _7891 = false;
        }
        if (_7891)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(_7879, _7879, 100.0, 100.0);
        }
        OpticalJet param_var_a_2 = OpticalJet{ _7881, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
        float _5583 = 1.0;
        float _5584 = 100000.0;
        Interval _7897 = iratio(_5583, _5584, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _7902;
        if (!intervalFailed)
        {
            _7902 = intervalFailed;
        }
        else
        {
            _7902 = false;
        }
        bool _7907;
        if (_7902)
        {
            _7907 = jetFailureSite == 0u;
        }
        else
        {
            _7907 = false;
        }
        if (_7907)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(1.0, 1.0, 100000.0, 100000.0);
        }
        Interval _5569 = Interval{ _7524, _7525 };
        Interval _5570 = _7897;
        Interval _7911 = imul(_5569, _5570, intervalFailed, optical_product_upper);
        Interval _5571 = Interval{ _7526, _7527 };
        Interval _5572 = _7897;
        Interval _7913 = jet_mul_derivative(_5571, _5572, intervalFailed, optical_product_upper);
        Interval _5573 = _7913;
        Interval _5574 = Interval{ _7524, _7525 };
        Interval _5575 = Interval{ 0.0, 0.0 };
        Interval _7915 = jet_mul_derivative(_5574, _5575, intervalFailed, optical_product_upper);
        Interval _5576 = _7915;
        Interval _7916 = jet_add_derivative(_5573, _5576, intervalFailed);
        Interval _5577 = Interval{ _7528, _7529 };
        Interval _5578 = _7897;
        Interval _7918 = jet_mul_derivative(_5577, _5578, intervalFailed, optical_product_upper);
        Interval _5579 = _7918;
        Interval _5580 = Interval{ _7524, _7525 };
        Interval _5581 = Interval{ 0.0, 0.0 };
        Interval _7920 = jet_mul_derivative(_5580, _5581, intervalFailed, optical_product_upper);
        Interval _5582 = _7920;
        Interval _7921 = jet_add_derivative(_5579, _5582, intervalFailed);
        OpticalJet param_var_b_1 = OpticalJet{ _7911, _7916, _7921 };
        OpticalJet _7923 = jmax(param_var_a_2, param_var_b_1);
        Interval _7924 = _7923.v;
        _6673 = _7924.lo;
        _6675 = _7924.hi;
        Interval _7925 = _7923.dx;
        _6677 = _7925.lo;
        _6679 = _7925.hi;
        Interval _7926 = _7923.dy;
        _6681 = _7926.lo;
        _6683 = _7926.hi;
        Interval _7931 = _7923.v;
        Interval _5555 = Interval{ _7844, _7845 };
        Interval _5556 = _7931;
        Interval _7935 = imul(_5555, _5556, intervalFailed, optical_product_upper);
        Interval _5557 = Interval{ _7846, _7847 };
        Interval _5558 = _7931;
        Interval _7937 = jet_mul_derivative(_5557, _5558, intervalFailed, optical_product_upper);
        Interval _5559 = _7937;
        Interval _5560 = Interval{ _7844, _7845 };
        Interval _5561 = _7923.dx;
        Interval _7939 = jet_mul_derivative(_5560, _5561, intervalFailed, optical_product_upper);
        Interval _5562 = _7939;
        Interval _7940 = jet_add_derivative(_5559, _5562, intervalFailed);
        Interval _5563 = Interval{ _7848, _7849 };
        Interval _5564 = _7931;
        Interval _7942 = jet_mul_derivative(_5563, _5564, intervalFailed, optical_product_upper);
        Interval _5565 = _7942;
        Interval _5566 = Interval{ _7844, _7845 };
        Interval _5567 = _7923.dy;
        Interval _7944 = jet_mul_derivative(_5566, _5567, intervalFailed, optical_product_upper);
        Interval _5568 = _7944;
        Interval _7945 = jet_add_derivative(_5565, _5568, intervalFailed);
        Interval _7946 = _7923.v;
        Interval _5541 = Interval{ _7850, _7851 };
        Interval _5542 = _7946;
        Interval _7950 = imul(_5541, _5542, intervalFailed, optical_product_upper);
        Interval _5543 = Interval{ _7852, _7853 };
        Interval _5544 = _7946;
        Interval _7952 = jet_mul_derivative(_5543, _5544, intervalFailed, optical_product_upper);
        Interval _5545 = _7952;
        Interval _5546 = Interval{ _7850, _7851 };
        Interval _5547 = _7923.dx;
        Interval _7954 = jet_mul_derivative(_5546, _5547, intervalFailed, optical_product_upper);
        Interval _5548 = _7954;
        Interval _7955 = jet_add_derivative(_5545, _5548, intervalFailed);
        Interval _5549 = Interval{ _7854, _7855 };
        Interval _5550 = _7946;
        Interval _7957 = jet_mul_derivative(_5549, _5550, intervalFailed, optical_product_upper);
        Interval _5551 = _7957;
        Interval _5552 = Interval{ _7850, _7851 };
        Interval _5553 = _7923.dy;
        Interval _7959 = jet_mul_derivative(_5552, _5553, intervalFailed, optical_product_upper);
        Interval _5554 = _7959;
        Interval _7960 = jet_add_derivative(_5551, _5554, intervalFailed);
        Interval _7961 = _7923.v;
        Interval _5527 = Interval{ _7856, _7857 };
        Interval _5528 = _7961;
        Interval _7965 = imul(_5527, _5528, intervalFailed, optical_product_upper);
        Interval _5529 = Interval{ _7858, _7859 };
        Interval _5530 = _7961;
        Interval _7967 = jet_mul_derivative(_5529, _5530, intervalFailed, optical_product_upper);
        Interval _5531 = _7967;
        Interval _5532 = Interval{ _7856, _7857 };
        Interval _5533 = _7923.dx;
        Interval _7969 = jet_mul_derivative(_5532, _5533, intervalFailed, optical_product_upper);
        Interval _5534 = _7969;
        Interval _7970 = jet_add_derivative(_5531, _5534, intervalFailed);
        Interval _5535 = Interval{ _7860, _7861 };
        Interval _5536 = _7961;
        Interval _7972 = jet_mul_derivative(_5535, _5536, intervalFailed, optical_product_upper);
        Interval _5537 = _7972;
        Interval _5538 = Interval{ _7856, _7857 };
        Interval _5539 = _7923.dy;
        Interval _7974 = jet_mul_derivative(_5538, _5539, intervalFailed, optical_product_upper);
        Interval _5540 = _7974;
        Interval _7975 = jet_add_derivative(_5537, _5540, intervalFailed);
        Interval _5521 = hit.x.v;
        Interval _5522 = _7935;
        Interval _7979 = iadd(_5521, _5522, intervalFailed);
        Interval _5523 = hit.x.dx;
        Interval _5524 = _7940;
        Interval _7980 = jet_add_derivative(_5523, _5524, intervalFailed);
        Interval _5525 = hit.x.dy;
        Interval _5526 = _7945;
        Interval _7981 = jet_add_derivative(_5525, _5526, intervalFailed);
        Interval _5515 = hit.y.v;
        Interval _5516 = _7950;
        Interval _7985 = iadd(_5515, _5516, intervalFailed);
        Interval _5517 = hit.y.dx;
        Interval _5518 = _7955;
        Interval _7986 = jet_add_derivative(_5517, _5518, intervalFailed);
        Interval _5519 = hit.y.dy;
        Interval _5520 = _7960;
        Interval _7987 = jet_add_derivative(_5519, _5520, intervalFailed);
        Interval _5509 = hit.z.v;
        Interval _5510 = _7965;
        Interval _7991 = iadd(_5509, _5510, intervalFailed);
        Interval _5511 = hit.z.dx;
        Interval _5512 = _7970;
        Interval _7992 = jet_add_derivative(_5511, _5512, intervalFailed);
        Interval _5513 = hit.z.dy;
        Interval _5514 = _7975;
        Interval _7993 = jet_add_derivative(_5513, _5514, intervalFailed);
        _6721 = _7979.lo;
        _6723 = _7979.hi;
        _6725 = _7980.lo;
        _6727 = _7980.hi;
        _6729 = _7981.lo;
        _6731 = _7981.hi;
        _6733 = _7985.lo;
        _6735 = _7985.hi;
        _6737 = _7986.lo;
        _6739 = _7986.hi;
        _6741 = _7987.lo;
        _6743 = _7987.hi;
        _6745 = _7991.lo;
        _6747 = _7991.hi;
        _6749 = _7992.lo;
        _6751 = _7992.hi;
        _6753 = _7993.lo;
        _6755 = _7993.hi;
        Interval _5495 = Interval{ _6684, _6686 };
        Interval _5496 = Interval{ _7844, _7845 };
        Interval _7996 = imul(_5495, _5496, intervalFailed, optical_product_upper);
        Interval _5497 = Interval{ _6688, _6690 };
        Interval _5498 = Interval{ _7844, _7845 };
        Interval _7999 = jet_mul_derivative(_5497, _5498, intervalFailed, optical_product_upper);
        Interval _5499 = _7999;
        Interval _5500 = Interval{ _6684, _6686 };
        Interval _5501 = Interval{ _7846, _7847 };
        Interval _8002 = jet_mul_derivative(_5500, _5501, intervalFailed, optical_product_upper);
        Interval _5502 = _8002;
        Interval _8003 = jet_add_derivative(_5499, _5502, intervalFailed);
        Interval _5503 = Interval{ _6692, _6694 };
        Interval _5504 = Interval{ _7844, _7845 };
        Interval _8006 = jet_mul_derivative(_5503, _5504, intervalFailed, optical_product_upper);
        Interval _5505 = _8006;
        Interval _5506 = Interval{ _6684, _6686 };
        Interval _5507 = Interval{ _7848, _7849 };
        Interval _8009 = jet_mul_derivative(_5506, _5507, intervalFailed, optical_product_upper);
        Interval _5508 = _8009;
        Interval _8010 = jet_add_derivative(_5505, _5508, intervalFailed);
        Interval _5481 = Interval{ _6696, _6698 };
        Interval _5482 = Interval{ _7850, _7851 };
        Interval _8013 = imul(_5481, _5482, intervalFailed, optical_product_upper);
        Interval _5483 = Interval{ _6700, _6702 };
        Interval _5484 = Interval{ _7850, _7851 };
        Interval _8016 = jet_mul_derivative(_5483, _5484, intervalFailed, optical_product_upper);
        Interval _5485 = _8016;
        Interval _5486 = Interval{ _6696, _6698 };
        Interval _5487 = Interval{ _7852, _7853 };
        Interval _8019 = jet_mul_derivative(_5486, _5487, intervalFailed, optical_product_upper);
        Interval _5488 = _8019;
        Interval _8020 = jet_add_derivative(_5485, _5488, intervalFailed);
        Interval _5489 = Interval{ _6704, _6706 };
        Interval _5490 = Interval{ _7850, _7851 };
        Interval _8023 = jet_mul_derivative(_5489, _5490, intervalFailed, optical_product_upper);
        Interval _5491 = _8023;
        Interval _5492 = Interval{ _6696, _6698 };
        Interval _5493 = Interval{ _7854, _7855 };
        Interval _8026 = jet_mul_derivative(_5492, _5493, intervalFailed, optical_product_upper);
        Interval _5494 = _8026;
        Interval _8027 = jet_add_derivative(_5491, _5494, intervalFailed);
        Interval _5475 = _7996;
        Interval _5476 = _8013;
        Interval _8028 = iadd(_5475, _5476, intervalFailed);
        Interval _5477 = _8003;
        Interval _5478 = _8020;
        Interval _8029 = jet_add_derivative(_5477, _5478, intervalFailed);
        Interval _5479 = _8010;
        Interval _5480 = _8027;
        Interval _8030 = jet_add_derivative(_5479, _5480, intervalFailed);
        Interval _5461 = Interval{ _6708, _6710 };
        Interval _5462 = Interval{ _7856, _7857 };
        Interval _8033 = imul(_5461, _5462, intervalFailed, optical_product_upper);
        Interval _5463 = Interval{ _6712, _6714 };
        Interval _5464 = Interval{ _7856, _7857 };
        Interval _8036 = jet_mul_derivative(_5463, _5464, intervalFailed, optical_product_upper);
        Interval _5465 = _8036;
        Interval _5466 = Interval{ _6708, _6710 };
        Interval _5467 = Interval{ _7858, _7859 };
        Interval _8039 = jet_mul_derivative(_5466, _5467, intervalFailed, optical_product_upper);
        Interval _5468 = _8039;
        Interval _8040 = jet_add_derivative(_5465, _5468, intervalFailed);
        Interval _5469 = Interval{ _6716, _6718 };
        Interval _5470 = Interval{ _7856, _7857 };
        Interval _8043 = jet_mul_derivative(_5469, _5470, intervalFailed, optical_product_upper);
        Interval _5471 = _8043;
        Interval _5472 = Interval{ _6708, _6710 };
        Interval _5473 = Interval{ _7860, _7861 };
        Interval _8046 = jet_mul_derivative(_5472, _5473, intervalFailed, optical_product_upper);
        Interval _5474 = _8046;
        Interval _8047 = jet_add_derivative(_5471, _5474, intervalFailed);
        Interval _5455 = _8028;
        Interval _5456 = _8033;
        Interval _8048 = iadd(_5455, _5456, intervalFailed);
        Interval _5457 = _8029;
        Interval _5458 = _8040;
        Interval _8049 = jet_add_derivative(_5457, _5458, intervalFailed);
        Interval _5459 = _8030;
        Interval _5460 = _8047;
        Interval _8050 = jet_add_derivative(_5459, _5460, intervalFailed);
        Interval _5441 = Interval{ 2.0, 2.0 };
        Interval _5442 = _8048;
        Interval _8051 = imul(_5441, _5442, intervalFailed, optical_product_upper);
        Interval _5443 = Interval{ 0.0, 0.0 };
        Interval _5444 = _8048;
        Interval _8052 = jet_mul_derivative(_5443, _5444, intervalFailed, optical_product_upper);
        Interval _5445 = _8052;
        Interval _5446 = Interval{ 2.0, 2.0 };
        Interval _5447 = _8049;
        Interval _8053 = jet_mul_derivative(_5446, _5447, intervalFailed, optical_product_upper);
        Interval _5448 = _8053;
        Interval _8054 = jet_add_derivative(_5445, _5448, intervalFailed);
        Interval _5449 = Interval{ 0.0, 0.0 };
        Interval _5450 = _8048;
        Interval _8055 = jet_mul_derivative(_5449, _5450, intervalFailed, optical_product_upper);
        Interval _5451 = _8055;
        Interval _5452 = Interval{ 2.0, 2.0 };
        Interval _5453 = _8050;
        Interval _8056 = jet_mul_derivative(_5452, _5453, intervalFailed, optical_product_upper);
        Interval _5454 = _8056;
        Interval _8057 = jet_add_derivative(_5451, _5454, intervalFailed);
        Interval _5427 = Interval{ _7844, _7845 };
        Interval _5428 = _8051;
        Interval _8059 = imul(_5427, _5428, intervalFailed, optical_product_upper);
        Interval _5429 = Interval{ _7846, _7847 };
        Interval _5430 = _8051;
        Interval _8061 = jet_mul_derivative(_5429, _5430, intervalFailed, optical_product_upper);
        Interval _5431 = _8061;
        Interval _5432 = Interval{ _7844, _7845 };
        Interval _5433 = _8054;
        Interval _8063 = jet_mul_derivative(_5432, _5433, intervalFailed, optical_product_upper);
        Interval _5434 = _8063;
        Interval _8064 = jet_add_derivative(_5431, _5434, intervalFailed);
        Interval _5435 = Interval{ _7848, _7849 };
        Interval _5436 = _8051;
        Interval _8066 = jet_mul_derivative(_5435, _5436, intervalFailed, optical_product_upper);
        Interval _5437 = _8066;
        Interval _5438 = Interval{ _7844, _7845 };
        Interval _5439 = _8057;
        Interval _8068 = jet_mul_derivative(_5438, _5439, intervalFailed, optical_product_upper);
        Interval _5440 = _8068;
        Interval _8069 = jet_add_derivative(_5437, _5440, intervalFailed);
        Interval _5413 = Interval{ _7850, _7851 };
        Interval _5414 = _8051;
        Interval _8071 = imul(_5413, _5414, intervalFailed, optical_product_upper);
        Interval _5415 = Interval{ _7852, _7853 };
        Interval _5416 = _8051;
        Interval _8073 = jet_mul_derivative(_5415, _5416, intervalFailed, optical_product_upper);
        Interval _5417 = _8073;
        Interval _5418 = Interval{ _7850, _7851 };
        Interval _5419 = _8054;
        Interval _8075 = jet_mul_derivative(_5418, _5419, intervalFailed, optical_product_upper);
        Interval _5420 = _8075;
        Interval _8076 = jet_add_derivative(_5417, _5420, intervalFailed);
        Interval _5421 = Interval{ _7854, _7855 };
        Interval _5422 = _8051;
        Interval _8078 = jet_mul_derivative(_5421, _5422, intervalFailed, optical_product_upper);
        Interval _5423 = _8078;
        Interval _5424 = Interval{ _7850, _7851 };
        Interval _5425 = _8057;
        Interval _8080 = jet_mul_derivative(_5424, _5425, intervalFailed, optical_product_upper);
        Interval _5426 = _8080;
        Interval _8081 = jet_add_derivative(_5423, _5426, intervalFailed);
        Interval _5399 = Interval{ _7856, _7857 };
        Interval _5400 = _8051;
        Interval _8083 = imul(_5399, _5400, intervalFailed, optical_product_upper);
        Interval _5401 = Interval{ _7858, _7859 };
        Interval _5402 = _8051;
        Interval _8085 = jet_mul_derivative(_5401, _5402, intervalFailed, optical_product_upper);
        Interval _5403 = _8085;
        Interval _5404 = Interval{ _7856, _7857 };
        Interval _5405 = _8054;
        Interval _8087 = jet_mul_derivative(_5404, _5405, intervalFailed, optical_product_upper);
        Interval _5406 = _8087;
        Interval _8088 = jet_add_derivative(_5403, _5406, intervalFailed);
        Interval _5407 = Interval{ _7860, _7861 };
        Interval _5408 = _8051;
        Interval _8090 = jet_mul_derivative(_5407, _5408, intervalFailed, optical_product_upper);
        Interval _5409 = _8090;
        Interval _5410 = Interval{ _7856, _7857 };
        Interval _5411 = _8057;
        Interval _8092 = jet_mul_derivative(_5410, _5411, intervalFailed, optical_product_upper);
        Interval _5412 = _8092;
        Interval _8093 = jet_add_derivative(_5409, _5412, intervalFailed);
        Interval _5393 = Interval{ _6684, _6686 };
        Interval _5394 = Interval{ as_type<float>(as_type<uint>(_8059.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8059.lo) ^ 2147483648u) };
        Interval _8168 = iadd(_5393, _5394, intervalFailed);
        Interval _5395 = Interval{ _6688, _6690 };
        Interval _5396 = Interval{ as_type<float>(as_type<uint>(_8064.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8064.lo) ^ 2147483648u) };
        Interval _8171 = jet_add_derivative(_5395, _5396, intervalFailed);
        Interval _5397 = Interval{ _6692, _6694 };
        Interval _5398 = Interval{ as_type<float>(as_type<uint>(_8069.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8069.lo) ^ 2147483648u) };
        Interval _8174 = jet_add_derivative(_5397, _5398, intervalFailed);
        Interval _5387 = Interval{ _6696, _6698 };
        Interval _5388 = Interval{ as_type<float>(as_type<uint>(_8071.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8071.lo) ^ 2147483648u) };
        Interval _8177 = iadd(_5387, _5388, intervalFailed);
        Interval _5389 = Interval{ _6700, _6702 };
        Interval _5390 = Interval{ as_type<float>(as_type<uint>(_8076.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8076.lo) ^ 2147483648u) };
        Interval _8180 = jet_add_derivative(_5389, _5390, intervalFailed);
        Interval _5391 = Interval{ _6704, _6706 };
        Interval _5392 = Interval{ as_type<float>(as_type<uint>(_8081.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8081.lo) ^ 2147483648u) };
        Interval _8183 = jet_add_derivative(_5391, _5392, intervalFailed);
        Interval _5381 = Interval{ _6708, _6710 };
        Interval _5382 = Interval{ as_type<float>(as_type<uint>(_8083.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8083.lo) ^ 2147483648u) };
        Interval _8186 = iadd(_5381, _5382, intervalFailed);
        Interval _5383 = Interval{ _6712, _6714 };
        Interval _5384 = Interval{ as_type<float>(as_type<uint>(_8088.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8088.lo) ^ 2147483648u) };
        Interval _8189 = jet_add_derivative(_5383, _5384, intervalFailed);
        Interval _5385 = Interval{ _6716, _6718 };
        Interval _5386 = Interval{ as_type<float>(as_type<uint>(_8093.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8093.lo) ^ 2147483648u) };
        Interval _8192 = jet_add_derivative(_5385, _5386, intervalFailed);
        bool _8212;
        if (_6760)
        {
            _8212 = !_6770;
        }
        else
        {
            _8212 = false;
        }
        bool _8217;
        if (_8212)
        {
            _8217 = receiver.settings.x != 0.0;
        }
        else
        {
            _8217 = false;
        }
        if (_8217)
        {
            bool _8224;
            if (_8177.lo <= 0.0)
            {
                _8224 = _8177.hi >= 0.0;
            }
            else
            {
                _8224 = false;
            }
            float _8231;
            if (_8224)
            {
                _8231 = 0.0;
            }
            else
            {
                _8231 = precise::min(abs(_8177.lo), abs(_8177.hi));
            }
            float _5379 = 0.949999988079071044921875;
            float _8235 = interval_down(_5379, intervalFailed);
            float _5380 = 0.949999988079071044921875;
            float _8236 = interval_up(_5380, intervalFailed);
            float _8909;
            float _8910;
            float _8911;
            float _8912;
            float _8913;
            float _8914;
            float _8915;
            float _8916;
            float _8917;
            float _8918;
            float _8919;
            float _8920;
            float _8921;
            float _8922;
            float _8923;
            float _8924;
            float _8925;
            float _8926;
            if (precise::max(abs(_8177.lo), abs(_8177.hi)) < _8235)
            {
                Interval _5365 = _8177;
                Interval _5366 = Interval{ 0.0, 0.0 };
                Interval _8575 = imul(_5365, _5366, intervalFailed, optical_product_upper);
                Interval _5367 = _8180;
                Interval _5368 = Interval{ 0.0, 0.0 };
                Interval _8576 = jet_mul_derivative(_5367, _5368, intervalFailed, optical_product_upper);
                Interval _5369 = _8576;
                Interval _5370 = _8177;
                Interval _5371 = Interval{ 0.0, 0.0 };
                Interval _8577 = jet_mul_derivative(_5370, _5371, intervalFailed, optical_product_upper);
                Interval _5372 = _8577;
                Interval _8578 = jet_add_derivative(_5369, _5372, intervalFailed);
                Interval _5373 = _8183;
                Interval _5374 = Interval{ 0.0, 0.0 };
                Interval _8579 = jet_mul_derivative(_5373, _5374, intervalFailed, optical_product_upper);
                Interval _5375 = _8579;
                Interval _5376 = _8177;
                Interval _5377 = Interval{ 0.0, 0.0 };
                Interval _8580 = jet_mul_derivative(_5376, _5377, intervalFailed, optical_product_upper);
                Interval _5378 = _8580;
                Interval _8581 = jet_add_derivative(_5375, _5378, intervalFailed);
                Interval _5351 = _8186;
                Interval _5352 = Interval{ 1.0, 1.0 };
                Interval _8582 = imul(_5351, _5352, intervalFailed, optical_product_upper);
                Interval _5353 = _8189;
                Interval _5354 = Interval{ 1.0, 1.0 };
                Interval _8583 = jet_mul_derivative(_5353, _5354, intervalFailed, optical_product_upper);
                Interval _5355 = _8583;
                Interval _5356 = _8186;
                Interval _5357 = Interval{ 0.0, 0.0 };
                Interval _8584 = jet_mul_derivative(_5356, _5357, intervalFailed, optical_product_upper);
                Interval _5358 = _8584;
                Interval _8585 = jet_add_derivative(_5355, _5358, intervalFailed);
                Interval _5359 = _8192;
                Interval _5360 = Interval{ 1.0, 1.0 };
                Interval _8586 = jet_mul_derivative(_5359, _5360, intervalFailed, optical_product_upper);
                Interval _5361 = _8586;
                Interval _5362 = _8186;
                Interval _5363 = Interval{ 0.0, 0.0 };
                Interval _8587 = jet_mul_derivative(_5362, _5363, intervalFailed, optical_product_upper);
                Interval _5364 = _8587;
                Interval _8588 = jet_add_derivative(_5361, _5364, intervalFailed);
                Interval _5345 = _8575;
                Interval _5346 = Interval{ as_type<float>(as_type<uint>(_8582.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8582.lo) ^ 2147483648u) };
                Interval _8614 = iadd(_5345, _5346, intervalFailed);
                Interval _5347 = _8578;
                Interval _5348 = Interval{ as_type<float>(as_type<uint>(_8585.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8585.lo) ^ 2147483648u) };
                Interval _8616 = jet_add_derivative(_5347, _5348, intervalFailed);
                Interval _5349 = _8581;
                Interval _5350 = Interval{ as_type<float>(as_type<uint>(_8588.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8588.lo) ^ 2147483648u) };
                Interval _8618 = jet_add_derivative(_5349, _5350, intervalFailed);
                Interval _5331 = _8186;
                Interval _5332 = Interval{ 0.0, 0.0 };
                Interval _8619 = imul(_5331, _5332, intervalFailed, optical_product_upper);
                Interval _5333 = _8189;
                Interval _5334 = Interval{ 0.0, 0.0 };
                Interval _8620 = jet_mul_derivative(_5333, _5334, intervalFailed, optical_product_upper);
                Interval _5335 = _8620;
                Interval _5336 = _8186;
                Interval _5337 = Interval{ 0.0, 0.0 };
                Interval _8621 = jet_mul_derivative(_5336, _5337, intervalFailed, optical_product_upper);
                Interval _5338 = _8621;
                Interval _8622 = jet_add_derivative(_5335, _5338, intervalFailed);
                Interval _5339 = _8192;
                Interval _5340 = Interval{ 0.0, 0.0 };
                Interval _8623 = jet_mul_derivative(_5339, _5340, intervalFailed, optical_product_upper);
                Interval _5341 = _8623;
                Interval _5342 = _8186;
                Interval _5343 = Interval{ 0.0, 0.0 };
                Interval _8624 = jet_mul_derivative(_5342, _5343, intervalFailed, optical_product_upper);
                Interval _5344 = _8624;
                Interval _8625 = jet_add_derivative(_5341, _5344, intervalFailed);
                Interval _5317 = _8168;
                Interval _5318 = Interval{ 0.0, 0.0 };
                Interval _8626 = imul(_5317, _5318, intervalFailed, optical_product_upper);
                Interval _5319 = _8171;
                Interval _5320 = Interval{ 0.0, 0.0 };
                Interval _8627 = jet_mul_derivative(_5319, _5320, intervalFailed, optical_product_upper);
                Interval _5321 = _8627;
                Interval _5322 = _8168;
                Interval _5323 = Interval{ 0.0, 0.0 };
                Interval _8628 = jet_mul_derivative(_5322, _5323, intervalFailed, optical_product_upper);
                Interval _5324 = _8628;
                Interval _8629 = jet_add_derivative(_5321, _5324, intervalFailed);
                Interval _5325 = _8174;
                Interval _5326 = Interval{ 0.0, 0.0 };
                Interval _8630 = jet_mul_derivative(_5325, _5326, intervalFailed, optical_product_upper);
                Interval _5327 = _8630;
                Interval _5328 = _8168;
                Interval _5329 = Interval{ 0.0, 0.0 };
                Interval _8631 = jet_mul_derivative(_5328, _5329, intervalFailed, optical_product_upper);
                Interval _5330 = _8631;
                Interval _8632 = jet_add_derivative(_5327, _5330, intervalFailed);
                Interval _5311 = _8619;
                Interval _5312 = Interval{ as_type<float>(as_type<uint>(_8626.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8626.lo) ^ 2147483648u) };
                Interval _8658 = iadd(_5311, _5312, intervalFailed);
                Interval _5313 = _8622;
                Interval _5314 = Interval{ as_type<float>(as_type<uint>(_8629.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8629.lo) ^ 2147483648u) };
                Interval _8660 = jet_add_derivative(_5313, _5314, intervalFailed);
                Interval _5315 = _8625;
                Interval _5316 = Interval{ as_type<float>(as_type<uint>(_8632.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8632.lo) ^ 2147483648u) };
                Interval _8662 = jet_add_derivative(_5315, _5316, intervalFailed);
                Interval _5297 = _8168;
                Interval _5298 = Interval{ 1.0, 1.0 };
                Interval _8663 = imul(_5297, _5298, intervalFailed, optical_product_upper);
                Interval _5299 = _8171;
                Interval _5300 = Interval{ 1.0, 1.0 };
                Interval _8664 = jet_mul_derivative(_5299, _5300, intervalFailed, optical_product_upper);
                Interval _5301 = _8664;
                Interval _5302 = _8168;
                Interval _5303 = Interval{ 0.0, 0.0 };
                Interval _8665 = jet_mul_derivative(_5302, _5303, intervalFailed, optical_product_upper);
                Interval _5304 = _8665;
                Interval _8666 = jet_add_derivative(_5301, _5304, intervalFailed);
                Interval _5305 = _8174;
                Interval _5306 = Interval{ 1.0, 1.0 };
                Interval _8667 = jet_mul_derivative(_5305, _5306, intervalFailed, optical_product_upper);
                Interval _5307 = _8667;
                Interval _5308 = _8168;
                Interval _5309 = Interval{ 0.0, 0.0 };
                Interval _8668 = jet_mul_derivative(_5308, _5309, intervalFailed, optical_product_upper);
                Interval _5310 = _8668;
                Interval _8669 = jet_add_derivative(_5307, _5310, intervalFailed);
                Interval _5283 = _8177;
                Interval _5284 = Interval{ 0.0, 0.0 };
                Interval _8670 = imul(_5283, _5284, intervalFailed, optical_product_upper);
                Interval _5285 = _8180;
                Interval _5286 = Interval{ 0.0, 0.0 };
                Interval _8671 = jet_mul_derivative(_5285, _5286, intervalFailed, optical_product_upper);
                Interval _5287 = _8671;
                Interval _5288 = _8177;
                Interval _5289 = Interval{ 0.0, 0.0 };
                Interval _8672 = jet_mul_derivative(_5288, _5289, intervalFailed, optical_product_upper);
                Interval _5290 = _8672;
                Interval _8673 = jet_add_derivative(_5287, _5290, intervalFailed);
                Interval _5291 = _8183;
                Interval _5292 = Interval{ 0.0, 0.0 };
                Interval _8674 = jet_mul_derivative(_5291, _5292, intervalFailed, optical_product_upper);
                Interval _5293 = _8674;
                Interval _5294 = _8177;
                Interval _5295 = Interval{ 0.0, 0.0 };
                Interval _8675 = jet_mul_derivative(_5294, _5295, intervalFailed, optical_product_upper);
                Interval _5296 = _8675;
                Interval _8676 = jet_add_derivative(_5293, _5296, intervalFailed);
                Interval _5277 = _8663;
                Interval _5278 = Interval{ as_type<float>(as_type<uint>(_8670.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8670.lo) ^ 2147483648u) };
                Interval _8702 = iadd(_5277, _5278, intervalFailed);
                Interval _5279 = _8666;
                Interval _5280 = Interval{ as_type<float>(as_type<uint>(_8673.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8673.lo) ^ 2147483648u) };
                Interval _8704 = jet_add_derivative(_5279, _5280, intervalFailed);
                Interval _5281 = _8669;
                Interval _5282 = Interval{ as_type<float>(as_type<uint>(_8676.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8676.lo) ^ 2147483648u) };
                Interval _8706 = jet_add_derivative(_5281, _5282, intervalFailed);
                bool _8713;
                if (_8614.lo <= 0.0)
                {
                    _8713 = _8614.hi >= 0.0;
                }
                else
                {
                    _8713 = false;
                }
                float _8720;
                if (_8713)
                {
                    _8720 = 0.0;
                }
                else
                {
                    _8720 = precise::min(abs(_8614.lo), abs(_8614.hi));
                }
                float _8723 = precise::max(abs(_8614.lo), abs(_8614.hi));
                float _5267 = spvFMul(_8720, _8720);
                float _8724 = interval_down(_5267, intervalFailed);
                float _5268 = spvFMul(_8723, _8723);
                float _8726 = interval_up(_5268, intervalFailed);
                Interval _5269 = Interval{ 2.0, 2.0 };
                Interval _5270 = _8614;
                Interval _8727 = imul(_5269, _5270, intervalFailed, optical_product_upper);
                Interval _5271 = _8727;
                Interval _5272 = _8616;
                Interval _8728 = jet_mul_derivative(_5271, _5272, intervalFailed, optical_product_upper);
                Interval _5273 = Interval{ 2.0, 2.0 };
                Interval _5274 = _8614;
                Interval _8729 = imul(_5273, _5274, intervalFailed, optical_product_upper);
                Interval _5275 = _8729;
                Interval _5276 = _8618;
                Interval _8730 = jet_mul_derivative(_5275, _5276, intervalFailed, optical_product_upper);
                bool _8737;
                if (_8658.lo <= 0.0)
                {
                    _8737 = _8658.hi >= 0.0;
                }
                else
                {
                    _8737 = false;
                }
                float _8744;
                if (_8737)
                {
                    _8744 = 0.0;
                }
                else
                {
                    _8744 = precise::min(abs(_8658.lo), abs(_8658.hi));
                }
                float _8747 = precise::max(abs(_8658.lo), abs(_8658.hi));
                float _5257 = spvFMul(_8744, _8744);
                float _8748 = interval_down(_5257, intervalFailed);
                float _5258 = spvFMul(_8747, _8747);
                float _8750 = interval_up(_5258, intervalFailed);
                Interval _5259 = Interval{ 2.0, 2.0 };
                Interval _5260 = _8658;
                Interval _8751 = imul(_5259, _5260, intervalFailed, optical_product_upper);
                Interval _5261 = _8751;
                Interval _5262 = _8660;
                Interval _8752 = jet_mul_derivative(_5261, _5262, intervalFailed, optical_product_upper);
                Interval _5263 = Interval{ 2.0, 2.0 };
                Interval _5264 = _8658;
                Interval _8753 = imul(_5263, _5264, intervalFailed, optical_product_upper);
                Interval _5265 = _8753;
                Interval _5266 = _8662;
                Interval _8754 = jet_mul_derivative(_5265, _5266, intervalFailed, optical_product_upper);
                Interval _5251 = Interval{ precise::max(0.0, _8724), _8726 };
                Interval _5252 = Interval{ precise::max(0.0, _8748), _8750 };
                Interval _8757 = iadd(_5251, _5252, intervalFailed);
                Interval _5253 = _8728;
                Interval _5254 = _8752;
                Interval _8758 = jet_add_derivative(_5253, _5254, intervalFailed);
                Interval _5255 = _8730;
                Interval _5256 = _8754;
                Interval _8759 = jet_add_derivative(_5255, _5256, intervalFailed);
                bool _8766;
                if (_8702.lo <= 0.0)
                {
                    _8766 = _8702.hi >= 0.0;
                }
                else
                {
                    _8766 = false;
                }
                float _8773;
                if (_8766)
                {
                    _8773 = 0.0;
                }
                else
                {
                    _8773 = precise::min(abs(_8702.lo), abs(_8702.hi));
                }
                float _8776 = precise::max(abs(_8702.lo), abs(_8702.hi));
                float _5241 = spvFMul(_8773, _8773);
                float _8777 = interval_down(_5241, intervalFailed);
                float _5242 = spvFMul(_8776, _8776);
                float _8779 = interval_up(_5242, intervalFailed);
                Interval _5243 = Interval{ 2.0, 2.0 };
                Interval _5244 = _8702;
                Interval _8780 = imul(_5243, _5244, intervalFailed, optical_product_upper);
                Interval _5245 = _8780;
                Interval _5246 = _8704;
                Interval _8781 = jet_mul_derivative(_5245, _5246, intervalFailed, optical_product_upper);
                Interval _5247 = Interval{ 2.0, 2.0 };
                Interval _5248 = _8702;
                Interval _8782 = imul(_5247, _5248, intervalFailed, optical_product_upper);
                Interval _5249 = _8782;
                Interval _5250 = _8706;
                Interval _8783 = jet_mul_derivative(_5249, _5250, intervalFailed, optical_product_upper);
                Interval _5235 = _8757;
                Interval _5236 = Interval{ precise::max(0.0, _8777), _8779 };
                Interval _8785 = iadd(_5235, _5236, intervalFailed);
                Interval _5237 = _8758;
                Interval _5238 = _8781;
                Interval _8786 = jet_add_derivative(_5237, _5238, intervalFailed);
                Interval _5239 = _8759;
                Interval _5240 = _8783;
                Interval _8787 = jet_add_derivative(_5239, _5240, intervalFailed);
                Interval _5226 = _8785;
                Interval _8789 = isqrt(_5226, intervalFailed);
                bool _8797;
                if (!intervalFailed)
                {
                    _8797 = intervalFailed;
                }
                else
                {
                    _8797 = false;
                }
                bool _8802;
                if (_8797)
                {
                    _8802 = jetFailureSite == 0u;
                }
                else
                {
                    _8802 = false;
                }
                if (_8802)
                {
                    jetFailureSite = 3u;
                    jetFailureArguments = float4(_8785.lo, _8785.hi, 0.0, 0.0);
                }
                if (_8789.lo <= 0.0)
                {
                    jetBranchKnown = false;
                }
                Interval _5227 = Interval{ 2.0, 2.0 };
                Interval _5228 = _8789;
                Interval _8809 = imul(_5227, _5228, intervalFailed, optical_product_upper);
                Interval _5229 = Interval{ 1.0, 1.0 };
                Interval _5230 = _8809;
                Interval _8811 = idiv(_5229, _5230, intervalFailed, interval_divide_upper);
                bool _8818;
                if (!intervalFailed)
                {
                    _8818 = intervalFailed;
                }
                else
                {
                    _8818 = false;
                }
                bool _8823;
                if (_8818)
                {
                    _8823 = jetFailureSite == 0u;
                }
                else
                {
                    _8823 = false;
                }
                if (_8823)
                {
                    jetFailureSite = 4u;
                    jetFailureArguments = float4(1.0, 1.0, _8809.lo, _8809.hi);
                }
                Interval _5231 = _8811;
                Interval _5232 = _8786;
                Interval _8827 = jet_mul_derivative(_5231, _5232, intervalFailed, optical_product_upper);
                Interval _5233 = _8811;
                Interval _5234 = _8787;
                Interval _8828 = jet_mul_derivative(_5233, _5234, intervalFailed, optical_product_upper);
                Interval _5212 = Interval{ 1.0, 1.0 };
                Interval _5213 = _8789;
                Interval _8830 = idiv(_5212, _5213, intervalFailed, interval_divide_upper);
                bool _8837;
                if (!intervalFailed)
                {
                    _8837 = intervalFailed;
                }
                else
                {
                    _8837 = false;
                }
                bool _8842;
                if (_8837)
                {
                    _8842 = jetFailureSite == 0u;
                }
                else
                {
                    _8842 = false;
                }
                if (_8842)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(1.0, 1.0, _8789.lo, _8789.hi);
                }
                Interval _5214 = Interval{ 0.0, 0.0 };
                Interval _5215 = _8830;
                Interval _5216 = _8827;
                Interval _8846 = jet_mul_derivative(_5215, _5216, intervalFailed, optical_product_upper);
                Interval _5217 = Interval{ as_type<float>(as_type<uint>(_8846.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8846.lo) ^ 2147483648u) };
                Interval _8856 = jet_add_derivative(_5214, _5217, intervalFailed);
                Interval _5218 = _8856;
                Interval _5219 = _8789;
                Interval _8857 = jet_div_derivative(_5218, _5219, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _5220 = Interval{ 0.0, 0.0 };
                Interval _5221 = _8830;
                Interval _5222 = _8828;
                Interval _8858 = jet_mul_derivative(_5221, _5222, intervalFailed, optical_product_upper);
                Interval _5223 = Interval{ as_type<float>(as_type<uint>(_8858.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8858.lo) ^ 2147483648u) };
                Interval _8868 = jet_add_derivative(_5220, _5223, intervalFailed);
                Interval _5224 = _8868;
                Interval _5225 = _8789;
                Interval _8869 = jet_div_derivative(_5224, _5225, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _5198 = _8614;
                Interval _5199 = _8830;
                Interval _8870 = imul(_5198, _5199, intervalFailed, optical_product_upper);
                Interval _5200 = _8616;
                Interval _5201 = _8830;
                Interval _8871 = jet_mul_derivative(_5200, _5201, intervalFailed, optical_product_upper);
                Interval _5202 = _8871;
                Interval _5203 = _8614;
                Interval _5204 = _8857;
                Interval _8872 = jet_mul_derivative(_5203, _5204, intervalFailed, optical_product_upper);
                Interval _5205 = _8872;
                Interval _8873 = jet_add_derivative(_5202, _5205, intervalFailed);
                Interval _5206 = _8618;
                Interval _5207 = _8830;
                Interval _8874 = jet_mul_derivative(_5206, _5207, intervalFailed, optical_product_upper);
                Interval _5208 = _8874;
                Interval _5209 = _8614;
                Interval _5210 = _8869;
                Interval _8875 = jet_mul_derivative(_5209, _5210, intervalFailed, optical_product_upper);
                Interval _5211 = _8875;
                Interval _8876 = jet_add_derivative(_5208, _5211, intervalFailed);
                Interval _5184 = _8658;
                Interval _5185 = _8830;
                Interval _8877 = imul(_5184, _5185, intervalFailed, optical_product_upper);
                Interval _5186 = _8660;
                Interval _5187 = _8830;
                Interval _8878 = jet_mul_derivative(_5186, _5187, intervalFailed, optical_product_upper);
                Interval _5188 = _8878;
                Interval _5189 = _8658;
                Interval _5190 = _8857;
                Interval _8879 = jet_mul_derivative(_5189, _5190, intervalFailed, optical_product_upper);
                Interval _5191 = _8879;
                Interval _8880 = jet_add_derivative(_5188, _5191, intervalFailed);
                Interval _5192 = _8662;
                Interval _5193 = _8830;
                Interval _8881 = jet_mul_derivative(_5192, _5193, intervalFailed, optical_product_upper);
                Interval _5194 = _8881;
                Interval _5195 = _8658;
                Interval _5196 = _8869;
                Interval _8882 = jet_mul_derivative(_5195, _5196, intervalFailed, optical_product_upper);
                Interval _5197 = _8882;
                Interval _8883 = jet_add_derivative(_5194, _5197, intervalFailed);
                Interval _5170 = _8702;
                Interval _5171 = _8830;
                Interval _8884 = imul(_5170, _5171, intervalFailed, optical_product_upper);
                Interval _5172 = _8704;
                Interval _5173 = _8830;
                Interval _8885 = jet_mul_derivative(_5172, _5173, intervalFailed, optical_product_upper);
                Interval _5174 = _8885;
                Interval _5175 = _8702;
                Interval _5176 = _8857;
                Interval _8886 = jet_mul_derivative(_5175, _5176, intervalFailed, optical_product_upper);
                Interval _5177 = _8886;
                Interval _8887 = jet_add_derivative(_5174, _5177, intervalFailed);
                Interval _5178 = _8706;
                Interval _5179 = _8830;
                Interval _8888 = jet_mul_derivative(_5178, _5179, intervalFailed, optical_product_upper);
                Interval _5180 = _8888;
                Interval _5181 = _8702;
                Interval _5182 = _8869;
                Interval _8889 = jet_mul_derivative(_5181, _5182, intervalFailed, optical_product_upper);
                Interval _5183 = _8889;
                Interval _8890 = jet_add_derivative(_5180, _5183, intervalFailed);
                _8909 = _8870.lo;
                _8910 = _8870.hi;
                _8911 = _8873.lo;
                _8912 = _8873.hi;
                _8913 = _8876.lo;
                _8914 = _8876.hi;
                _8915 = _8877.lo;
                _8916 = _8877.hi;
                _8917 = _8880.lo;
                _8918 = _8880.hi;
                _8919 = _8883.lo;
                _8920 = _8883.hi;
                _8921 = _8884.lo;
                _8922 = _8884.hi;
                _8923 = _8887.lo;
                _8924 = _8887.hi;
                _8925 = _8890.lo;
                _8926 = _8890.hi;
            }
            else
            {
                float _8557;
                float _8558;
                float _8559;
                float _8560;
                float _8561;
                float _8562;
                float _8563;
                float _8564;
                float _8565;
                float _8566;
                float _8567;
                float _8568;
                float _8569;
                float _8570;
                float _8571;
                float _8572;
                float _8573;
                float _8574;
                float _5168 = 0.949999988079071044921875;
                float _8238 = interval_down(_5168, intervalFailed);
                float _5169 = 0.949999988079071044921875;
                float _8239 = interval_up(_5169, intervalFailed);
                if (_8231 > _8239)
                {
                    Interval _5154 = _8177;
                    Interval _5155 = Interval{ 0.0, 0.0 };
                    Interval _8241 = imul(_5154, _5155, intervalFailed, optical_product_upper);
                    Interval _5156 = _8180;
                    Interval _5157 = Interval{ 0.0, 0.0 };
                    Interval _8242 = jet_mul_derivative(_5156, _5157, intervalFailed, optical_product_upper);
                    Interval _5158 = _8242;
                    Interval _5159 = _8177;
                    Interval _5160 = Interval{ 0.0, 0.0 };
                    Interval _8243 = jet_mul_derivative(_5159, _5160, intervalFailed, optical_product_upper);
                    Interval _5161 = _8243;
                    Interval _8244 = jet_add_derivative(_5158, _5161, intervalFailed);
                    Interval _5162 = _8183;
                    Interval _5163 = Interval{ 0.0, 0.0 };
                    Interval _8245 = jet_mul_derivative(_5162, _5163, intervalFailed, optical_product_upper);
                    Interval _5164 = _8245;
                    Interval _5165 = _8177;
                    Interval _5166 = Interval{ 0.0, 0.0 };
                    Interval _8246 = jet_mul_derivative(_5165, _5166, intervalFailed, optical_product_upper);
                    Interval _5167 = _8246;
                    Interval _8247 = jet_add_derivative(_5164, _5167, intervalFailed);
                    Interval _5140 = _8186;
                    Interval _5141 = Interval{ 0.0, 0.0 };
                    Interval _8248 = imul(_5140, _5141, intervalFailed, optical_product_upper);
                    Interval _5142 = _8189;
                    Interval _5143 = Interval{ 0.0, 0.0 };
                    Interval _8249 = jet_mul_derivative(_5142, _5143, intervalFailed, optical_product_upper);
                    Interval _5144 = _8249;
                    Interval _5145 = _8186;
                    Interval _5146 = Interval{ 0.0, 0.0 };
                    Interval _8250 = jet_mul_derivative(_5145, _5146, intervalFailed, optical_product_upper);
                    Interval _5147 = _8250;
                    Interval _8251 = jet_add_derivative(_5144, _5147, intervalFailed);
                    Interval _5148 = _8192;
                    Interval _5149 = Interval{ 0.0, 0.0 };
                    Interval _8252 = jet_mul_derivative(_5148, _5149, intervalFailed, optical_product_upper);
                    Interval _5150 = _8252;
                    Interval _5151 = _8186;
                    Interval _5152 = Interval{ 0.0, 0.0 };
                    Interval _8253 = jet_mul_derivative(_5151, _5152, intervalFailed, optical_product_upper);
                    Interval _5153 = _8253;
                    Interval _8254 = jet_add_derivative(_5150, _5153, intervalFailed);
                    Interval _5134 = _8241;
                    Interval _5135 = Interval{ as_type<float>(as_type<uint>(_8248.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8248.lo) ^ 2147483648u) };
                    Interval _8280 = iadd(_5134, _5135, intervalFailed);
                    Interval _5136 = _8244;
                    Interval _5137 = Interval{ as_type<float>(as_type<uint>(_8251.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8251.lo) ^ 2147483648u) };
                    Interval _8282 = jet_add_derivative(_5136, _5137, intervalFailed);
                    Interval _5138 = _8247;
                    Interval _5139 = Interval{ as_type<float>(as_type<uint>(_8254.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8254.lo) ^ 2147483648u) };
                    Interval _8284 = jet_add_derivative(_5138, _5139, intervalFailed);
                    Interval _5120 = _8186;
                    Interval _5121 = Interval{ 1.0, 1.0 };
                    Interval _8285 = imul(_5120, _5121, intervalFailed, optical_product_upper);
                    Interval _5122 = _8189;
                    Interval _5123 = Interval{ 1.0, 1.0 };
                    Interval _8286 = jet_mul_derivative(_5122, _5123, intervalFailed, optical_product_upper);
                    Interval _5124 = _8286;
                    Interval _5125 = _8186;
                    Interval _5126 = Interval{ 0.0, 0.0 };
                    Interval _8287 = jet_mul_derivative(_5125, _5126, intervalFailed, optical_product_upper);
                    Interval _5127 = _8287;
                    Interval _8288 = jet_add_derivative(_5124, _5127, intervalFailed);
                    Interval _5128 = _8192;
                    Interval _5129 = Interval{ 1.0, 1.0 };
                    Interval _8289 = jet_mul_derivative(_5128, _5129, intervalFailed, optical_product_upper);
                    Interval _5130 = _8289;
                    Interval _5131 = _8186;
                    Interval _5132 = Interval{ 0.0, 0.0 };
                    Interval _8290 = jet_mul_derivative(_5131, _5132, intervalFailed, optical_product_upper);
                    Interval _5133 = _8290;
                    Interval _8291 = jet_add_derivative(_5130, _5133, intervalFailed);
                    Interval _5106 = _8168;
                    Interval _5107 = Interval{ 0.0, 0.0 };
                    Interval _8292 = imul(_5106, _5107, intervalFailed, optical_product_upper);
                    Interval _5108 = _8171;
                    Interval _5109 = Interval{ 0.0, 0.0 };
                    Interval _8293 = jet_mul_derivative(_5108, _5109, intervalFailed, optical_product_upper);
                    Interval _5110 = _8293;
                    Interval _5111 = _8168;
                    Interval _5112 = Interval{ 0.0, 0.0 };
                    Interval _8294 = jet_mul_derivative(_5111, _5112, intervalFailed, optical_product_upper);
                    Interval _5113 = _8294;
                    Interval _8295 = jet_add_derivative(_5110, _5113, intervalFailed);
                    Interval _5114 = _8174;
                    Interval _5115 = Interval{ 0.0, 0.0 };
                    Interval _8296 = jet_mul_derivative(_5114, _5115, intervalFailed, optical_product_upper);
                    Interval _5116 = _8296;
                    Interval _5117 = _8168;
                    Interval _5118 = Interval{ 0.0, 0.0 };
                    Interval _8297 = jet_mul_derivative(_5117, _5118, intervalFailed, optical_product_upper);
                    Interval _5119 = _8297;
                    Interval _8298 = jet_add_derivative(_5116, _5119, intervalFailed);
                    Interval _5100 = _8285;
                    Interval _5101 = Interval{ as_type<float>(as_type<uint>(_8292.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8292.lo) ^ 2147483648u) };
                    Interval _8324 = iadd(_5100, _5101, intervalFailed);
                    Interval _5102 = _8288;
                    Interval _5103 = Interval{ as_type<float>(as_type<uint>(_8295.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8295.lo) ^ 2147483648u) };
                    Interval _8326 = jet_add_derivative(_5102, _5103, intervalFailed);
                    Interval _5104 = _8291;
                    Interval _5105 = Interval{ as_type<float>(as_type<uint>(_8298.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8298.lo) ^ 2147483648u) };
                    Interval _8328 = jet_add_derivative(_5104, _5105, intervalFailed);
                    Interval _5086 = _8168;
                    Interval _5087 = Interval{ 0.0, 0.0 };
                    Interval _8329 = imul(_5086, _5087, intervalFailed, optical_product_upper);
                    Interval _5088 = _8171;
                    Interval _5089 = Interval{ 0.0, 0.0 };
                    Interval _8330 = jet_mul_derivative(_5088, _5089, intervalFailed, optical_product_upper);
                    Interval _5090 = _8330;
                    Interval _5091 = _8168;
                    Interval _5092 = Interval{ 0.0, 0.0 };
                    Interval _8331 = jet_mul_derivative(_5091, _5092, intervalFailed, optical_product_upper);
                    Interval _5093 = _8331;
                    Interval _8332 = jet_add_derivative(_5090, _5093, intervalFailed);
                    Interval _5094 = _8174;
                    Interval _5095 = Interval{ 0.0, 0.0 };
                    Interval _8333 = jet_mul_derivative(_5094, _5095, intervalFailed, optical_product_upper);
                    Interval _5096 = _8333;
                    Interval _5097 = _8168;
                    Interval _5098 = Interval{ 0.0, 0.0 };
                    Interval _8334 = jet_mul_derivative(_5097, _5098, intervalFailed, optical_product_upper);
                    Interval _5099 = _8334;
                    Interval _8335 = jet_add_derivative(_5096, _5099, intervalFailed);
                    Interval _5072 = _8177;
                    Interval _5073 = Interval{ 1.0, 1.0 };
                    Interval _8336 = imul(_5072, _5073, intervalFailed, optical_product_upper);
                    Interval _5074 = _8180;
                    Interval _5075 = Interval{ 1.0, 1.0 };
                    Interval _8337 = jet_mul_derivative(_5074, _5075, intervalFailed, optical_product_upper);
                    Interval _5076 = _8337;
                    Interval _5077 = _8177;
                    Interval _5078 = Interval{ 0.0, 0.0 };
                    Interval _8338 = jet_mul_derivative(_5077, _5078, intervalFailed, optical_product_upper);
                    Interval _5079 = _8338;
                    Interval _8339 = jet_add_derivative(_5076, _5079, intervalFailed);
                    Interval _5080 = _8183;
                    Interval _5081 = Interval{ 1.0, 1.0 };
                    Interval _8340 = jet_mul_derivative(_5080, _5081, intervalFailed, optical_product_upper);
                    Interval _5082 = _8340;
                    Interval _5083 = _8177;
                    Interval _5084 = Interval{ 0.0, 0.0 };
                    Interval _8341 = jet_mul_derivative(_5083, _5084, intervalFailed, optical_product_upper);
                    Interval _5085 = _8341;
                    Interval _8342 = jet_add_derivative(_5082, _5085, intervalFailed);
                    Interval _5066 = _8329;
                    Interval _5067 = Interval{ as_type<float>(as_type<uint>(_8336.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8336.lo) ^ 2147483648u) };
                    Interval _8368 = iadd(_5066, _5067, intervalFailed);
                    Interval _5068 = _8332;
                    Interval _5069 = Interval{ as_type<float>(as_type<uint>(_8339.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8339.lo) ^ 2147483648u) };
                    Interval _8370 = jet_add_derivative(_5068, _5069, intervalFailed);
                    Interval _5070 = _8335;
                    Interval _5071 = Interval{ as_type<float>(as_type<uint>(_8342.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8342.lo) ^ 2147483648u) };
                    Interval _8372 = jet_add_derivative(_5070, _5071, intervalFailed);
                    bool _8379;
                    if (_8280.lo <= 0.0)
                    {
                        _8379 = _8280.hi >= 0.0;
                    }
                    else
                    {
                        _8379 = false;
                    }
                    float _8386;
                    if (_8379)
                    {
                        _8386 = 0.0;
                    }
                    else
                    {
                        _8386 = precise::min(abs(_8280.lo), abs(_8280.hi));
                    }
                    float _8389 = precise::max(abs(_8280.lo), abs(_8280.hi));
                    float _5056 = spvFMul(_8386, _8386);
                    float _8390 = interval_down(_5056, intervalFailed);
                    float _5057 = spvFMul(_8389, _8389);
                    float _8392 = interval_up(_5057, intervalFailed);
                    Interval _5058 = Interval{ 2.0, 2.0 };
                    Interval _5059 = _8280;
                    Interval _8393 = imul(_5058, _5059, intervalFailed, optical_product_upper);
                    Interval _5060 = _8393;
                    Interval _5061 = _8282;
                    Interval _8394 = jet_mul_derivative(_5060, _5061, intervalFailed, optical_product_upper);
                    Interval _5062 = Interval{ 2.0, 2.0 };
                    Interval _5063 = _8280;
                    Interval _8395 = imul(_5062, _5063, intervalFailed, optical_product_upper);
                    Interval _5064 = _8395;
                    Interval _5065 = _8284;
                    Interval _8396 = jet_mul_derivative(_5064, _5065, intervalFailed, optical_product_upper);
                    bool _8403;
                    if (_8324.lo <= 0.0)
                    {
                        _8403 = _8324.hi >= 0.0;
                    }
                    else
                    {
                        _8403 = false;
                    }
                    float _8410;
                    if (_8403)
                    {
                        _8410 = 0.0;
                    }
                    else
                    {
                        _8410 = precise::min(abs(_8324.lo), abs(_8324.hi));
                    }
                    float _8413 = precise::max(abs(_8324.lo), abs(_8324.hi));
                    float _5046 = spvFMul(_8410, _8410);
                    float _8414 = interval_down(_5046, intervalFailed);
                    float _5047 = spvFMul(_8413, _8413);
                    float _8416 = interval_up(_5047, intervalFailed);
                    Interval _5048 = Interval{ 2.0, 2.0 };
                    Interval _5049 = _8324;
                    Interval _8417 = imul(_5048, _5049, intervalFailed, optical_product_upper);
                    Interval _5050 = _8417;
                    Interval _5051 = _8326;
                    Interval _8418 = jet_mul_derivative(_5050, _5051, intervalFailed, optical_product_upper);
                    Interval _5052 = Interval{ 2.0, 2.0 };
                    Interval _5053 = _8324;
                    Interval _8419 = imul(_5052, _5053, intervalFailed, optical_product_upper);
                    Interval _5054 = _8419;
                    Interval _5055 = _8328;
                    Interval _8420 = jet_mul_derivative(_5054, _5055, intervalFailed, optical_product_upper);
                    Interval _5040 = Interval{ precise::max(0.0, _8390), _8392 };
                    Interval _5041 = Interval{ precise::max(0.0, _8414), _8416 };
                    Interval _8423 = iadd(_5040, _5041, intervalFailed);
                    Interval _5042 = _8394;
                    Interval _5043 = _8418;
                    Interval _8424 = jet_add_derivative(_5042, _5043, intervalFailed);
                    Interval _5044 = _8396;
                    Interval _5045 = _8420;
                    Interval _8425 = jet_add_derivative(_5044, _5045, intervalFailed);
                    bool _8432;
                    if (_8368.lo <= 0.0)
                    {
                        _8432 = _8368.hi >= 0.0;
                    }
                    else
                    {
                        _8432 = false;
                    }
                    float _8439;
                    if (_8432)
                    {
                        _8439 = 0.0;
                    }
                    else
                    {
                        _8439 = precise::min(abs(_8368.lo), abs(_8368.hi));
                    }
                    float _8442 = precise::max(abs(_8368.lo), abs(_8368.hi));
                    float _5030 = spvFMul(_8439, _8439);
                    float _8443 = interval_down(_5030, intervalFailed);
                    float _5031 = spvFMul(_8442, _8442);
                    float _8445 = interval_up(_5031, intervalFailed);
                    Interval _5032 = Interval{ 2.0, 2.0 };
                    Interval _5033 = _8368;
                    Interval _8446 = imul(_5032, _5033, intervalFailed, optical_product_upper);
                    Interval _5034 = _8446;
                    Interval _5035 = _8370;
                    Interval _8447 = jet_mul_derivative(_5034, _5035, intervalFailed, optical_product_upper);
                    Interval _5036 = Interval{ 2.0, 2.0 };
                    Interval _5037 = _8368;
                    Interval _8448 = imul(_5036, _5037, intervalFailed, optical_product_upper);
                    Interval _5038 = _8448;
                    Interval _5039 = _8372;
                    Interval _8449 = jet_mul_derivative(_5038, _5039, intervalFailed, optical_product_upper);
                    Interval _5024 = _8423;
                    Interval _5025 = Interval{ precise::max(0.0, _8443), _8445 };
                    Interval _8451 = iadd(_5024, _5025, intervalFailed);
                    Interval _5026 = _8424;
                    Interval _5027 = _8447;
                    Interval _8452 = jet_add_derivative(_5026, _5027, intervalFailed);
                    Interval _5028 = _8425;
                    Interval _5029 = _8449;
                    Interval _8453 = jet_add_derivative(_5028, _5029, intervalFailed);
                    Interval _5015 = _8451;
                    Interval _8455 = isqrt(_5015, intervalFailed);
                    bool _8463;
                    if (!intervalFailed)
                    {
                        _8463 = intervalFailed;
                    }
                    else
                    {
                        _8463 = false;
                    }
                    bool _8468;
                    if (_8463)
                    {
                        _8468 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8468 = false;
                    }
                    if (_8468)
                    {
                        jetFailureSite = 3u;
                        jetFailureArguments = float4(_8451.lo, _8451.hi, 0.0, 0.0);
                    }
                    if (_8455.lo <= 0.0)
                    {
                        jetBranchKnown = false;
                    }
                    Interval _5016 = Interval{ 2.0, 2.0 };
                    Interval _5017 = _8455;
                    Interval _8475 = imul(_5016, _5017, intervalFailed, optical_product_upper);
                    Interval _5018 = Interval{ 1.0, 1.0 };
                    Interval _5019 = _8475;
                    Interval _8477 = idiv(_5018, _5019, intervalFailed, interval_divide_upper);
                    bool _8484;
                    if (!intervalFailed)
                    {
                        _8484 = intervalFailed;
                    }
                    else
                    {
                        _8484 = false;
                    }
                    bool _8489;
                    if (_8484)
                    {
                        _8489 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8489 = false;
                    }
                    if (_8489)
                    {
                        jetFailureSite = 4u;
                        jetFailureArguments = float4(1.0, 1.0, _8475.lo, _8475.hi);
                    }
                    Interval _5020 = _8477;
                    Interval _5021 = _8452;
                    Interval _8493 = jet_mul_derivative(_5020, _5021, intervalFailed, optical_product_upper);
                    Interval _5022 = _8477;
                    Interval _5023 = _8453;
                    Interval _8494 = jet_mul_derivative(_5022, _5023, intervalFailed, optical_product_upper);
                    Interval _5001 = Interval{ 1.0, 1.0 };
                    Interval _5002 = _8455;
                    Interval _8496 = idiv(_5001, _5002, intervalFailed, interval_divide_upper);
                    bool _8503;
                    if (!intervalFailed)
                    {
                        _8503 = intervalFailed;
                    }
                    else
                    {
                        _8503 = false;
                    }
                    bool _8508;
                    if (_8503)
                    {
                        _8508 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8508 = false;
                    }
                    if (_8508)
                    {
                        jetFailureSite = 1u;
                        jetFailureArguments = float4(1.0, 1.0, _8455.lo, _8455.hi);
                    }
                    Interval _5003 = Interval{ 0.0, 0.0 };
                    Interval _5004 = _8496;
                    Interval _5005 = _8493;
                    Interval _8512 = jet_mul_derivative(_5004, _5005, intervalFailed, optical_product_upper);
                    Interval _5006 = Interval{ as_type<float>(as_type<uint>(_8512.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8512.lo) ^ 2147483648u) };
                    Interval _8522 = jet_add_derivative(_5003, _5006, intervalFailed);
                    Interval _5007 = _8522;
                    Interval _5008 = _8455;
                    Interval _8523 = jet_div_derivative(_5007, _5008, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                    Interval _5009 = Interval{ 0.0, 0.0 };
                    Interval _5010 = _8496;
                    Interval _5011 = _8494;
                    Interval _8524 = jet_mul_derivative(_5010, _5011, intervalFailed, optical_product_upper);
                    Interval _5012 = Interval{ as_type<float>(as_type<uint>(_8524.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8524.lo) ^ 2147483648u) };
                    Interval _8534 = jet_add_derivative(_5009, _5012, intervalFailed);
                    Interval _5013 = _8534;
                    Interval _5014 = _8455;
                    Interval _8535 = jet_div_derivative(_5013, _5014, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                    Interval _4987 = _8280;
                    Interval _4988 = _8496;
                    Interval _8536 = imul(_4987, _4988, intervalFailed, optical_product_upper);
                    Interval _4989 = _8282;
                    Interval _4990 = _8496;
                    Interval _8537 = jet_mul_derivative(_4989, _4990, intervalFailed, optical_product_upper);
                    Interval _4991 = _8537;
                    Interval _4992 = _8280;
                    Interval _4993 = _8523;
                    Interval _8538 = jet_mul_derivative(_4992, _4993, intervalFailed, optical_product_upper);
                    Interval _4994 = _8538;
                    Interval _8539 = jet_add_derivative(_4991, _4994, intervalFailed);
                    Interval _4995 = _8284;
                    Interval _4996 = _8496;
                    Interval _8540 = jet_mul_derivative(_4995, _4996, intervalFailed, optical_product_upper);
                    Interval _4997 = _8540;
                    Interval _4998 = _8280;
                    Interval _4999 = _8535;
                    Interval _8541 = jet_mul_derivative(_4998, _4999, intervalFailed, optical_product_upper);
                    Interval _5000 = _8541;
                    Interval _8542 = jet_add_derivative(_4997, _5000, intervalFailed);
                    Interval _4973 = _8324;
                    Interval _4974 = _8496;
                    Interval _8543 = imul(_4973, _4974, intervalFailed, optical_product_upper);
                    Interval _4975 = _8326;
                    Interval _4976 = _8496;
                    Interval _8544 = jet_mul_derivative(_4975, _4976, intervalFailed, optical_product_upper);
                    Interval _4977 = _8544;
                    Interval _4978 = _8324;
                    Interval _4979 = _8523;
                    Interval _8545 = jet_mul_derivative(_4978, _4979, intervalFailed, optical_product_upper);
                    Interval _4980 = _8545;
                    Interval _8546 = jet_add_derivative(_4977, _4980, intervalFailed);
                    Interval _4981 = _8328;
                    Interval _4982 = _8496;
                    Interval _8547 = jet_mul_derivative(_4981, _4982, intervalFailed, optical_product_upper);
                    Interval _4983 = _8547;
                    Interval _4984 = _8324;
                    Interval _4985 = _8535;
                    Interval _8548 = jet_mul_derivative(_4984, _4985, intervalFailed, optical_product_upper);
                    Interval _4986 = _8548;
                    Interval _8549 = jet_add_derivative(_4983, _4986, intervalFailed);
                    Interval _4959 = _8368;
                    Interval _4960 = _8496;
                    Interval _8550 = imul(_4959, _4960, intervalFailed, optical_product_upper);
                    Interval _4961 = _8370;
                    Interval _4962 = _8496;
                    Interval _8551 = jet_mul_derivative(_4961, _4962, intervalFailed, optical_product_upper);
                    Interval _4963 = _8551;
                    Interval _4964 = _8368;
                    Interval _4965 = _8523;
                    Interval _8552 = jet_mul_derivative(_4964, _4965, intervalFailed, optical_product_upper);
                    Interval _4966 = _8552;
                    Interval _8553 = jet_add_derivative(_4963, _4966, intervalFailed);
                    Interval _4967 = _8372;
                    Interval _4968 = _8496;
                    Interval _8554 = jet_mul_derivative(_4967, _4968, intervalFailed, optical_product_upper);
                    Interval _4969 = _8554;
                    Interval _4970 = _8368;
                    Interval _4971 = _8535;
                    Interval _8555 = jet_mul_derivative(_4970, _4971, intervalFailed, optical_product_upper);
                    Interval _4972 = _8555;
                    Interval _8556 = jet_add_derivative(_4969, _4972, intervalFailed);
                    _8557 = _8536.lo;
                    _8558 = _8536.hi;
                    _8559 = _8539.lo;
                    _8560 = _8539.hi;
                    _8561 = _8542.lo;
                    _8562 = _8542.hi;
                    _8563 = _8543.lo;
                    _8564 = _8543.hi;
                    _8565 = _8546.lo;
                    _8566 = _8546.hi;
                    _8567 = _8549.lo;
                    _8568 = _8549.hi;
                    _8569 = _8550.lo;
                    _8570 = _8550.hi;
                    _8571 = _8553.lo;
                    _8572 = _8553.hi;
                    _8573 = _8556.lo;
                    _8574 = _8556.hi;
                }
                else
                {
                    return false;
                }
                _8909 = _8557;
                _8910 = _8558;
                _8911 = _8559;
                _8912 = _8560;
                _8913 = _8561;
                _8914 = _8562;
                _8915 = _8563;
                _8916 = _8564;
                _8917 = _8565;
                _8918 = _8566;
                _8919 = _8567;
                _8920 = _8568;
                _8921 = _8569;
                _8922 = _8570;
                _8923 = _8571;
                _8924 = _8572;
                _8925 = _8573;
                _8926 = _8574;
            }
            uint _8930 = uint(receiver.settings.y);
            float _8934 = float(_2236[_8930].x);
            float _4957 = _8934;
            float _4958 = 1000.0;
            Interval _8936 = iratio(_4957, _4958, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _8941;
            if (!intervalFailed)
            {
                _8941 = intervalFailed;
            }
            else
            {
                _8941 = false;
            }
            bool _8946;
            if (_8941)
            {
                _8946 = jetFailureSite == 0u;
            }
            else
            {
                _8946 = false;
            }
            if (_8946)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(_8934, _8934, 1000.0, 1000.0);
            }
            Interval _4943 = Interval{ _8909, _8910 };
            Interval _4944 = _8936;
            Interval _8951 = imul(_4943, _4944, intervalFailed, optical_product_upper);
            Interval _4945 = Interval{ _8911, _8912 };
            Interval _4946 = _8936;
            Interval _8953 = jet_mul_derivative(_4945, _4946, intervalFailed, optical_product_upper);
            Interval _4947 = _8953;
            Interval _4948 = Interval{ _8909, _8910 };
            Interval _4949 = Interval{ 0.0, 0.0 };
            Interval _8955 = jet_mul_derivative(_4948, _4949, intervalFailed, optical_product_upper);
            Interval _4950 = _8955;
            Interval _8956 = jet_add_derivative(_4947, _4950, intervalFailed);
            Interval _4951 = Interval{ _8913, _8914 };
            Interval _4952 = _8936;
            Interval _8958 = jet_mul_derivative(_4951, _4952, intervalFailed, optical_product_upper);
            Interval _4953 = _8958;
            Interval _4954 = Interval{ _8909, _8910 };
            Interval _4955 = Interval{ 0.0, 0.0 };
            Interval _8960 = jet_mul_derivative(_4954, _4955, intervalFailed, optical_product_upper);
            Interval _4956 = _8960;
            Interval _8961 = jet_add_derivative(_4953, _4956, intervalFailed);
            Interval _4929 = Interval{ _8915, _8916 };
            Interval _4930 = _8936;
            Interval _8963 = imul(_4929, _4930, intervalFailed, optical_product_upper);
            Interval _4931 = Interval{ _8917, _8918 };
            Interval _4932 = _8936;
            Interval _8965 = jet_mul_derivative(_4931, _4932, intervalFailed, optical_product_upper);
            Interval _4933 = _8965;
            Interval _4934 = Interval{ _8915, _8916 };
            Interval _4935 = Interval{ 0.0, 0.0 };
            Interval _8967 = jet_mul_derivative(_4934, _4935, intervalFailed, optical_product_upper);
            Interval _4936 = _8967;
            Interval _8968 = jet_add_derivative(_4933, _4936, intervalFailed);
            Interval _4937 = Interval{ _8919, _8920 };
            Interval _4938 = _8936;
            Interval _8970 = jet_mul_derivative(_4937, _4938, intervalFailed, optical_product_upper);
            Interval _4939 = _8970;
            Interval _4940 = Interval{ _8915, _8916 };
            Interval _4941 = Interval{ 0.0, 0.0 };
            Interval _8972 = jet_mul_derivative(_4940, _4941, intervalFailed, optical_product_upper);
            Interval _4942 = _8972;
            Interval _8973 = jet_add_derivative(_4939, _4942, intervalFailed);
            Interval _4915 = Interval{ _8921, _8922 };
            Interval _4916 = _8936;
            Interval _8975 = imul(_4915, _4916, intervalFailed, optical_product_upper);
            Interval _4917 = Interval{ _8923, _8924 };
            Interval _4918 = _8936;
            Interval _8977 = jet_mul_derivative(_4917, _4918, intervalFailed, optical_product_upper);
            Interval _4919 = _8977;
            Interval _4920 = Interval{ _8921, _8922 };
            Interval _4921 = Interval{ 0.0, 0.0 };
            Interval _8979 = jet_mul_derivative(_4920, _4921, intervalFailed, optical_product_upper);
            Interval _4922 = _8979;
            Interval _8980 = jet_add_derivative(_4919, _4922, intervalFailed);
            Interval _4923 = Interval{ _8925, _8926 };
            Interval _4924 = _8936;
            Interval _8982 = jet_mul_derivative(_4923, _4924, intervalFailed, optical_product_upper);
            Interval _4925 = _8982;
            Interval _4926 = Interval{ _8921, _8922 };
            Interval _4927 = Interval{ 0.0, 0.0 };
            Interval _8984 = jet_mul_derivative(_4926, _4927, intervalFailed, optical_product_upper);
            Interval _4928 = _8984;
            Interval _8985 = jet_add_derivative(_4925, _4928, intervalFailed);
            Interval _4901 = _8177;
            Interval _4902 = Interval{ _8921, _8922 };
            Interval _8987 = imul(_4901, _4902, intervalFailed, optical_product_upper);
            Interval _4903 = _8180;
            Interval _4904 = Interval{ _8921, _8922 };
            Interval _8989 = jet_mul_derivative(_4903, _4904, intervalFailed, optical_product_upper);
            Interval _4905 = _8989;
            Interval _4906 = _8177;
            Interval _4907 = Interval{ _8923, _8924 };
            Interval _8991 = jet_mul_derivative(_4906, _4907, intervalFailed, optical_product_upper);
            Interval _4908 = _8991;
            Interval _8992 = jet_add_derivative(_4905, _4908, intervalFailed);
            Interval _4909 = _8183;
            Interval _4910 = Interval{ _8921, _8922 };
            Interval _8994 = jet_mul_derivative(_4909, _4910, intervalFailed, optical_product_upper);
            Interval _4911 = _8994;
            Interval _4912 = _8177;
            Interval _4913 = Interval{ _8925, _8926 };
            Interval _8996 = jet_mul_derivative(_4912, _4913, intervalFailed, optical_product_upper);
            Interval _4914 = _8996;
            Interval _8997 = jet_add_derivative(_4911, _4914, intervalFailed);
            Interval _4887 = _8186;
            Interval _4888 = Interval{ _8915, _8916 };
            Interval _8999 = imul(_4887, _4888, intervalFailed, optical_product_upper);
            Interval _4889 = _8189;
            Interval _4890 = Interval{ _8915, _8916 };
            Interval _9001 = jet_mul_derivative(_4889, _4890, intervalFailed, optical_product_upper);
            Interval _4891 = _9001;
            Interval _4892 = _8186;
            Interval _4893 = Interval{ _8917, _8918 };
            Interval _9003 = jet_mul_derivative(_4892, _4893, intervalFailed, optical_product_upper);
            Interval _4894 = _9003;
            Interval _9004 = jet_add_derivative(_4891, _4894, intervalFailed);
            Interval _4895 = _8192;
            Interval _4896 = Interval{ _8915, _8916 };
            Interval _9006 = jet_mul_derivative(_4895, _4896, intervalFailed, optical_product_upper);
            Interval _4897 = _9006;
            Interval _4898 = _8186;
            Interval _4899 = Interval{ _8919, _8920 };
            Interval _9008 = jet_mul_derivative(_4898, _4899, intervalFailed, optical_product_upper);
            Interval _4900 = _9008;
            Interval _9009 = jet_add_derivative(_4897, _4900, intervalFailed);
            Interval _4881 = _8987;
            Interval _4882 = Interval{ as_type<float>(as_type<uint>(_8999.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8999.lo) ^ 2147483648u) };
            Interval _9035 = iadd(_4881, _4882, intervalFailed);
            Interval _4883 = _8992;
            Interval _4884 = Interval{ as_type<float>(as_type<uint>(_9004.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9004.lo) ^ 2147483648u) };
            Interval _9037 = jet_add_derivative(_4883, _4884, intervalFailed);
            Interval _4885 = _8997;
            Interval _4886 = Interval{ as_type<float>(as_type<uint>(_9009.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9009.lo) ^ 2147483648u) };
            Interval _9039 = jet_add_derivative(_4885, _4886, intervalFailed);
            Interval _4867 = _8186;
            Interval _4868 = Interval{ _8909, _8910 };
            Interval _9041 = imul(_4867, _4868, intervalFailed, optical_product_upper);
            Interval _4869 = _8189;
            Interval _4870 = Interval{ _8909, _8910 };
            Interval _9043 = jet_mul_derivative(_4869, _4870, intervalFailed, optical_product_upper);
            Interval _4871 = _9043;
            Interval _4872 = _8186;
            Interval _4873 = Interval{ _8911, _8912 };
            Interval _9045 = jet_mul_derivative(_4872, _4873, intervalFailed, optical_product_upper);
            Interval _4874 = _9045;
            Interval _9046 = jet_add_derivative(_4871, _4874, intervalFailed);
            Interval _4875 = _8192;
            Interval _4876 = Interval{ _8909, _8910 };
            Interval _9048 = jet_mul_derivative(_4875, _4876, intervalFailed, optical_product_upper);
            Interval _4877 = _9048;
            Interval _4878 = _8186;
            Interval _4879 = Interval{ _8913, _8914 };
            Interval _9050 = jet_mul_derivative(_4878, _4879, intervalFailed, optical_product_upper);
            Interval _4880 = _9050;
            Interval _9051 = jet_add_derivative(_4877, _4880, intervalFailed);
            Interval _4853 = _8168;
            Interval _4854 = Interval{ _8921, _8922 };
            Interval _9053 = imul(_4853, _4854, intervalFailed, optical_product_upper);
            Interval _4855 = _8171;
            Interval _4856 = Interval{ _8921, _8922 };
            Interval _9055 = jet_mul_derivative(_4855, _4856, intervalFailed, optical_product_upper);
            Interval _4857 = _9055;
            Interval _4858 = _8168;
            Interval _4859 = Interval{ _8923, _8924 };
            Interval _9057 = jet_mul_derivative(_4858, _4859, intervalFailed, optical_product_upper);
            Interval _4860 = _9057;
            Interval _9058 = jet_add_derivative(_4857, _4860, intervalFailed);
            Interval _4861 = _8174;
            Interval _4862 = Interval{ _8921, _8922 };
            Interval _9060 = jet_mul_derivative(_4861, _4862, intervalFailed, optical_product_upper);
            Interval _4863 = _9060;
            Interval _4864 = _8168;
            Interval _4865 = Interval{ _8925, _8926 };
            Interval _9062 = jet_mul_derivative(_4864, _4865, intervalFailed, optical_product_upper);
            Interval _4866 = _9062;
            Interval _9063 = jet_add_derivative(_4863, _4866, intervalFailed);
            Interval _4847 = _9041;
            Interval _4848 = Interval{ as_type<float>(as_type<uint>(_9053.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9053.lo) ^ 2147483648u) };
            Interval _9089 = iadd(_4847, _4848, intervalFailed);
            Interval _4849 = _9046;
            Interval _4850 = Interval{ as_type<float>(as_type<uint>(_9058.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9058.lo) ^ 2147483648u) };
            Interval _9091 = jet_add_derivative(_4849, _4850, intervalFailed);
            Interval _4851 = _9051;
            Interval _4852 = Interval{ as_type<float>(as_type<uint>(_9063.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9063.lo) ^ 2147483648u) };
            Interval _9093 = jet_add_derivative(_4851, _4852, intervalFailed);
            Interval _4833 = _8168;
            Interval _4834 = Interval{ _8915, _8916 };
            Interval _9095 = imul(_4833, _4834, intervalFailed, optical_product_upper);
            Interval _4835 = _8171;
            Interval _4836 = Interval{ _8915, _8916 };
            Interval _9097 = jet_mul_derivative(_4835, _4836, intervalFailed, optical_product_upper);
            Interval _4837 = _9097;
            Interval _4838 = _8168;
            Interval _4839 = Interval{ _8917, _8918 };
            Interval _9099 = jet_mul_derivative(_4838, _4839, intervalFailed, optical_product_upper);
            Interval _4840 = _9099;
            Interval _9100 = jet_add_derivative(_4837, _4840, intervalFailed);
            Interval _4841 = _8174;
            Interval _4842 = Interval{ _8915, _8916 };
            Interval _9102 = jet_mul_derivative(_4841, _4842, intervalFailed, optical_product_upper);
            Interval _4843 = _9102;
            Interval _4844 = _8168;
            Interval _4845 = Interval{ _8919, _8920 };
            Interval _9104 = jet_mul_derivative(_4844, _4845, intervalFailed, optical_product_upper);
            Interval _4846 = _9104;
            Interval _9105 = jet_add_derivative(_4843, _4846, intervalFailed);
            Interval _4819 = _8177;
            Interval _4820 = Interval{ _8909, _8910 };
            Interval _9107 = imul(_4819, _4820, intervalFailed, optical_product_upper);
            Interval _4821 = _8180;
            Interval _4822 = Interval{ _8909, _8910 };
            Interval _9109 = jet_mul_derivative(_4821, _4822, intervalFailed, optical_product_upper);
            Interval _4823 = _9109;
            Interval _4824 = _8177;
            Interval _4825 = Interval{ _8911, _8912 };
            Interval _9111 = jet_mul_derivative(_4824, _4825, intervalFailed, optical_product_upper);
            Interval _4826 = _9111;
            Interval _9112 = jet_add_derivative(_4823, _4826, intervalFailed);
            Interval _4827 = _8183;
            Interval _4828 = Interval{ _8909, _8910 };
            Interval _9114 = jet_mul_derivative(_4827, _4828, intervalFailed, optical_product_upper);
            Interval _4829 = _9114;
            Interval _4830 = _8177;
            Interval _4831 = Interval{ _8913, _8914 };
            Interval _9116 = jet_mul_derivative(_4830, _4831, intervalFailed, optical_product_upper);
            Interval _4832 = _9116;
            Interval _9117 = jet_add_derivative(_4829, _4832, intervalFailed);
            Interval _4813 = _9095;
            Interval _4814 = Interval{ as_type<float>(as_type<uint>(_9107.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9107.lo) ^ 2147483648u) };
            Interval _9143 = iadd(_4813, _4814, intervalFailed);
            Interval _4815 = _9100;
            Interval _4816 = Interval{ as_type<float>(as_type<uint>(_9112.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9112.lo) ^ 2147483648u) };
            Interval _9145 = jet_add_derivative(_4815, _4816, intervalFailed);
            Interval _4817 = _9105;
            Interval _4818 = Interval{ as_type<float>(as_type<uint>(_9117.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9117.lo) ^ 2147483648u) };
            Interval _9147 = jet_add_derivative(_4817, _4818, intervalFailed);
            float _9149 = float(_2236[_8930].y);
            float _4811 = _9149;
            float _4812 = 1000.0;
            Interval _9151 = iratio(_4811, _4812, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _9156;
            if (!intervalFailed)
            {
                _9156 = intervalFailed;
            }
            else
            {
                _9156 = false;
            }
            bool _9161;
            if (_9156)
            {
                _9161 = jetFailureSite == 0u;
            }
            else
            {
                _9161 = false;
            }
            if (_9161)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(_9149, _9149, 1000.0, 1000.0);
            }
            Interval _4797 = _9035;
            Interval _4798 = _9151;
            Interval _9165 = imul(_4797, _4798, intervalFailed, optical_product_upper);
            Interval _4799 = _9037;
            Interval _4800 = _9151;
            Interval _9166 = jet_mul_derivative(_4799, _4800, intervalFailed, optical_product_upper);
            Interval _4801 = _9166;
            Interval _4802 = _9035;
            Interval _4803 = Interval{ 0.0, 0.0 };
            Interval _9167 = jet_mul_derivative(_4802, _4803, intervalFailed, optical_product_upper);
            Interval _4804 = _9167;
            Interval _9168 = jet_add_derivative(_4801, _4804, intervalFailed);
            Interval _4805 = _9039;
            Interval _4806 = _9151;
            Interval _9169 = jet_mul_derivative(_4805, _4806, intervalFailed, optical_product_upper);
            Interval _4807 = _9169;
            Interval _4808 = _9035;
            Interval _4809 = Interval{ 0.0, 0.0 };
            Interval _9170 = jet_mul_derivative(_4808, _4809, intervalFailed, optical_product_upper);
            Interval _4810 = _9170;
            Interval _9171 = jet_add_derivative(_4807, _4810, intervalFailed);
            Interval _4783 = _9089;
            Interval _4784 = _9151;
            Interval _9172 = imul(_4783, _4784, intervalFailed, optical_product_upper);
            Interval _4785 = _9091;
            Interval _4786 = _9151;
            Interval _9173 = jet_mul_derivative(_4785, _4786, intervalFailed, optical_product_upper);
            Interval _4787 = _9173;
            Interval _4788 = _9089;
            Interval _4789 = Interval{ 0.0, 0.0 };
            Interval _9174 = jet_mul_derivative(_4788, _4789, intervalFailed, optical_product_upper);
            Interval _4790 = _9174;
            Interval _9175 = jet_add_derivative(_4787, _4790, intervalFailed);
            Interval _4791 = _9093;
            Interval _4792 = _9151;
            Interval _9176 = jet_mul_derivative(_4791, _4792, intervalFailed, optical_product_upper);
            Interval _4793 = _9176;
            Interval _4794 = _9089;
            Interval _4795 = Interval{ 0.0, 0.0 };
            Interval _9177 = jet_mul_derivative(_4794, _4795, intervalFailed, optical_product_upper);
            Interval _4796 = _9177;
            Interval _9178 = jet_add_derivative(_4793, _4796, intervalFailed);
            Interval _4769 = _9143;
            Interval _4770 = _9151;
            Interval _9179 = imul(_4769, _4770, intervalFailed, optical_product_upper);
            Interval _4771 = _9145;
            Interval _4772 = _9151;
            Interval _9180 = jet_mul_derivative(_4771, _4772, intervalFailed, optical_product_upper);
            Interval _4773 = _9180;
            Interval _4774 = _9143;
            Interval _4775 = Interval{ 0.0, 0.0 };
            Interval _9181 = jet_mul_derivative(_4774, _4775, intervalFailed, optical_product_upper);
            Interval _4776 = _9181;
            Interval _9182 = jet_add_derivative(_4773, _4776, intervalFailed);
            Interval _4777 = _9147;
            Interval _4778 = _9151;
            Interval _9183 = jet_mul_derivative(_4777, _4778, intervalFailed, optical_product_upper);
            Interval _4779 = _9183;
            Interval _4780 = _9143;
            Interval _4781 = Interval{ 0.0, 0.0 };
            Interval _9184 = jet_mul_derivative(_4780, _4781, intervalFailed, optical_product_upper);
            Interval _4782 = _9184;
            Interval _9185 = jet_add_derivative(_4779, _4782, intervalFailed);
            Interval _4763 = _8951;
            Interval _4764 = _9165;
            Interval _9186 = iadd(_4763, _4764, intervalFailed);
            Interval _4765 = _8956;
            Interval _4766 = _9168;
            Interval _9187 = jet_add_derivative(_4765, _4766, intervalFailed);
            Interval _4767 = _8961;
            Interval _4768 = _9171;
            Interval _9188 = jet_add_derivative(_4767, _4768, intervalFailed);
            Interval _4757 = _8963;
            Interval _4758 = _9172;
            Interval _9189 = iadd(_4757, _4758, intervalFailed);
            Interval _4759 = _8968;
            Interval _4760 = _9175;
            Interval _9190 = jet_add_derivative(_4759, _4760, intervalFailed);
            Interval _4761 = _8973;
            Interval _4762 = _9178;
            Interval _9191 = jet_add_derivative(_4761, _4762, intervalFailed);
            Interval _4751 = _8975;
            Interval _4752 = _9179;
            Interval _9192 = iadd(_4751, _4752, intervalFailed);
            Interval _4753 = _8980;
            Interval _4754 = _9182;
            Interval _9193 = jet_add_derivative(_4753, _4754, intervalFailed);
            Interval _4755 = _8985;
            Interval _4756 = _9185;
            Interval _9194 = jet_add_derivative(_4755, _4756, intervalFailed);
            bool _9202;
            if (receiver.settings.x <= 0.0)
            {
                _9202 = receiver.settings.x >= 0.0;
            }
            else
            {
                _9202 = false;
            }
            float _9209;
            if (_9202)
            {
                _9209 = 0.0;
            }
            else
            {
                _9209 = precise::min(abs(receiver.settings.x), abs(receiver.settings.x));
            }
            float _9212 = precise::max(abs(receiver.settings.x), abs(receiver.settings.x));
            float _4741 = spvFMul(_9209, _9209);
            float _9213 = interval_down(_4741, intervalFailed);
            float _9214 = precise::max(0.0, _9213);
            float _4742 = spvFMul(_9212, _9212);
            float _9215 = interval_up(_4742, intervalFailed);
            Interval _4743 = Interval{ 2.0, 2.0 };
            Interval _4744 = Interval{ receiver.settings.x, receiver.settings.x };
            Interval _9217 = imul(_4743, _4744, intervalFailed, optical_product_upper);
            Interval _4745 = _9217;
            Interval _4746 = Interval{ 0.0, 0.0 };
            Interval _9218 = jet_mul_derivative(_4745, _4746, intervalFailed, optical_product_upper);
            Interval _4747 = Interval{ 2.0, 2.0 };
            Interval _4748 = Interval{ receiver.settings.x, receiver.settings.x };
            Interval _9220 = imul(_4747, _4748, intervalFailed, optical_product_upper);
            Interval _4749 = _9220;
            Interval _4750 = Interval{ 0.0, 0.0 };
            Interval _9221 = jet_mul_derivative(_4749, _4750, intervalFailed, optical_product_upper);
            Interval _4727 = _9186;
            Interval _4728 = Interval{ _9214, _9215 };
            Interval _9223 = imul(_4727, _4728, intervalFailed, optical_product_upper);
            Interval _4729 = _9187;
            Interval _4730 = Interval{ _9214, _9215 };
            Interval _9225 = jet_mul_derivative(_4729, _4730, intervalFailed, optical_product_upper);
            Interval _4731 = _9225;
            Interval _4732 = _9186;
            Interval _4733 = _9218;
            Interval _9226 = jet_mul_derivative(_4732, _4733, intervalFailed, optical_product_upper);
            Interval _4734 = _9226;
            Interval _9227 = jet_add_derivative(_4731, _4734, intervalFailed);
            Interval _4735 = _9188;
            Interval _4736 = Interval{ _9214, _9215 };
            Interval _9229 = jet_mul_derivative(_4735, _4736, intervalFailed, optical_product_upper);
            Interval _4737 = _9229;
            Interval _4738 = _9186;
            Interval _4739 = _9221;
            Interval _9230 = jet_mul_derivative(_4738, _4739, intervalFailed, optical_product_upper);
            Interval _4740 = _9230;
            Interval _9231 = jet_add_derivative(_4737, _4740, intervalFailed);
            Interval _4713 = _9189;
            Interval _4714 = Interval{ _9214, _9215 };
            Interval _9233 = imul(_4713, _4714, intervalFailed, optical_product_upper);
            Interval _4715 = _9190;
            Interval _4716 = Interval{ _9214, _9215 };
            Interval _9235 = jet_mul_derivative(_4715, _4716, intervalFailed, optical_product_upper);
            Interval _4717 = _9235;
            Interval _4718 = _9189;
            Interval _4719 = _9218;
            Interval _9236 = jet_mul_derivative(_4718, _4719, intervalFailed, optical_product_upper);
            Interval _4720 = _9236;
            Interval _9237 = jet_add_derivative(_4717, _4720, intervalFailed);
            Interval _4721 = _9191;
            Interval _4722 = Interval{ _9214, _9215 };
            Interval _9239 = jet_mul_derivative(_4721, _4722, intervalFailed, optical_product_upper);
            Interval _4723 = _9239;
            Interval _4724 = _9189;
            Interval _4725 = _9221;
            Interval _9240 = jet_mul_derivative(_4724, _4725, intervalFailed, optical_product_upper);
            Interval _4726 = _9240;
            Interval _9241 = jet_add_derivative(_4723, _4726, intervalFailed);
            Interval _4699 = _9192;
            Interval _4700 = Interval{ _9214, _9215 };
            Interval _9243 = imul(_4699, _4700, intervalFailed, optical_product_upper);
            Interval _4701 = _9193;
            Interval _4702 = Interval{ _9214, _9215 };
            Interval _9245 = jet_mul_derivative(_4701, _4702, intervalFailed, optical_product_upper);
            Interval _4703 = _9245;
            Interval _4704 = _9192;
            Interval _4705 = _9218;
            Interval _9246 = jet_mul_derivative(_4704, _4705, intervalFailed, optical_product_upper);
            Interval _4706 = _9246;
            Interval _9247 = jet_add_derivative(_4703, _4706, intervalFailed);
            Interval _4707 = _9194;
            Interval _4708 = Interval{ _9214, _9215 };
            Interval _9249 = jet_mul_derivative(_4707, _4708, intervalFailed, optical_product_upper);
            Interval _4709 = _9249;
            Interval _4710 = _9192;
            Interval _4711 = _9221;
            Interval _9250 = jet_mul_derivative(_4710, _4711, intervalFailed, optical_product_upper);
            Interval _4712 = _9250;
            Interval _9251 = jet_add_derivative(_4709, _4712, intervalFailed);
            Interval _4693 = _8168;
            Interval _4694 = _9223;
            Interval _9252 = iadd(_4693, _4694, intervalFailed);
            Interval _4695 = _8171;
            Interval _4696 = _9227;
            Interval _9253 = jet_add_derivative(_4695, _4696, intervalFailed);
            Interval _4697 = _8174;
            Interval _4698 = _9231;
            Interval _9254 = jet_add_derivative(_4697, _4698, intervalFailed);
            Interval _4687 = _8177;
            Interval _4688 = _9233;
            Interval _9255 = iadd(_4687, _4688, intervalFailed);
            Interval _4689 = _8180;
            Interval _4690 = _9237;
            Interval _9256 = jet_add_derivative(_4689, _4690, intervalFailed);
            Interval _4691 = _8183;
            Interval _4692 = _9241;
            Interval _9257 = jet_add_derivative(_4691, _4692, intervalFailed);
            Interval _4681 = _8186;
            Interval _4682 = _9243;
            Interval _9258 = iadd(_4681, _4682, intervalFailed);
            Interval _4683 = _8189;
            Interval _4684 = _9247;
            Interval _9259 = jet_add_derivative(_4683, _4684, intervalFailed);
            Interval _4685 = _8192;
            Interval _4686 = _9251;
            Interval _9260 = jet_add_derivative(_4685, _4686, intervalFailed);
            bool _9267;
            if (_9252.lo <= 0.0)
            {
                _9267 = _9252.hi >= 0.0;
            }
            else
            {
                _9267 = false;
            }
            float _9274;
            if (_9267)
            {
                _9274 = 0.0;
            }
            else
            {
                _9274 = precise::min(abs(_9252.lo), abs(_9252.hi));
            }
            float _9277 = precise::max(abs(_9252.lo), abs(_9252.hi));
            float _4671 = spvFMul(_9274, _9274);
            float _9278 = interval_down(_4671, intervalFailed);
            float _4672 = spvFMul(_9277, _9277);
            float _9280 = interval_up(_4672, intervalFailed);
            Interval _4673 = Interval{ 2.0, 2.0 };
            Interval _4674 = _9252;
            Interval _9281 = imul(_4673, _4674, intervalFailed, optical_product_upper);
            Interval _4675 = _9281;
            Interval _4676 = _9253;
            Interval _9282 = jet_mul_derivative(_4675, _4676, intervalFailed, optical_product_upper);
            Interval _4677 = Interval{ 2.0, 2.0 };
            Interval _4678 = _9252;
            Interval _9283 = imul(_4677, _4678, intervalFailed, optical_product_upper);
            Interval _4679 = _9283;
            Interval _4680 = _9254;
            Interval _9284 = jet_mul_derivative(_4679, _4680, intervalFailed, optical_product_upper);
            bool _9291;
            if (_9255.lo <= 0.0)
            {
                _9291 = _9255.hi >= 0.0;
            }
            else
            {
                _9291 = false;
            }
            float _9298;
            if (_9291)
            {
                _9298 = 0.0;
            }
            else
            {
                _9298 = precise::min(abs(_9255.lo), abs(_9255.hi));
            }
            float _9301 = precise::max(abs(_9255.lo), abs(_9255.hi));
            float _4661 = spvFMul(_9298, _9298);
            float _9302 = interval_down(_4661, intervalFailed);
            float _4662 = spvFMul(_9301, _9301);
            float _9304 = interval_up(_4662, intervalFailed);
            Interval _4663 = Interval{ 2.0, 2.0 };
            Interval _4664 = _9255;
            Interval _9305 = imul(_4663, _4664, intervalFailed, optical_product_upper);
            Interval _4665 = _9305;
            Interval _4666 = _9256;
            Interval _9306 = jet_mul_derivative(_4665, _4666, intervalFailed, optical_product_upper);
            Interval _4667 = Interval{ 2.0, 2.0 };
            Interval _4668 = _9255;
            Interval _9307 = imul(_4667, _4668, intervalFailed, optical_product_upper);
            Interval _4669 = _9307;
            Interval _4670 = _9257;
            Interval _9308 = jet_mul_derivative(_4669, _4670, intervalFailed, optical_product_upper);
            Interval _4655 = Interval{ precise::max(0.0, _9278), _9280 };
            Interval _4656 = Interval{ precise::max(0.0, _9302), _9304 };
            Interval _9311 = iadd(_4655, _4656, intervalFailed);
            Interval _4657 = _9282;
            Interval _4658 = _9306;
            Interval _9312 = jet_add_derivative(_4657, _4658, intervalFailed);
            Interval _4659 = _9284;
            Interval _4660 = _9308;
            Interval _9313 = jet_add_derivative(_4659, _4660, intervalFailed);
            bool _9320;
            if (_9258.lo <= 0.0)
            {
                _9320 = _9258.hi >= 0.0;
            }
            else
            {
                _9320 = false;
            }
            float _9327;
            if (_9320)
            {
                _9327 = 0.0;
            }
            else
            {
                _9327 = precise::min(abs(_9258.lo), abs(_9258.hi));
            }
            float _9330 = precise::max(abs(_9258.lo), abs(_9258.hi));
            float _4645 = spvFMul(_9327, _9327);
            float _9331 = interval_down(_4645, intervalFailed);
            float _4646 = spvFMul(_9330, _9330);
            float _9333 = interval_up(_4646, intervalFailed);
            Interval _4647 = Interval{ 2.0, 2.0 };
            Interval _4648 = _9258;
            Interval _9334 = imul(_4647, _4648, intervalFailed, optical_product_upper);
            Interval _4649 = _9334;
            Interval _4650 = _9259;
            Interval _9335 = jet_mul_derivative(_4649, _4650, intervalFailed, optical_product_upper);
            Interval _4651 = Interval{ 2.0, 2.0 };
            Interval _4652 = _9258;
            Interval _9336 = imul(_4651, _4652, intervalFailed, optical_product_upper);
            Interval _4653 = _9336;
            Interval _4654 = _9260;
            Interval _9337 = jet_mul_derivative(_4653, _4654, intervalFailed, optical_product_upper);
            Interval _4639 = _9311;
            Interval _4640 = Interval{ precise::max(0.0, _9331), _9333 };
            Interval _9339 = iadd(_4639, _4640, intervalFailed);
            Interval _4641 = _9312;
            Interval _4642 = _9335;
            Interval _9340 = jet_add_derivative(_4641, _4642, intervalFailed);
            Interval _4643 = _9313;
            Interval _4644 = _9337;
            Interval _9341 = jet_add_derivative(_4643, _4644, intervalFailed);
            Interval _4630 = _9339;
            Interval _9343 = isqrt(_4630, intervalFailed);
            bool _9351;
            if (!intervalFailed)
            {
                _9351 = intervalFailed;
            }
            else
            {
                _9351 = false;
            }
            bool _9356;
            if (_9351)
            {
                _9356 = jetFailureSite == 0u;
            }
            else
            {
                _9356 = false;
            }
            if (_9356)
            {
                jetFailureSite = 3u;
                jetFailureArguments = float4(_9339.lo, _9339.hi, 0.0, 0.0);
            }
            if (_9343.lo <= 0.0)
            {
                jetBranchKnown = false;
            }
            Interval _4631 = Interval{ 2.0, 2.0 };
            Interval _4632 = _9343;
            Interval _9363 = imul(_4631, _4632, intervalFailed, optical_product_upper);
            Interval _4633 = Interval{ 1.0, 1.0 };
            Interval _4634 = _9363;
            Interval _9365 = idiv(_4633, _4634, intervalFailed, interval_divide_upper);
            bool _9372;
            if (!intervalFailed)
            {
                _9372 = intervalFailed;
            }
            else
            {
                _9372 = false;
            }
            bool _9377;
            if (_9372)
            {
                _9377 = jetFailureSite == 0u;
            }
            else
            {
                _9377 = false;
            }
            if (_9377)
            {
                jetFailureSite = 4u;
                jetFailureArguments = float4(1.0, 1.0, _9363.lo, _9363.hi);
            }
            Interval _4635 = _9365;
            Interval _4636 = _9340;
            Interval _9381 = jet_mul_derivative(_4635, _4636, intervalFailed, optical_product_upper);
            Interval _4637 = _9365;
            Interval _4638 = _9341;
            Interval _9382 = jet_mul_derivative(_4637, _4638, intervalFailed, optical_product_upper);
            Interval _4616 = Interval{ 1.0, 1.0 };
            Interval _4617 = _9343;
            Interval _9384 = idiv(_4616, _4617, intervalFailed, interval_divide_upper);
            bool _9391;
            if (!intervalFailed)
            {
                _9391 = intervalFailed;
            }
            else
            {
                _9391 = false;
            }
            bool _9396;
            if (_9391)
            {
                _9396 = jetFailureSite == 0u;
            }
            else
            {
                _9396 = false;
            }
            if (_9396)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(1.0, 1.0, _9343.lo, _9343.hi);
            }
            Interval _4618 = Interval{ 0.0, 0.0 };
            Interval _4619 = _9384;
            Interval _4620 = _9381;
            Interval _9400 = jet_mul_derivative(_4619, _4620, intervalFailed, optical_product_upper);
            Interval _4621 = Interval{ as_type<float>(as_type<uint>(_9400.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9400.lo) ^ 2147483648u) };
            Interval _9410 = jet_add_derivative(_4618, _4621, intervalFailed);
            Interval _4622 = _9410;
            Interval _4623 = _9343;
            Interval _9411 = jet_div_derivative(_4622, _4623, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _4624 = Interval{ 0.0, 0.0 };
            Interval _4625 = _9384;
            Interval _4626 = _9382;
            Interval _9412 = jet_mul_derivative(_4625, _4626, intervalFailed, optical_product_upper);
            Interval _4627 = Interval{ as_type<float>(as_type<uint>(_9412.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9412.lo) ^ 2147483648u) };
            Interval _9422 = jet_add_derivative(_4624, _4627, intervalFailed);
            Interval _4628 = _9422;
            Interval _4629 = _9343;
            Interval _9423 = jet_div_derivative(_4628, _4629, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _4602 = _9252;
            Interval _4603 = _9384;
            Interval _9424 = imul(_4602, _4603, intervalFailed, optical_product_upper);
            Interval _4604 = _9253;
            Interval _4605 = _9384;
            Interval _9425 = jet_mul_derivative(_4604, _4605, intervalFailed, optical_product_upper);
            Interval _4606 = _9425;
            Interval _4607 = _9252;
            Interval _4608 = _9411;
            Interval _9426 = jet_mul_derivative(_4607, _4608, intervalFailed, optical_product_upper);
            Interval _4609 = _9426;
            Interval _9427 = jet_add_derivative(_4606, _4609, intervalFailed);
            Interval _4610 = _9254;
            Interval _4611 = _9384;
            Interval _9428 = jet_mul_derivative(_4610, _4611, intervalFailed, optical_product_upper);
            Interval _4612 = _9428;
            Interval _4613 = _9252;
            Interval _4614 = _9423;
            Interval _9429 = jet_mul_derivative(_4613, _4614, intervalFailed, optical_product_upper);
            Interval _4615 = _9429;
            Interval _9430 = jet_add_derivative(_4612, _4615, intervalFailed);
            Interval _4588 = _9255;
            Interval _4589 = _9384;
            Interval _9431 = imul(_4588, _4589, intervalFailed, optical_product_upper);
            Interval _4590 = _9256;
            Interval _4591 = _9384;
            Interval _9432 = jet_mul_derivative(_4590, _4591, intervalFailed, optical_product_upper);
            Interval _4592 = _9432;
            Interval _4593 = _9255;
            Interval _4594 = _9411;
            Interval _9433 = jet_mul_derivative(_4593, _4594, intervalFailed, optical_product_upper);
            Interval _4595 = _9433;
            Interval _9434 = jet_add_derivative(_4592, _4595, intervalFailed);
            Interval _4596 = _9257;
            Interval _4597 = _9384;
            Interval _9435 = jet_mul_derivative(_4596, _4597, intervalFailed, optical_product_upper);
            Interval _4598 = _9435;
            Interval _4599 = _9255;
            Interval _4600 = _9423;
            Interval _9436 = jet_mul_derivative(_4599, _4600, intervalFailed, optical_product_upper);
            Interval _4601 = _9436;
            Interval _9437 = jet_add_derivative(_4598, _4601, intervalFailed);
            Interval _4574 = _9258;
            Interval _4575 = _9384;
            Interval _9438 = imul(_4574, _4575, intervalFailed, optical_product_upper);
            Interval _4576 = _9259;
            Interval _4577 = _9384;
            Interval _9439 = jet_mul_derivative(_4576, _4577, intervalFailed, optical_product_upper);
            Interval _4578 = _9439;
            Interval _4579 = _9258;
            Interval _4580 = _9411;
            Interval _9440 = jet_mul_derivative(_4579, _4580, intervalFailed, optical_product_upper);
            Interval _4581 = _9440;
            Interval _9441 = jet_add_derivative(_4578, _4581, intervalFailed);
            Interval _4582 = _9260;
            Interval _4583 = _9384;
            Interval _9442 = jet_mul_derivative(_4582, _4583, intervalFailed, optical_product_upper);
            Interval _4584 = _9442;
            Interval _4585 = _9258;
            Interval _4586 = _9423;
            Interval _9443 = jet_mul_derivative(_4585, _4586, intervalFailed, optical_product_upper);
            Interval _4587 = _9443;
            Interval _9444 = jet_add_derivative(_4584, _4587, intervalFailed);
            Interval _4560 = _9424;
            Interval _4561 = Interval{ _7844, _7845 };
            Interval _9446 = imul(_4560, _4561, intervalFailed, optical_product_upper);
            Interval _4562 = _9427;
            Interval _4563 = Interval{ _7844, _7845 };
            Interval _9448 = jet_mul_derivative(_4562, _4563, intervalFailed, optical_product_upper);
            Interval _4564 = _9448;
            Interval _4565 = _9424;
            Interval _4566 = Interval{ _7846, _7847 };
            Interval _9450 = jet_mul_derivative(_4565, _4566, intervalFailed, optical_product_upper);
            Interval _4567 = _9450;
            Interval _9451 = jet_add_derivative(_4564, _4567, intervalFailed);
            Interval _4568 = _9430;
            Interval _4569 = Interval{ _7844, _7845 };
            Interval _9453 = jet_mul_derivative(_4568, _4569, intervalFailed, optical_product_upper);
            Interval _4570 = _9453;
            Interval _4571 = _9424;
            Interval _4572 = Interval{ _7848, _7849 };
            Interval _9455 = jet_mul_derivative(_4571, _4572, intervalFailed, optical_product_upper);
            Interval _4573 = _9455;
            Interval _9456 = jet_add_derivative(_4570, _4573, intervalFailed);
            Interval _4546 = _9431;
            Interval _4547 = Interval{ _7850, _7851 };
            Interval _9458 = imul(_4546, _4547, intervalFailed, optical_product_upper);
            Interval _4548 = _9434;
            Interval _4549 = Interval{ _7850, _7851 };
            Interval _9460 = jet_mul_derivative(_4548, _4549, intervalFailed, optical_product_upper);
            Interval _4550 = _9460;
            Interval _4551 = _9431;
            Interval _4552 = Interval{ _7852, _7853 };
            Interval _9462 = jet_mul_derivative(_4551, _4552, intervalFailed, optical_product_upper);
            Interval _4553 = _9462;
            Interval _9463 = jet_add_derivative(_4550, _4553, intervalFailed);
            Interval _4554 = _9437;
            Interval _4555 = Interval{ _7850, _7851 };
            Interval _9465 = jet_mul_derivative(_4554, _4555, intervalFailed, optical_product_upper);
            Interval _4556 = _9465;
            Interval _4557 = _9431;
            Interval _4558 = Interval{ _7854, _7855 };
            Interval _9467 = jet_mul_derivative(_4557, _4558, intervalFailed, optical_product_upper);
            Interval _4559 = _9467;
            Interval _9468 = jet_add_derivative(_4556, _4559, intervalFailed);
            Interval _4540 = _9446;
            Interval _4541 = _9458;
            Interval _9469 = iadd(_4540, _4541, intervalFailed);
            Interval _4542 = _9451;
            Interval _4543 = _9463;
            Interval _9470 = jet_add_derivative(_4542, _4543, intervalFailed);
            Interval _4544 = _9456;
            Interval _4545 = _9468;
            Interval _9471 = jet_add_derivative(_4544, _4545, intervalFailed);
            Interval _4526 = _9438;
            Interval _4527 = Interval{ _7856, _7857 };
            Interval _9473 = imul(_4526, _4527, intervalFailed, optical_product_upper);
            Interval _4528 = _9441;
            Interval _4529 = Interval{ _7856, _7857 };
            Interval _9475 = jet_mul_derivative(_4528, _4529, intervalFailed, optical_product_upper);
            Interval _4530 = _9475;
            Interval _4531 = _9438;
            Interval _4532 = Interval{ _7858, _7859 };
            Interval _9477 = jet_mul_derivative(_4531, _4532, intervalFailed, optical_product_upper);
            Interval _4533 = _9477;
            Interval _9478 = jet_add_derivative(_4530, _4533, intervalFailed);
            Interval _4534 = _9444;
            Interval _4535 = Interval{ _7856, _7857 };
            Interval _9480 = jet_mul_derivative(_4534, _4535, intervalFailed, optical_product_upper);
            Interval _4536 = _9480;
            Interval _4537 = _9438;
            Interval _4538 = Interval{ _7860, _7861 };
            Interval _9482 = jet_mul_derivative(_4537, _4538, intervalFailed, optical_product_upper);
            Interval _4539 = _9482;
            Interval _9483 = jet_add_derivative(_4536, _4539, intervalFailed);
            Interval _4520 = _9469;
            Interval _4521 = _9473;
            Interval _9484 = iadd(_4520, _4521, intervalFailed);
            Interval _4522 = _9470;
            Interval _4523 = _9478;
            Interval _9485 = jet_add_derivative(_4522, _4523, intervalFailed);
            Interval _4524 = _9471;
            Interval _4525 = _9483;
            Interval _9486 = jet_add_derivative(_4524, _4525, intervalFailed);
            float _9509;
            float _9510;
            float _9511;
            float _9512;
            float _9513;
            float _9514;
            float _9515;
            float _9516;
            float _9517;
            float _9518;
            float _9519;
            float _9520;
            float _9521;
            float _9522;
            float _9523;
            float _9524;
            float _9525;
            float _9526;
            if (_9484.lo > 0.0)
            {
                _9509 = _9424.lo;
                _9510 = _9424.hi;
                _9511 = _9427.lo;
                _9512 = _9427.hi;
                _9513 = _9430.lo;
                _9514 = _9430.hi;
                _9515 = _9431.lo;
                _9516 = _9431.hi;
                _9517 = _9434.lo;
                _9518 = _9434.hi;
                _9519 = _9437.lo;
                _9520 = _9437.hi;
                _9521 = _9438.lo;
                _9522 = _9438.hi;
                _9523 = _9441.lo;
                _9524 = _9441.hi;
                _9525 = _9444.lo;
                _9526 = _9444.hi;
            }
            else
            {
                if (_9484.hi > 0.0)
                {
                    return false;
                }
                _9509 = _8168.lo;
                _9510 = _8168.hi;
                _9511 = _8171.lo;
                _9512 = _8171.hi;
                _9513 = _8174.lo;
                _9514 = _8174.hi;
                _9515 = _8177.lo;
                _9516 = _8177.hi;
                _9517 = _8180.lo;
                _9518 = _8180.hi;
                _9519 = _8183.lo;
                _9520 = _8183.hi;
                _9521 = _8186.lo;
                _9522 = _8186.hi;
                _9523 = _8189.lo;
                _9524 = _8189.hi;
                _9525 = _8192.lo;
                _9526 = _8192.hi;
            }
            _6685 = _9509;
            _6687 = _9510;
            _6689 = _9511;
            _6691 = _9512;
            _6693 = _9513;
            _6695 = _9514;
            _6697 = _9515;
            _6699 = _9516;
            _6701 = _9517;
            _6703 = _9518;
            _6705 = _9519;
            _6707 = _9520;
            _6709 = _9521;
            _6711 = _9522;
            _6713 = _9523;
            _6715 = _9524;
            _6717 = _9525;
            _6719 = _9526;
        }
        else
        {
            _6685 = _8168.lo;
            _6687 = _8168.hi;
            _6689 = _8171.lo;
            _6691 = _8171.hi;
            _6693 = _8174.lo;
            _6695 = _8174.hi;
            _6697 = _8177.lo;
            _6699 = _8177.hi;
            _6701 = _8180.lo;
            _6703 = _8180.hi;
            _6705 = _8183.lo;
            _6707 = _8183.hi;
            _6709 = _8186.lo;
            _6711 = _8186.hi;
            _6713 = _8189.lo;
            _6715 = _8189.hi;
            _6717 = _8192.lo;
            _6719 = _8192.hi;
        }
        bool _9531;
        if (!intervalFailed)
        {
            _9531 = !jetBranchKnown;
        }
        else
        {
            _9531 = true;
        }
        if (_9531)
        {
            return false;
        }
    }
    ray.origin = OpticalJet3{ OpticalJet{ Interval{ _6720, _6722 }, Interval{ _6724, _6726 }, Interval{ _6728, _6730 } }, OpticalJet{ Interval{ _6732, _6734 }, Interval{ _6736, _6738 }, Interval{ _6740, _6742 } }, OpticalJet{ Interval{ _6744, _6746 }, Interval{ _6748, _6750 }, Interval{ _6752, _6754 } } };
    ray.outgoing = OpticalJet3{ OpticalJet{ Interval{ _6684, _6686 }, Interval{ _6688, _6690 }, Interval{ _6692, _6694 } }, OpticalJet{ Interval{ _6696, _6698 }, Interval{ _6700, _6702 }, Interval{ _6704, _6706 } }, OpticalJet{ Interval{ _6708, _6710 }, Interval{ _6712, _6714 }, Interval{ _6716, _6718 } } };
    ray.bias0 = OpticalJet{ Interval{ _6672, _6674 }, Interval{ _6676, _6678 }, Interval{ _6680, _6682 } };
    bool _9568;
    if (!intervalFailed)
    {
        _9568 = jetBranchKnown;
    }
    else
    {
        _9568 = false;
    }
    return _9568;
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
    float _10078;
    float _10079;
    float _10080;
    float _10081;
    float _10082;
    float _10083;
    float _10084;
    float _10085;
    float _10086;
    float _10087;
    float _10088;
    float _10089;
    float _10090;
    float _10091;
    float _10092;
    float _10093;
    float _10094;
    float _10095;
    if (finiteTerminal)
    {
        Interval _9937 = target.x;
        Interval _9938 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.v.lo) ^ 2147483648u) };
        Interval _10043 = iadd(_9937, _9938, intervalFailed);
        Interval _9939 = Interval{ 0.0, 0.0 };
        Interval _9940 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.dx.lo) ^ 2147483648u) };
        Interval _10045 = jet_add_derivative(_9939, _9940, intervalFailed);
        Interval _9941 = Interval{ 0.0, 0.0 };
        Interval _9942 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.dy.lo) ^ 2147483648u) };
        Interval _10047 = jet_add_derivative(_9941, _9942, intervalFailed);
        Interval _9931 = target.y;
        Interval _9932 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.v.lo) ^ 2147483648u) };
        Interval _10049 = iadd(_9931, _9932, intervalFailed);
        Interval _9933 = Interval{ 0.0, 0.0 };
        Interval _9934 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.dx.lo) ^ 2147483648u) };
        Interval _10051 = jet_add_derivative(_9933, _9934, intervalFailed);
        Interval _9935 = Interval{ 0.0, 0.0 };
        Interval _9936 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.dy.lo) ^ 2147483648u) };
        Interval _10053 = jet_add_derivative(_9935, _9936, intervalFailed);
        Interval _9925 = target.z;
        Interval _9926 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.v.lo) ^ 2147483648u) };
        Interval _10055 = iadd(_9925, _9926, intervalFailed);
        Interval _9927 = Interval{ 0.0, 0.0 };
        Interval _9928 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.dx.lo) ^ 2147483648u) };
        Interval _10057 = jet_add_derivative(_9927, _9928, intervalFailed);
        Interval _9929 = Interval{ 0.0, 0.0 };
        Interval _9930 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.dy.lo) ^ 2147483648u) };
        Interval _10059 = jet_add_derivative(_9929, _9930, intervalFailed);
        _10078 = _10043.lo;
        _10079 = _10043.hi;
        _10080 = _10045.lo;
        _10081 = _10045.hi;
        _10082 = _10047.lo;
        _10083 = _10047.hi;
        _10084 = _10049.lo;
        _10085 = _10049.hi;
        _10086 = _10051.lo;
        _10087 = _10051.hi;
        _10088 = _10053.lo;
        _10089 = _10053.hi;
        _10090 = _10055.lo;
        _10091 = _10055.hi;
        _10092 = _10057.lo;
        _10093 = _10057.hi;
        _10094 = _10059.lo;
        _10095 = _10059.hi;
    }
    else
    {
        _10078 = target.x.lo;
        _10079 = target.x.hi;
        _10080 = 0.0;
        _10081 = 0.0;
        _10082 = 0.0;
        _10083 = 0.0;
        _10084 = target.y.lo;
        _10085 = target.y.hi;
        _10086 = 0.0;
        _10087 = 0.0;
        _10088 = 0.0;
        _10089 = 0.0;
        _10090 = target.z.lo;
        _10091 = target.z.hi;
        _10092 = 0.0;
        _10093 = 0.0;
        _10094 = 0.0;
        _10095 = 0.0;
    }
    bool _10100;
    if (_10078 <= 0.0)
    {
        _10100 = _10079 >= 0.0;
    }
    else
    {
        _10100 = false;
    }
    float _10107;
    if (_10100)
    {
        _10107 = 0.0;
    }
    else
    {
        _10107 = precise::min(abs(_10078), abs(_10079));
    }
    float _10110 = precise::max(abs(_10078), abs(_10079));
    float _9915 = spvFMul(_10107, _10107);
    float _10111 = interval_down(_9915, intervalFailed);
    float _9916 = spvFMul(_10110, _10110);
    float _10113 = interval_up(_9916, intervalFailed);
    Interval _9917 = Interval{ 2.0, 2.0 };
    Interval _9918 = Interval{ _10078, _10079 };
    Interval _10115 = imul(_9917, _9918, intervalFailed, optical_product_upper);
    Interval _9919 = _10115;
    Interval _9920 = Interval{ _10080, _10081 };
    Interval _10117 = jet_mul_derivative(_9919, _9920, intervalFailed, optical_product_upper);
    Interval _9921 = Interval{ 2.0, 2.0 };
    Interval _9922 = Interval{ _10078, _10079 };
    Interval _10119 = imul(_9921, _9922, intervalFailed, optical_product_upper);
    Interval _9923 = _10119;
    Interval _9924 = Interval{ _10082, _10083 };
    Interval _10121 = jet_mul_derivative(_9923, _9924, intervalFailed, optical_product_upper);
    bool _10126;
    if (_10084 <= 0.0)
    {
        _10126 = _10085 >= 0.0;
    }
    else
    {
        _10126 = false;
    }
    float _10133;
    if (_10126)
    {
        _10133 = 0.0;
    }
    else
    {
        _10133 = precise::min(abs(_10084), abs(_10085));
    }
    float _10136 = precise::max(abs(_10084), abs(_10085));
    float _9905 = spvFMul(_10133, _10133);
    float _10137 = interval_down(_9905, intervalFailed);
    float _9906 = spvFMul(_10136, _10136);
    float _10139 = interval_up(_9906, intervalFailed);
    Interval _9907 = Interval{ 2.0, 2.0 };
    Interval _9908 = Interval{ _10084, _10085 };
    Interval _10141 = imul(_9907, _9908, intervalFailed, optical_product_upper);
    Interval _9909 = _10141;
    Interval _9910 = Interval{ _10086, _10087 };
    Interval _10143 = jet_mul_derivative(_9909, _9910, intervalFailed, optical_product_upper);
    Interval _9911 = Interval{ 2.0, 2.0 };
    Interval _9912 = Interval{ _10084, _10085 };
    Interval _10145 = imul(_9911, _9912, intervalFailed, optical_product_upper);
    Interval _9913 = _10145;
    Interval _9914 = Interval{ _10088, _10089 };
    Interval _10147 = jet_mul_derivative(_9913, _9914, intervalFailed, optical_product_upper);
    Interval _9899 = Interval{ precise::max(0.0, _10111), _10113 };
    Interval _9900 = Interval{ precise::max(0.0, _10137), _10139 };
    Interval _10150 = iadd(_9899, _9900, intervalFailed);
    Interval _9901 = _10117;
    Interval _9902 = _10143;
    Interval _10151 = jet_add_derivative(_9901, _9902, intervalFailed);
    Interval _9903 = _10121;
    Interval _9904 = _10147;
    Interval _10152 = jet_add_derivative(_9903, _9904, intervalFailed);
    bool _10157;
    if (_10090 <= 0.0)
    {
        _10157 = _10091 >= 0.0;
    }
    else
    {
        _10157 = false;
    }
    float _10164;
    if (_10157)
    {
        _10164 = 0.0;
    }
    else
    {
        _10164 = precise::min(abs(_10090), abs(_10091));
    }
    float _10167 = precise::max(abs(_10090), abs(_10091));
    float _9889 = spvFMul(_10164, _10164);
    float _10168 = interval_down(_9889, intervalFailed);
    float _9890 = spvFMul(_10167, _10167);
    float _10170 = interval_up(_9890, intervalFailed);
    Interval _9891 = Interval{ 2.0, 2.0 };
    Interval _9892 = Interval{ _10090, _10091 };
    Interval _10172 = imul(_9891, _9892, intervalFailed, optical_product_upper);
    Interval _9893 = _10172;
    Interval _9894 = Interval{ _10092, _10093 };
    Interval _10174 = jet_mul_derivative(_9893, _9894, intervalFailed, optical_product_upper);
    Interval _9895 = Interval{ 2.0, 2.0 };
    Interval _9896 = Interval{ _10090, _10091 };
    Interval _10176 = imul(_9895, _9896, intervalFailed, optical_product_upper);
    Interval _9897 = _10176;
    Interval _9898 = Interval{ _10094, _10095 };
    Interval _10178 = jet_mul_derivative(_9897, _9898, intervalFailed, optical_product_upper);
    Interval _9883 = _10150;
    Interval _9884 = Interval{ precise::max(0.0, _10168), _10170 };
    Interval _10180 = iadd(_9883, _9884, intervalFailed);
    Interval _9885 = _10151;
    Interval _9886 = _10174;
    Interval _10181 = jet_add_derivative(_9885, _9886, intervalFailed);
    Interval _9887 = _10152;
    Interval _9888 = _10178;
    Interval _10182 = jet_add_derivative(_9887, _9888, intervalFailed);
    Interval _9874 = _10180;
    Interval _10184 = isqrt(_9874, intervalFailed);
    bool _10192;
    if (!intervalFailed)
    {
        _10192 = intervalFailed;
    }
    else
    {
        _10192 = false;
    }
    bool _10197;
    if (_10192)
    {
        _10197 = jetFailureSite == 0u;
    }
    else
    {
        _10197 = false;
    }
    if (_10197)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_10180.lo, _10180.hi, 0.0, 0.0);
    }
    if (_10184.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _9875 = Interval{ 2.0, 2.0 };
    Interval _9876 = _10184;
    Interval _10204 = imul(_9875, _9876, intervalFailed, optical_product_upper);
    Interval _9877 = Interval{ 1.0, 1.0 };
    Interval _9878 = _10204;
    Interval _10206 = idiv(_9877, _9878, intervalFailed, interval_divide_upper);
    bool _10213;
    if (!intervalFailed)
    {
        _10213 = intervalFailed;
    }
    else
    {
        _10213 = false;
    }
    bool _10218;
    if (_10213)
    {
        _10218 = jetFailureSite == 0u;
    }
    else
    {
        _10218 = false;
    }
    if (_10218)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _10204.lo, _10204.hi);
    }
    Interval _9879 = _10206;
    Interval _9880 = _10181;
    Interval _10222 = jet_mul_derivative(_9879, _9880, intervalFailed, optical_product_upper);
    Interval _9881 = _10206;
    Interval _9882 = _10182;
    Interval _10223 = jet_mul_derivative(_9881, _9882, intervalFailed, optical_product_upper);
    float _10228;
    if (finiteTerminal)
    {
        _10228 = ray.bias0.v.hi;
    }
    else
    {
        _10228 = 0.0;
    }
    bool _10288;
    if ((isunordered(_10184.lo, _10228) || _10184.lo > _10228))
    {
        Interval _9860 = Interval{ _10078, _10079 };
        Interval _9861 = ray.outgoing.x.v;
        Interval _10239 = imul(_9860, _9861, intervalFailed, optical_product_upper);
        Interval _9862 = Interval{ _10080, _10081 };
        Interval _9863 = ray.outgoing.x.v;
        Interval _10241 = jet_mul_derivative(_9862, _9863, intervalFailed, optical_product_upper);
        Interval _9864 = _10241;
        Interval _9865 = Interval{ _10078, _10079 };
        Interval _9866 = ray.outgoing.x.dx;
        Interval _10243 = jet_mul_derivative(_9865, _9866, intervalFailed, optical_product_upper);
        Interval _9867 = _10243;
        Interval _10244 = jet_add_derivative(_9864, _9867, intervalFailed);
        Interval _9868 = Interval{ _10082, _10083 };
        Interval _9869 = ray.outgoing.x.v;
        Interval _10246 = jet_mul_derivative(_9868, _9869, intervalFailed, optical_product_upper);
        Interval _9870 = _10246;
        Interval _9871 = Interval{ _10078, _10079 };
        Interval _9872 = ray.outgoing.x.dy;
        Interval _10248 = jet_mul_derivative(_9871, _9872, intervalFailed, optical_product_upper);
        Interval _9873 = _10248;
        Interval _10249 = jet_add_derivative(_9870, _9873, intervalFailed);
        Interval _9846 = Interval{ _10084, _10085 };
        Interval _9847 = ray.outgoing.y.v;
        Interval _10254 = imul(_9846, _9847, intervalFailed, optical_product_upper);
        Interval _9848 = Interval{ _10086, _10087 };
        Interval _9849 = ray.outgoing.y.v;
        Interval _10256 = jet_mul_derivative(_9848, _9849, intervalFailed, optical_product_upper);
        Interval _9850 = _10256;
        Interval _9851 = Interval{ _10084, _10085 };
        Interval _9852 = ray.outgoing.y.dx;
        Interval _10258 = jet_mul_derivative(_9851, _9852, intervalFailed, optical_product_upper);
        Interval _9853 = _10258;
        Interval _10259 = jet_add_derivative(_9850, _9853, intervalFailed);
        Interval _9854 = Interval{ _10088, _10089 };
        Interval _9855 = ray.outgoing.y.v;
        Interval _10261 = jet_mul_derivative(_9854, _9855, intervalFailed, optical_product_upper);
        Interval _9856 = _10261;
        Interval _9857 = Interval{ _10084, _10085 };
        Interval _9858 = ray.outgoing.y.dy;
        Interval _10263 = jet_mul_derivative(_9857, _9858, intervalFailed, optical_product_upper);
        Interval _9859 = _10263;
        Interval _10264 = jet_add_derivative(_9856, _9859, intervalFailed);
        Interval _9840 = _10239;
        Interval _9841 = _10254;
        Interval _10265 = iadd(_9840, _9841, intervalFailed);
        Interval _9842 = _10244;
        Interval _9843 = _10259;
        Interval _10266 = jet_add_derivative(_9842, _9843, intervalFailed);
        Interval _9844 = _10249;
        Interval _9845 = _10264;
        Interval _10267 = jet_add_derivative(_9844, _9845, intervalFailed);
        Interval _9826 = Interval{ _10090, _10091 };
        Interval _9827 = ray.outgoing.z.v;
        Interval _10272 = imul(_9826, _9827, intervalFailed, optical_product_upper);
        Interval _9828 = Interval{ _10092, _10093 };
        Interval _9829 = ray.outgoing.z.v;
        Interval _10274 = jet_mul_derivative(_9828, _9829, intervalFailed, optical_product_upper);
        Interval _9830 = _10274;
        Interval _9831 = Interval{ _10090, _10091 };
        Interval _9832 = ray.outgoing.z.dx;
        Interval _10276 = jet_mul_derivative(_9831, _9832, intervalFailed, optical_product_upper);
        Interval _9833 = _10276;
        Interval _10277 = jet_add_derivative(_9830, _9833, intervalFailed);
        Interval _9834 = Interval{ _10094, _10095 };
        Interval _9835 = ray.outgoing.z.v;
        Interval _10279 = jet_mul_derivative(_9834, _9835, intervalFailed, optical_product_upper);
        Interval _9836 = _10279;
        Interval _9837 = Interval{ _10090, _10091 };
        Interval _9838 = ray.outgoing.z.dy;
        Interval _10281 = jet_mul_derivative(_9837, _9838, intervalFailed, optical_product_upper);
        Interval _9839 = _10281;
        Interval _10282 = jet_add_derivative(_9836, _9839, intervalFailed);
        Interval _9820 = _10265;
        Interval _9821 = _10272;
        Interval _10283 = iadd(_9820, _9821, intervalFailed);
        Interval _9822 = _10266;
        Interval _9823 = _10277;
        Interval _10284 = jet_add_derivative(_9822, _9823, intervalFailed);
        Interval _9824 = _10267;
        Interval _9825 = _10282;
        Interval _10285 = jet_add_derivative(_9824, _9825, intervalFailed);
        _10288 = _10283.lo <= 0.0;
    }
    else
    {
        _10288 = true;
    }
    if (_10288)
    {
        return false;
    }
    float _10307;
    float _10308;
    if (omitted == 0u)
    {
        _10307 = ray.outgoing.x.v.lo;
        _10308 = ray.outgoing.x.v.hi;
    }
    else
    {
        float _10301;
        float _10302;
        if (omitted == 1u)
        {
            _10301 = ray.outgoing.y.v.lo;
            _10302 = ray.outgoing.y.v.hi;
        }
        else
        {
            _10301 = ray.outgoing.z.v.lo;
            _10302 = ray.outgoing.z.v.hi;
        }
        _10307 = _10301;
        _10308 = _10302;
    }
    bool _10313;
    if (_10307 <= 0.0)
    {
        _10313 = _10308 >= 0.0;
    }
    else
    {
        _10313 = false;
    }
    if (_10313)
    {
        return false;
    }
    bool _10318;
    if (_10078 <= 0.0)
    {
        _10318 = _10079 >= 0.0;
    }
    else
    {
        _10318 = false;
    }
    float _10325;
    if (_10318)
    {
        _10325 = 0.0;
    }
    else
    {
        _10325 = precise::min(abs(_10078), abs(_10079));
    }
    float _10328 = precise::max(abs(_10078), abs(_10079));
    float _9810 = spvFMul(_10325, _10325);
    float _10329 = interval_down(_9810, intervalFailed);
    float _9811 = spvFMul(_10328, _10328);
    float _10331 = interval_up(_9811, intervalFailed);
    Interval _9812 = Interval{ 2.0, 2.0 };
    Interval _9813 = Interval{ _10078, _10079 };
    Interval _10333 = imul(_9812, _9813, intervalFailed, optical_product_upper);
    Interval _9814 = _10333;
    Interval _9815 = Interval{ _10080, _10081 };
    Interval _10335 = jet_mul_derivative(_9814, _9815, intervalFailed, optical_product_upper);
    Interval _9816 = Interval{ 2.0, 2.0 };
    Interval _9817 = Interval{ _10078, _10079 };
    Interval _10337 = imul(_9816, _9817, intervalFailed, optical_product_upper);
    Interval _9818 = _10337;
    Interval _9819 = Interval{ _10082, _10083 };
    Interval _10339 = jet_mul_derivative(_9818, _9819, intervalFailed, optical_product_upper);
    bool _10344;
    if (_10084 <= 0.0)
    {
        _10344 = _10085 >= 0.0;
    }
    else
    {
        _10344 = false;
    }
    float _10351;
    if (_10344)
    {
        _10351 = 0.0;
    }
    else
    {
        _10351 = precise::min(abs(_10084), abs(_10085));
    }
    float _10354 = precise::max(abs(_10084), abs(_10085));
    float _9800 = spvFMul(_10351, _10351);
    float _10355 = interval_down(_9800, intervalFailed);
    float _9801 = spvFMul(_10354, _10354);
    float _10357 = interval_up(_9801, intervalFailed);
    Interval _9802 = Interval{ 2.0, 2.0 };
    Interval _9803 = Interval{ _10084, _10085 };
    Interval _10359 = imul(_9802, _9803, intervalFailed, optical_product_upper);
    Interval _9804 = _10359;
    Interval _9805 = Interval{ _10086, _10087 };
    Interval _10361 = jet_mul_derivative(_9804, _9805, intervalFailed, optical_product_upper);
    Interval _9806 = Interval{ 2.0, 2.0 };
    Interval _9807 = Interval{ _10084, _10085 };
    Interval _10363 = imul(_9806, _9807, intervalFailed, optical_product_upper);
    Interval _9808 = _10363;
    Interval _9809 = Interval{ _10088, _10089 };
    Interval _10365 = jet_mul_derivative(_9808, _9809, intervalFailed, optical_product_upper);
    Interval _9794 = Interval{ precise::max(0.0, _10329), _10331 };
    Interval _9795 = Interval{ precise::max(0.0, _10355), _10357 };
    Interval _10368 = iadd(_9794, _9795, intervalFailed);
    Interval _9796 = _10335;
    Interval _9797 = _10361;
    Interval _10369 = jet_add_derivative(_9796, _9797, intervalFailed);
    Interval _9798 = _10339;
    Interval _9799 = _10365;
    Interval _10370 = jet_add_derivative(_9798, _9799, intervalFailed);
    bool _10375;
    if (_10090 <= 0.0)
    {
        _10375 = _10091 >= 0.0;
    }
    else
    {
        _10375 = false;
    }
    float _10382;
    if (_10375)
    {
        _10382 = 0.0;
    }
    else
    {
        _10382 = precise::min(abs(_10090), abs(_10091));
    }
    float _10385 = precise::max(abs(_10090), abs(_10091));
    float _9784 = spvFMul(_10382, _10382);
    float _10386 = interval_down(_9784, intervalFailed);
    float _9785 = spvFMul(_10385, _10385);
    float _10388 = interval_up(_9785, intervalFailed);
    Interval _9786 = Interval{ 2.0, 2.0 };
    Interval _9787 = Interval{ _10090, _10091 };
    Interval _10390 = imul(_9786, _9787, intervalFailed, optical_product_upper);
    Interval _9788 = _10390;
    Interval _9789 = Interval{ _10092, _10093 };
    Interval _10392 = jet_mul_derivative(_9788, _9789, intervalFailed, optical_product_upper);
    Interval _9790 = Interval{ 2.0, 2.0 };
    Interval _9791 = Interval{ _10090, _10091 };
    Interval _10394 = imul(_9790, _9791, intervalFailed, optical_product_upper);
    Interval _9792 = _10394;
    Interval _9793 = Interval{ _10094, _10095 };
    Interval _10396 = jet_mul_derivative(_9792, _9793, intervalFailed, optical_product_upper);
    Interval _9778 = _10368;
    Interval _9779 = Interval{ precise::max(0.0, _10386), _10388 };
    Interval _10398 = iadd(_9778, _9779, intervalFailed);
    Interval _9780 = _10369;
    Interval _9781 = _10392;
    Interval _10399 = jet_add_derivative(_9780, _9781, intervalFailed);
    Interval _9782 = _10370;
    Interval _9783 = _10396;
    Interval _10400 = jet_add_derivative(_9782, _9783, intervalFailed);
    Interval _9769 = _10398;
    Interval _10402 = isqrt(_9769, intervalFailed);
    bool _10410;
    if (!intervalFailed)
    {
        _10410 = intervalFailed;
    }
    else
    {
        _10410 = false;
    }
    bool _10415;
    if (_10410)
    {
        _10415 = jetFailureSite == 0u;
    }
    else
    {
        _10415 = false;
    }
    if (_10415)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_10398.lo, _10398.hi, 0.0, 0.0);
    }
    if (_10402.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _9770 = Interval{ 2.0, 2.0 };
    Interval _9771 = _10402;
    Interval _10422 = imul(_9770, _9771, intervalFailed, optical_product_upper);
    Interval _9772 = Interval{ 1.0, 1.0 };
    Interval _9773 = _10422;
    Interval _10424 = idiv(_9772, _9773, intervalFailed, interval_divide_upper);
    bool _10431;
    if (!intervalFailed)
    {
        _10431 = intervalFailed;
    }
    else
    {
        _10431 = false;
    }
    bool _10436;
    if (_10431)
    {
        _10436 = jetFailureSite == 0u;
    }
    else
    {
        _10436 = false;
    }
    if (_10436)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _10422.lo, _10422.hi);
    }
    Interval _9774 = _10424;
    Interval _9775 = _10399;
    Interval _10440 = jet_mul_derivative(_9774, _9775, intervalFailed, optical_product_upper);
    Interval _9776 = _10424;
    Interval _9777 = _10400;
    Interval _10441 = jet_mul_derivative(_9776, _9777, intervalFailed, optical_product_upper);
    Interval _9755 = Interval{ 1.0, 1.0 };
    Interval _9756 = _10402;
    Interval _10443 = idiv(_9755, _9756, intervalFailed, interval_divide_upper);
    bool _10450;
    if (!intervalFailed)
    {
        _10450 = intervalFailed;
    }
    else
    {
        _10450 = false;
    }
    bool _10455;
    if (_10450)
    {
        _10455 = jetFailureSite == 0u;
    }
    else
    {
        _10455 = false;
    }
    if (_10455)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _10402.lo, _10402.hi);
    }
    Interval _9757 = Interval{ 0.0, 0.0 };
    Interval _9758 = _10443;
    Interval _9759 = _10440;
    Interval _10459 = jet_mul_derivative(_9758, _9759, intervalFailed, optical_product_upper);
    Interval _9760 = Interval{ as_type<float>(as_type<uint>(_10459.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10459.lo) ^ 2147483648u) };
    Interval _10469 = jet_add_derivative(_9757, _9760, intervalFailed);
    Interval _9761 = _10469;
    Interval _9762 = _10402;
    Interval _10470 = jet_div_derivative(_9761, _9762, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _9763 = Interval{ 0.0, 0.0 };
    Interval _9764 = _10443;
    Interval _9765 = _10441;
    Interval _10471 = jet_mul_derivative(_9764, _9765, intervalFailed, optical_product_upper);
    Interval _9766 = Interval{ as_type<float>(as_type<uint>(_10471.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10471.lo) ^ 2147483648u) };
    Interval _10481 = jet_add_derivative(_9763, _9766, intervalFailed);
    Interval _9767 = _10481;
    Interval _9768 = _10402;
    Interval _10482 = jet_div_derivative(_9767, _9768, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _9741 = Interval{ _10078, _10079 };
    Interval _9742 = _10443;
    Interval _10484 = imul(_9741, _9742, intervalFailed, optical_product_upper);
    Interval _9743 = Interval{ _10080, _10081 };
    Interval _9744 = _10443;
    Interval _10486 = jet_mul_derivative(_9743, _9744, intervalFailed, optical_product_upper);
    Interval _9745 = _10486;
    Interval _9746 = Interval{ _10078, _10079 };
    Interval _9747 = _10470;
    Interval _10488 = jet_mul_derivative(_9746, _9747, intervalFailed, optical_product_upper);
    Interval _9748 = _10488;
    Interval _10489 = jet_add_derivative(_9745, _9748, intervalFailed);
    Interval _9749 = Interval{ _10082, _10083 };
    Interval _9750 = _10443;
    Interval _10491 = jet_mul_derivative(_9749, _9750, intervalFailed, optical_product_upper);
    Interval _9751 = _10491;
    Interval _9752 = Interval{ _10078, _10079 };
    Interval _9753 = _10482;
    Interval _10493 = jet_mul_derivative(_9752, _9753, intervalFailed, optical_product_upper);
    Interval _9754 = _10493;
    Interval _10494 = jet_add_derivative(_9751, _9754, intervalFailed);
    Interval _9727 = Interval{ _10084, _10085 };
    Interval _9728 = _10443;
    Interval _10496 = imul(_9727, _9728, intervalFailed, optical_product_upper);
    Interval _9729 = Interval{ _10086, _10087 };
    Interval _9730 = _10443;
    Interval _10498 = jet_mul_derivative(_9729, _9730, intervalFailed, optical_product_upper);
    Interval _9731 = _10498;
    Interval _9732 = Interval{ _10084, _10085 };
    Interval _9733 = _10470;
    Interval _10500 = jet_mul_derivative(_9732, _9733, intervalFailed, optical_product_upper);
    Interval _9734 = _10500;
    Interval _10501 = jet_add_derivative(_9731, _9734, intervalFailed);
    Interval _9735 = Interval{ _10088, _10089 };
    Interval _9736 = _10443;
    Interval _10503 = jet_mul_derivative(_9735, _9736, intervalFailed, optical_product_upper);
    Interval _9737 = _10503;
    Interval _9738 = Interval{ _10084, _10085 };
    Interval _9739 = _10482;
    Interval _10505 = jet_mul_derivative(_9738, _9739, intervalFailed, optical_product_upper);
    Interval _9740 = _10505;
    Interval _10506 = jet_add_derivative(_9737, _9740, intervalFailed);
    Interval _9713 = Interval{ _10090, _10091 };
    Interval _9714 = _10443;
    Interval _10508 = imul(_9713, _9714, intervalFailed, optical_product_upper);
    Interval _9715 = Interval{ _10092, _10093 };
    Interval _9716 = _10443;
    Interval _10510 = jet_mul_derivative(_9715, _9716, intervalFailed, optical_product_upper);
    Interval _9717 = _10510;
    Interval _9718 = Interval{ _10090, _10091 };
    Interval _9719 = _10470;
    Interval _10512 = jet_mul_derivative(_9718, _9719, intervalFailed, optical_product_upper);
    Interval _9720 = _10512;
    Interval _10513 = jet_add_derivative(_9717, _9720, intervalFailed);
    Interval _9721 = Interval{ _10094, _10095 };
    Interval _9722 = _10443;
    Interval _10515 = jet_mul_derivative(_9721, _9722, intervalFailed, optical_product_upper);
    Interval _9723 = _10515;
    Interval _9724 = Interval{ _10090, _10091 };
    Interval _9725 = _10482;
    Interval _10517 = jet_mul_derivative(_9724, _9725, intervalFailed, optical_product_upper);
    Interval _9726 = _10517;
    Interval _10518 = jet_add_derivative(_9723, _9726, intervalFailed);
    Interval _9699 = _10496;
    Interval _9700 = ray.outgoing.z.v;
    Interval _10527 = imul(_9699, _9700, intervalFailed, optical_product_upper);
    Interval _9701 = _10501;
    Interval _9702 = ray.outgoing.z.v;
    Interval _10528 = jet_mul_derivative(_9701, _9702, intervalFailed, optical_product_upper);
    Interval _9703 = _10528;
    Interval _9704 = _10496;
    Interval _9705 = ray.outgoing.z.dx;
    Interval _10529 = jet_mul_derivative(_9704, _9705, intervalFailed, optical_product_upper);
    Interval _9706 = _10529;
    Interval _10530 = jet_add_derivative(_9703, _9706, intervalFailed);
    Interval _9707 = _10506;
    Interval _9708 = ray.outgoing.z.v;
    Interval _10531 = jet_mul_derivative(_9707, _9708, intervalFailed, optical_product_upper);
    Interval _9709 = _10531;
    Interval _9710 = _10496;
    Interval _9711 = ray.outgoing.z.dy;
    Interval _10532 = jet_mul_derivative(_9710, _9711, intervalFailed, optical_product_upper);
    Interval _9712 = _10532;
    Interval _10533 = jet_add_derivative(_9709, _9712, intervalFailed);
    Interval _9685 = _10508;
    Interval _9686 = ray.outgoing.y.v;
    Interval _10537 = imul(_9685, _9686, intervalFailed, optical_product_upper);
    Interval _9687 = _10513;
    Interval _9688 = ray.outgoing.y.v;
    Interval _10538 = jet_mul_derivative(_9687, _9688, intervalFailed, optical_product_upper);
    Interval _9689 = _10538;
    Interval _9690 = _10508;
    Interval _9691 = ray.outgoing.y.dx;
    Interval _10539 = jet_mul_derivative(_9690, _9691, intervalFailed, optical_product_upper);
    Interval _9692 = _10539;
    Interval _10540 = jet_add_derivative(_9689, _9692, intervalFailed);
    Interval _9693 = _10518;
    Interval _9694 = ray.outgoing.y.v;
    Interval _10541 = jet_mul_derivative(_9693, _9694, intervalFailed, optical_product_upper);
    Interval _9695 = _10541;
    Interval _9696 = _10508;
    Interval _9697 = ray.outgoing.y.dy;
    Interval _10542 = jet_mul_derivative(_9696, _9697, intervalFailed, optical_product_upper);
    Interval _9698 = _10542;
    Interval _10543 = jet_add_derivative(_9695, _9698, intervalFailed);
    Interval _9679 = _10527;
    Interval _9680 = Interval{ as_type<float>(as_type<uint>(_10537.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10537.lo) ^ 2147483648u) };
    Interval _10569 = iadd(_9679, _9680, intervalFailed);
    Interval _9681 = _10530;
    Interval _9682 = Interval{ as_type<float>(as_type<uint>(_10540.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10540.lo) ^ 2147483648u) };
    Interval _10571 = jet_add_derivative(_9681, _9682, intervalFailed);
    Interval _9683 = _10533;
    Interval _9684 = Interval{ as_type<float>(as_type<uint>(_10543.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10543.lo) ^ 2147483648u) };
    Interval _10573 = jet_add_derivative(_9683, _9684, intervalFailed);
    Interval _9665 = _10508;
    Interval _9666 = ray.outgoing.x.v;
    Interval _10577 = imul(_9665, _9666, intervalFailed, optical_product_upper);
    Interval _9667 = _10513;
    Interval _9668 = ray.outgoing.x.v;
    Interval _10578 = jet_mul_derivative(_9667, _9668, intervalFailed, optical_product_upper);
    Interval _9669 = _10578;
    Interval _9670 = _10508;
    Interval _9671 = ray.outgoing.x.dx;
    Interval _10579 = jet_mul_derivative(_9670, _9671, intervalFailed, optical_product_upper);
    Interval _9672 = _10579;
    Interval _10580 = jet_add_derivative(_9669, _9672, intervalFailed);
    Interval _9673 = _10518;
    Interval _9674 = ray.outgoing.x.v;
    Interval _10581 = jet_mul_derivative(_9673, _9674, intervalFailed, optical_product_upper);
    Interval _9675 = _10581;
    Interval _9676 = _10508;
    Interval _9677 = ray.outgoing.x.dy;
    Interval _10582 = jet_mul_derivative(_9676, _9677, intervalFailed, optical_product_upper);
    Interval _9678 = _10582;
    Interval _10583 = jet_add_derivative(_9675, _9678, intervalFailed);
    Interval _9651 = _10484;
    Interval _9652 = ray.outgoing.z.v;
    Interval _10587 = imul(_9651, _9652, intervalFailed, optical_product_upper);
    Interval _9653 = _10489;
    Interval _9654 = ray.outgoing.z.v;
    Interval _10588 = jet_mul_derivative(_9653, _9654, intervalFailed, optical_product_upper);
    Interval _9655 = _10588;
    Interval _9656 = _10484;
    Interval _9657 = ray.outgoing.z.dx;
    Interval _10589 = jet_mul_derivative(_9656, _9657, intervalFailed, optical_product_upper);
    Interval _9658 = _10589;
    Interval _10590 = jet_add_derivative(_9655, _9658, intervalFailed);
    Interval _9659 = _10494;
    Interval _9660 = ray.outgoing.z.v;
    Interval _10591 = jet_mul_derivative(_9659, _9660, intervalFailed, optical_product_upper);
    Interval _9661 = _10591;
    Interval _9662 = _10484;
    Interval _9663 = ray.outgoing.z.dy;
    Interval _10592 = jet_mul_derivative(_9662, _9663, intervalFailed, optical_product_upper);
    Interval _9664 = _10592;
    Interval _10593 = jet_add_derivative(_9661, _9664, intervalFailed);
    Interval _9645 = _10577;
    Interval _9646 = Interval{ as_type<float>(as_type<uint>(_10587.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10587.lo) ^ 2147483648u) };
    Interval _10619 = iadd(_9645, _9646, intervalFailed);
    Interval _9647 = _10580;
    Interval _9648 = Interval{ as_type<float>(as_type<uint>(_10590.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10590.lo) ^ 2147483648u) };
    Interval _10621 = jet_add_derivative(_9647, _9648, intervalFailed);
    Interval _9649 = _10583;
    Interval _9650 = Interval{ as_type<float>(as_type<uint>(_10593.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10593.lo) ^ 2147483648u) };
    Interval _10623 = jet_add_derivative(_9649, _9650, intervalFailed);
    Interval _9631 = _10484;
    Interval _9632 = ray.outgoing.y.v;
    Interval _10627 = imul(_9631, _9632, intervalFailed, optical_product_upper);
    Interval _9633 = _10489;
    Interval _9634 = ray.outgoing.y.v;
    Interval _10628 = jet_mul_derivative(_9633, _9634, intervalFailed, optical_product_upper);
    Interval _9635 = _10628;
    Interval _9636 = _10484;
    Interval _9637 = ray.outgoing.y.dx;
    Interval _10629 = jet_mul_derivative(_9636, _9637, intervalFailed, optical_product_upper);
    Interval _9638 = _10629;
    Interval _10630 = jet_add_derivative(_9635, _9638, intervalFailed);
    Interval _9639 = _10494;
    Interval _9640 = ray.outgoing.y.v;
    Interval _10631 = jet_mul_derivative(_9639, _9640, intervalFailed, optical_product_upper);
    Interval _9641 = _10631;
    Interval _9642 = _10484;
    Interval _9643 = ray.outgoing.y.dy;
    Interval _10632 = jet_mul_derivative(_9642, _9643, intervalFailed, optical_product_upper);
    Interval _9644 = _10632;
    Interval _10633 = jet_add_derivative(_9641, _9644, intervalFailed);
    Interval _9617 = _10496;
    Interval _9618 = ray.outgoing.x.v;
    Interval _10637 = imul(_9617, _9618, intervalFailed, optical_product_upper);
    Interval _9619 = _10501;
    Interval _9620 = ray.outgoing.x.v;
    Interval _10638 = jet_mul_derivative(_9619, _9620, intervalFailed, optical_product_upper);
    Interval _9621 = _10638;
    Interval _9622 = _10496;
    Interval _9623 = ray.outgoing.x.dx;
    Interval _10639 = jet_mul_derivative(_9622, _9623, intervalFailed, optical_product_upper);
    Interval _9624 = _10639;
    Interval _10640 = jet_add_derivative(_9621, _9624, intervalFailed);
    Interval _9625 = _10506;
    Interval _9626 = ray.outgoing.x.v;
    Interval _10641 = jet_mul_derivative(_9625, _9626, intervalFailed, optical_product_upper);
    Interval _9627 = _10641;
    Interval _9628 = _10496;
    Interval _9629 = ray.outgoing.x.dy;
    Interval _10642 = jet_mul_derivative(_9628, _9629, intervalFailed, optical_product_upper);
    Interval _9630 = _10642;
    Interval _10643 = jet_add_derivative(_9627, _9630, intervalFailed);
    Interval _9611 = _10627;
    Interval _9612 = Interval{ as_type<float>(as_type<uint>(_10637.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10637.lo) ^ 2147483648u) };
    Interval _10669 = iadd(_9611, _9612, intervalFailed);
    Interval _9613 = _10630;
    Interval _9614 = Interval{ as_type<float>(as_type<uint>(_10640.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10640.lo) ^ 2147483648u) };
    Interval _10671 = jet_add_derivative(_9613, _9614, intervalFailed);
    Interval _9615 = _10633;
    Interval _9616 = Interval{ as_type<float>(as_type<uint>(_10643.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10643.lo) ^ 2147483648u) };
    Interval _10673 = jet_add_derivative(_9615, _9616, intervalFailed);
    Interval _9597 = _10569;
    Interval _9598 = Interval{ focal, focal };
    Interval _10676 = imul(_9597, _9598, intervalFailed, optical_product_upper);
    Interval _9599 = _10571;
    Interval _9600 = Interval{ focal, focal };
    Interval _10678 = jet_mul_derivative(_9599, _9600, intervalFailed, optical_product_upper);
    Interval _9601 = _10678;
    Interval _9602 = _10569;
    Interval _9603 = Interval{ 0.0, 0.0 };
    Interval _10679 = jet_mul_derivative(_9602, _9603, intervalFailed, optical_product_upper);
    Interval _9604 = _10679;
    Interval _10680 = jet_add_derivative(_9601, _9604, intervalFailed);
    Interval _9605 = _10573;
    Interval _9606 = Interval{ focal, focal };
    Interval _10682 = jet_mul_derivative(_9605, _9606, intervalFailed, optical_product_upper);
    Interval _9607 = _10682;
    Interval _9608 = _10569;
    Interval _9609 = Interval{ 0.0, 0.0 };
    Interval _10683 = jet_mul_derivative(_9608, _9609, intervalFailed, optical_product_upper);
    Interval _9610 = _10683;
    Interval _10684 = jet_add_derivative(_9607, _9610, intervalFailed);
    Interval _9583 = _10619;
    Interval _9584 = Interval{ focal, focal };
    Interval _10686 = imul(_9583, _9584, intervalFailed, optical_product_upper);
    Interval _9585 = _10621;
    Interval _9586 = Interval{ focal, focal };
    Interval _10688 = jet_mul_derivative(_9585, _9586, intervalFailed, optical_product_upper);
    Interval _9587 = _10688;
    Interval _9588 = _10619;
    Interval _9589 = Interval{ 0.0, 0.0 };
    Interval _10689 = jet_mul_derivative(_9588, _9589, intervalFailed, optical_product_upper);
    Interval _9590 = _10689;
    Interval _10690 = jet_add_derivative(_9587, _9590, intervalFailed);
    Interval _9591 = _10623;
    Interval _9592 = Interval{ focal, focal };
    Interval _10692 = jet_mul_derivative(_9591, _9592, intervalFailed, optical_product_upper);
    Interval _9593 = _10692;
    Interval _9594 = _10619;
    Interval _9595 = Interval{ 0.0, 0.0 };
    Interval _10693 = jet_mul_derivative(_9594, _9595, intervalFailed, optical_product_upper);
    Interval _9596 = _10693;
    Interval _10694 = jet_add_derivative(_9593, _9596, intervalFailed);
    Interval _9569 = _10669;
    Interval _9570 = Interval{ focal, focal };
    Interval _10696 = imul(_9569, _9570, intervalFailed, optical_product_upper);
    Interval _9571 = _10671;
    Interval _9572 = Interval{ focal, focal };
    Interval _10698 = jet_mul_derivative(_9571, _9572, intervalFailed, optical_product_upper);
    Interval _9573 = _10698;
    Interval _9574 = _10669;
    Interval _9575 = Interval{ 0.0, 0.0 };
    Interval _10699 = jet_mul_derivative(_9574, _9575, intervalFailed, optical_product_upper);
    Interval _9576 = _10699;
    Interval _10700 = jet_add_derivative(_9573, _9576, intervalFailed);
    Interval _9577 = _10673;
    Interval _9578 = Interval{ focal, focal };
    Interval _10702 = jet_mul_derivative(_9577, _9578, intervalFailed, optical_product_upper);
    Interval _9579 = _10702;
    Interval _9580 = _10669;
    Interval _9581 = Interval{ 0.0, 0.0 };
    Interval _10703 = jet_mul_derivative(_9580, _9581, intervalFailed, optical_product_upper);
    Interval _9582 = _10703;
    Interval _10704 = jet_add_derivative(_9579, _9582, intervalFailed);
    if (omitted == 0u)
    {
        e0 = OpticalJet{ _10686, _10690, _10694 };
        e1 = OpticalJet{ _10696, _10700, _10704 };
    }
    else
    {
        if (omitted == 1u)
        {
            e0 = OpticalJet{ _10696, _10700, _10704 };
            e1 = OpticalJet{ _10676, _10680, _10684 };
        }
        else
        {
            e0 = OpticalJet{ _10676, _10680, _10684 };
            e1 = OpticalJet{ _10686, _10690, _10694 };
        }
    }
    bool _10718;
    if (!intervalFailed)
    {
        _10718 = jetBranchKnown;
    }
    else
    {
        _10718 = false;
    }
    return _10718;
}

static inline __attribute__((always_inline))
OpticalLocalRoot optical_local_root(thread const float4& box, thread const float2& centre, thread const Interval& f0, thread const Interval& f1, thread const Interval& j00, thread const Interval& j01, thread const Interval& j10, thread const Interval& j11, thread bool& intervalFailed, thread float& optical_product_upper, thread bool& jetBranchKnown)
{
    bool _10737;
    if (!intervalFailed)
    {
        _10737 = !jetBranchKnown;
    }
    else
    {
        _10737 = true;
    }
    bool _10746;
    if (!_10737)
    {
        bool4 _10740 = isnan(box);
        bool4 _10741 = isinf(box);
        _10746 = !all(not(bool4(_10740.x || _10741.x, _10740.y || _10741.y, _10740.z || _10741.z, _10740.w || _10741.w)));
    }
    else
    {
        _10746 = true;
    }
    bool _10755;
    if (!_10746)
    {
        bool2 _10749 = isnan(centre);
        bool2 _10750 = isinf(centre);
        _10755 = !all(not(bool2(_10749.x || _10750.x, _10749.y || _10750.y)));
    }
    else
    {
        _10755 = true;
    }
    bool _10763;
    if (!_10755)
    {
        _10763 = any(box.xy >= box.zw);
    }
    else
    {
        _10763 = true;
    }
    bool _10770;
    if (!_10763)
    {
        _10770 = any(centre <= box.xy);
    }
    else
    {
        _10770 = true;
    }
    bool _10777;
    if (!_10770)
    {
        _10777 = any(centre >= box.zw);
    }
    else
    {
        _10777 = true;
    }
    bool _10798;
    if (!_10777)
    {
        bool _10792;
        if (!(isnan(f0.lo) || isinf(f0.lo)))
        {
            _10792 = !(isnan(f0.hi) || isinf(f0.hi));
        }
        else
        {
            _10792 = false;
        }
        bool _10796;
        if (_10792)
        {
            _10796 = f0.lo <= f0.hi;
        }
        else
        {
            _10796 = false;
        }
        _10798 = !_10796;
    }
    else
    {
        _10798 = true;
    }
    bool _10819;
    if (!_10798)
    {
        bool _10813;
        if (!(isnan(f1.lo) || isinf(f1.lo)))
        {
            _10813 = !(isnan(f1.hi) || isinf(f1.hi));
        }
        else
        {
            _10813 = false;
        }
        bool _10817;
        if (_10813)
        {
            _10817 = f1.lo <= f1.hi;
        }
        else
        {
            _10817 = false;
        }
        _10819 = !_10817;
    }
    else
    {
        _10819 = true;
    }
    bool _10840;
    if (!_10819)
    {
        bool _10834;
        if (!(isnan(j00.lo) || isinf(j00.lo)))
        {
            _10834 = !(isnan(j00.hi) || isinf(j00.hi));
        }
        else
        {
            _10834 = false;
        }
        bool _10838;
        if (_10834)
        {
            _10838 = j00.lo <= j00.hi;
        }
        else
        {
            _10838 = false;
        }
        _10840 = !_10838;
    }
    else
    {
        _10840 = true;
    }
    bool _10861;
    if (!_10840)
    {
        bool _10855;
        if (!(isnan(j01.lo) || isinf(j01.lo)))
        {
            _10855 = !(isnan(j01.hi) || isinf(j01.hi));
        }
        else
        {
            _10855 = false;
        }
        bool _10859;
        if (_10855)
        {
            _10859 = j01.lo <= j01.hi;
        }
        else
        {
            _10859 = false;
        }
        _10861 = !_10859;
    }
    else
    {
        _10861 = true;
    }
    bool _10882;
    if (!_10861)
    {
        bool _10876;
        if (!(isnan(j10.lo) || isinf(j10.lo)))
        {
            _10876 = !(isnan(j10.hi) || isinf(j10.hi));
        }
        else
        {
            _10876 = false;
        }
        bool _10880;
        if (_10876)
        {
            _10880 = j10.lo <= j10.hi;
        }
        else
        {
            _10880 = false;
        }
        _10882 = !_10880;
    }
    else
    {
        _10882 = true;
    }
    bool _10903;
    if (!_10882)
    {
        bool _10897;
        if (!(isnan(j11.lo) || isinf(j11.lo)))
        {
            _10897 = !(isnan(j11.hi) || isinf(j11.hi));
        }
        else
        {
            _10897 = false;
        }
        bool _10901;
        if (_10897)
        {
            _10901 = j11.lo <= j11.hi;
        }
        else
        {
            _10901 = false;
        }
        _10903 = !_10901;
    }
    else
    {
        _10903 = true;
    }
    if (_10903)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float _1685 = spvFMul(spvFAdd(j00.lo, j00.hi), 0.5);
    float _1687 = spvFMul(spvFAdd(j01.lo, j01.hi), 0.5);
    float _1689 = spvFMul(spvFAdd(j10.lo, j10.hi), 0.5);
    float _1691 = spvFMul(spvFAdd(j11.lo, j11.hi), 0.5);
    float4 _10920 = float4(_1685, _1687, _1689, _1691);
    float _1694 = spvFSub(spvFMul(_1685, _1691), spvFMul(_1687, _1689));
    bool4 _10921 = isnan(_10920);
    bool4 _10922 = isinf(_10920);
    bool _10929;
    if (all(not(bool4(_10921.x || _10922.x, _10921.y || _10922.y, _10921.z || _10922.z, _10921.w || _10922.w))))
    {
        _10929 = isnan(_1694) || isinf(_1694);
    }
    else
    {
        _10929 = true;
    }
    bool _10932;
    if (!_10929)
    {
        _10932 = _1694 == 0.0;
    }
    else
    {
        _10932 = true;
    }
    if (_10932)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float4 _1697 = float4(_1691, -_1687, -_1689, _1685) / float4(_1694);
    bool4 _10935 = isnan(_1697);
    bool4 _10936 = isinf(_1697);
    bool _10967;
    if (all(not(bool4(_10935.x || _10936.x, _10935.y || _10936.y, _10935.z || _10936.z, _10935.w || _10936.w))))
    {
        float _10940 = _1697.x;
        Interval param_var_a = Interval{ _10940, _10940 };
        float _10942 = _1697.w;
        Interval param_var_b = Interval{ _10942, _10942 };
        Interval _10944 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        float _10945 = _1697.y;
        Interval param_var_a_1 = Interval{ _10945, _10945 };
        float _10947 = _1697.z;
        Interval param_var_b_1 = Interval{ _10947, _10947 };
        Interval _10949 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        Interval _10731 = _10944;
        Interval _10732 = Interval{ as_type<float>(as_type<uint>(_10949.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10949.lo) ^ 2147483648u) };
        Interval _10959 = iadd(_10731, _10732, intervalFailed);
        bool _10966;
        if (_10959.lo <= 0.0)
        {
            _10966 = _10959.hi >= 0.0;
        }
        else
        {
            _10966 = false;
        }
        _10967 = _10966;
    }
    else
    {
        _10967 = true;
    }
    if (_10967)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float _10968 = _1697.x;
    Interval param_var_a_2 = Interval{ _10968, _10968 };
    Interval param_var_b_2 = j00;
    Interval _10971 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval param_var_a_3 = _10971;
    float _10972 = _1697.y;
    Interval param_var_a_4 = Interval{ _10972, _10972 };
    Interval param_var_b_3 = j10;
    Interval _10975 = imul(param_var_a_4, param_var_b_3, intervalFailed, optical_product_upper);
    Interval param_var_b_4 = _10975;
    Interval _10976 = iadd(param_var_a_3, param_var_b_4, intervalFailed);
    Interval _10729 = Interval{ 1.0, 1.0 };
    Interval _10730 = Interval{ as_type<float>(as_type<uint>(_10976.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10976.lo) ^ 2147483648u) };
    Interval _10986 = iadd(_10729, _10730, intervalFailed);
    float _10987 = _1697.x;
    Interval param_var_a_5 = Interval{ _10987, _10987 };
    Interval param_var_b_5 = j01;
    Interval _10990 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
    Interval param_var_a_6 = _10990;
    float _10991 = _1697.y;
    Interval param_var_a_7 = Interval{ _10991, _10991 };
    Interval param_var_b_6 = j11;
    Interval _10994 = imul(param_var_a_7, param_var_b_6, intervalFailed, optical_product_upper);
    Interval param_var_b_7 = _10994;
    Interval _10995 = iadd(param_var_a_6, param_var_b_7, intervalFailed);
    float _11000 = as_type<float>(as_type<uint>(_10995.hi) ^ 2147483648u);
    float _11003 = as_type<float>(as_type<uint>(_10995.lo) ^ 2147483648u);
    float _11004 = _1697.z;
    Interval param_var_a_8 = Interval{ _11004, _11004 };
    Interval param_var_b_8 = j00;
    Interval _11007 = imul(param_var_a_8, param_var_b_8, intervalFailed, optical_product_upper);
    Interval param_var_a_9 = _11007;
    float _11008 = _1697.w;
    Interval param_var_a_10 = Interval{ _11008, _11008 };
    Interval param_var_b_9 = j10;
    Interval _11011 = imul(param_var_a_10, param_var_b_9, intervalFailed, optical_product_upper);
    Interval param_var_b_10 = _11011;
    Interval _11012 = iadd(param_var_a_9, param_var_b_10, intervalFailed);
    float _11017 = as_type<float>(as_type<uint>(_11012.hi) ^ 2147483648u);
    float _11020 = as_type<float>(as_type<uint>(_11012.lo) ^ 2147483648u);
    float _11021 = _1697.z;
    Interval param_var_a_11 = Interval{ _11021, _11021 };
    Interval param_var_b_11 = j01;
    Interval _11024 = imul(param_var_a_11, param_var_b_11, intervalFailed, optical_product_upper);
    Interval param_var_a_12 = _11024;
    float _11025 = _1697.w;
    Interval param_var_a_13 = Interval{ _11025, _11025 };
    Interval param_var_b_12 = j11;
    Interval _11028 = imul(param_var_a_13, param_var_b_12, intervalFailed, optical_product_upper);
    Interval param_var_b_13 = _11028;
    Interval _11029 = iadd(param_var_a_12, param_var_b_13, intervalFailed);
    Interval _10727 = Interval{ 1.0, 1.0 };
    Interval _10728 = Interval{ as_type<float>(as_type<uint>(_11029.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11029.lo) ^ 2147483648u) };
    Interval _11039 = iadd(_10727, _10728, intervalFailed);
    Interval _10725 = Interval{ box.x, box.z };
    Interval _10726 = Interval{ as_type<float>(as_type<uint>(centre.x) ^ 2147483648u), as_type<float>(as_type<uint>(centre.x) ^ 2147483648u) };
    Interval _11054 = iadd(_10725, _10726, intervalFailed);
    Interval _10723 = Interval{ box.y, box.w };
    Interval _10724 = Interval{ as_type<float>(as_type<uint>(centre.y) ^ 2147483648u), as_type<float>(as_type<uint>(centre.y) ^ 2147483648u) };
    Interval _11069 = iadd(_10723, _10724, intervalFailed);
    float _11072 = _1697.x;
    Interval param_var_a_14 = Interval{ _11072, _11072 };
    Interval param_var_b_14 = f0;
    Interval _11075 = imul(param_var_a_14, param_var_b_14, intervalFailed, optical_product_upper);
    Interval param_var_a_15 = _11075;
    float _11076 = _1697.y;
    Interval param_var_a_16 = Interval{ _11076, _11076 };
    Interval param_var_b_15 = f1;
    Interval _11079 = imul(param_var_a_16, param_var_b_15, intervalFailed, optical_product_upper);
    Interval param_var_b_16 = _11079;
    Interval _11080 = iadd(param_var_a_15, param_var_b_16, intervalFailed);
    Interval _10721 = Interval{ centre.x, centre.x };
    Interval _10722 = Interval{ as_type<float>(as_type<uint>(_11080.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11080.lo) ^ 2147483648u) };
    Interval _11091 = iadd(_10721, _10722, intervalFailed);
    float _11094 = _1697.z;
    Interval param_var_a_17 = Interval{ _11094, _11094 };
    Interval param_var_b_17 = f0;
    Interval _11097 = imul(param_var_a_17, param_var_b_17, intervalFailed, optical_product_upper);
    Interval param_var_a_18 = _11097;
    float _11098 = _1697.w;
    Interval param_var_a_19 = Interval{ _11098, _11098 };
    Interval param_var_b_18 = f1;
    Interval _11101 = imul(param_var_a_19, param_var_b_18, intervalFailed, optical_product_upper);
    Interval param_var_b_19 = _11101;
    Interval _11102 = iadd(param_var_a_18, param_var_b_19, intervalFailed);
    Interval _10719 = Interval{ centre.y, centre.y };
    Interval _10720 = Interval{ as_type<float>(as_type<uint>(_11102.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11102.lo) ^ 2147483648u) };
    Interval _11113 = iadd(_10719, _10720, intervalFailed);
    Interval param_var_a_20 = _11091;
    Interval param_var_a_21 = _10986;
    Interval param_var_b_20 = _11054;
    Interval _11114 = imul(param_var_a_21, param_var_b_20, intervalFailed, optical_product_upper);
    Interval param_var_a_22 = _11114;
    Interval param_var_a_23 = Interval{ _11000, _11003 };
    Interval param_var_b_21 = _11069;
    Interval _11116 = imul(param_var_a_23, param_var_b_21, intervalFailed, optical_product_upper);
    Interval param_var_b_22 = _11116;
    Interval _11117 = iadd(param_var_a_22, param_var_b_22, intervalFailed);
    Interval param_var_b_23 = _11117;
    Interval _11118 = iadd(param_var_a_20, param_var_b_23, intervalFailed);
    Interval param_var_a_24 = _11113;
    Interval param_var_a_25 = Interval{ _11017, _11020 };
    Interval param_var_b_24 = _11054;
    Interval _11122 = imul(param_var_a_25, param_var_b_24, intervalFailed, optical_product_upper);
    Interval param_var_a_26 = _11122;
    Interval param_var_a_27 = _11039;
    Interval param_var_b_25 = _11069;
    Interval _11123 = imul(param_var_a_27, param_var_b_25, intervalFailed, optical_product_upper);
    Interval param_var_b_26 = _11123;
    Interval _11124 = iadd(param_var_a_26, param_var_b_26, intervalFailed);
    Interval param_var_b_27 = _11124;
    Interval _11125 = iadd(param_var_a_24, param_var_b_27, intervalFailed);
    bool _11134;
    if (_10986.lo <= 0.0)
    {
        _11134 = _10986.hi >= 0.0;
    }
    else
    {
        _11134 = false;
    }
    float _11141;
    if (_11134)
    {
        _11141 = 0.0;
    }
    else
    {
        _11141 = precise::min(abs(_10986.lo), abs(_10986.hi));
    }
    Interval param_var_a_28 = Interval{ _11141, precise::max(abs(_10986.lo), abs(_10986.hi)) };
    bool _11150;
    if (_11000 <= 0.0)
    {
        _11150 = _11003 >= 0.0;
    }
    else
    {
        _11150 = false;
    }
    float _11157;
    if (_11150)
    {
        _11157 = 0.0;
    }
    else
    {
        _11157 = precise::min(abs(_11000), abs(_11003));
    }
    Interval param_var_b_28 = Interval{ _11157, precise::max(abs(_11000), abs(_11003)) };
    Interval _11162 = iadd(param_var_a_28, param_var_b_28, intervalFailed);
    bool _11169;
    if (_11017 <= 0.0)
    {
        _11169 = _11020 >= 0.0;
    }
    else
    {
        _11169 = false;
    }
    float _11176;
    if (_11169)
    {
        _11176 = 0.0;
    }
    else
    {
        _11176 = precise::min(abs(_11017), abs(_11020));
    }
    Interval param_var_a_29 = Interval{ _11176, precise::max(abs(_11017), abs(_11020)) };
    bool _11187;
    if (_11039.lo <= 0.0)
    {
        _11187 = _11039.hi >= 0.0;
    }
    else
    {
        _11187 = false;
    }
    float _11194;
    if (_11187)
    {
        _11194 = 0.0;
    }
    else
    {
        _11194 = precise::min(abs(_11039.lo), abs(_11039.hi));
    }
    Interval param_var_b_29 = Interval{ _11194, precise::max(abs(_11039.lo), abs(_11039.hi)) };
    Interval _11199 = iadd(param_var_a_29, param_var_b_29, intervalFailed);
    float _11202 = precise::max(_11162.lo, _11199.lo);
    float _11203 = precise::max(_11162.hi, _11199.hi);
    bool _11208;
    if (!intervalFailed)
    {
        _11208 = !jetBranchKnown;
    }
    else
    {
        _11208 = true;
    }
    bool _11228;
    if (!_11208)
    {
        bool _11222;
        if (!(isnan(_11118.lo) || isinf(_11118.lo)))
        {
            _11222 = !(isnan(_11118.hi) || isinf(_11118.hi));
        }
        else
        {
            _11222 = false;
        }
        bool _11226;
        if (_11222)
        {
            _11226 = _11118.lo <= _11118.hi;
        }
        else
        {
            _11226 = false;
        }
        _11228 = !_11226;
    }
    else
    {
        _11228 = true;
    }
    bool _11248;
    if (!_11228)
    {
        bool _11242;
        if (!(isnan(_11125.lo) || isinf(_11125.lo)))
        {
            _11242 = !(isnan(_11125.hi) || isinf(_11125.hi));
        }
        else
        {
            _11242 = false;
        }
        bool _11246;
        if (_11242)
        {
            _11246 = _11125.lo <= _11125.hi;
        }
        else
        {
            _11246 = false;
        }
        _11248 = !_11246;
    }
    else
    {
        _11248 = true;
    }
    bool _11266;
    if (!_11248)
    {
        bool _11260;
        if (!(isnan(_11202) || isinf(_11202)))
        {
            _11260 = !(isnan(_11203) || isinf(_11203));
        }
        else
        {
            _11260 = false;
        }
        bool _11264;
        if (_11260)
        {
            _11264 = _11202 <= _11203;
        }
        else
        {
            _11264 = false;
        }
        _11266 = !_11264;
    }
    else
    {
        _11266 = true;
    }
    if (_11266)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    bool _11274;
    if ((isunordered(_11118.hi, box.x) || _11118.hi >= box.x))
    {
        _11274 = _11118.lo > box.z;
    }
    else
    {
        _11274 = true;
    }
    bool _11279;
    if (!_11274)
    {
        _11279 = _11125.hi < box.y;
    }
    else
    {
        _11279 = true;
    }
    bool _11284;
    if (!_11279)
    {
        _11284 = _11125.lo > box.w;
    }
    else
    {
        _11284 = true;
    }
    uint _11303;
    if (_11284)
    {
        _11303 = 2u;
    }
    else
    {
        bool _11289;
        if (_11203 < 1.0)
        {
            _11289 = _11118.lo > box.x;
        }
        else
        {
            _11289 = false;
        }
        bool _11293;
        if (_11289)
        {
            _11293 = _11118.hi < box.z;
        }
        else
        {
            _11293 = false;
        }
        bool _11297;
        if (_11293)
        {
            _11297 = _11125.lo > box.y;
        }
        else
        {
            _11297 = false;
        }
        bool _11301;
        if (_11297)
        {
            _11301 = _11125.hi < box.w;
        }
        else
        {
            _11301 = false;
        }
        uint _11302;
        if (_11301)
        {
            _11302 = 1u;
        }
        else
        {
            _11302 = 0u;
        }
        _11303 = _11302;
    }
    return OpticalLocalRoot{ _11303, float4(_11118.lo, _11125.lo, _11118.hi, _11125.hi), _11203, short(true) };
}

static inline __attribute__((always_inline))
bool optical_intersect_inherited_root(thread OpticalLocalRoot& proved, thread const OpticalLocalRoot& next, thread bool& intervalFailed, thread bool& jetBranchKnown)
{
    bool _11311;
    if (proved.status == 1u)
    {
        _11311 = !bool(proved.evaluated);
    }
    else
    {
        _11311 = true;
    }
    bool _11316;
    if (!_11311)
    {
        _11316 = !bool(next.evaluated);
    }
    else
    {
        _11316 = true;
    }
    bool _11321;
    if (!_11316)
    {
        _11321 = next.status == 2u;
    }
    else
    {
        _11321 = true;
    }
    bool _11324;
    if (!_11321)
    {
        _11324 = intervalFailed;
    }
    else
    {
        _11324 = true;
    }
    bool _11328;
    if (!_11324)
    {
        _11328 = !jetBranchKnown;
    }
    else
    {
        _11328 = true;
    }
    if (_11328)
    {
        return false;
    }
    float4 _11347 = float4(precise::max(proved.enclosure.xy, next.enclosure.xy), precise::min(proved.enclosure.zw, next.enclosure.zw));
    bool4 _11348 = isnan(_11347);
    bool4 _11349 = isinf(_11347);
    bool _11357;
    if (all(not(bool4(_11348.x || _11349.x, _11348.y || _11349.y, _11348.z || _11349.z, _11348.w || _11349.w))))
    {
        _11357 = any(_11347.xy > _11347.zw);
    }
    else
    {
        _11357 = true;
    }
    if (_11357)
    {
        return false;
    }
    proved.enclosure = _11347;
    return true;
}

static inline __attribute__((always_inline))
void write_optical_local_root(thread const uint& outAt, thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread const Interval3& target, thread const bool& finiteTarget, thread const uint& refinements, device type_RWByteAddressBuffer& results, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    float2 _1631 = spvFAdd(box.xy, box.zw) * 0.5;
    OpticalJet a = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    OpticalJet b = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    OpticalLocalRoot root;
    root.status = 0u;
    root.enclosure = float4(0.0);
    root.contraction = 1000000015047466219876688855040.0;
    root.evaluated = short(false);
    bool _4019 = min(refinements, 2u) == 2u;
    float _4055;
    float _4057;
    float _4059;
    float _4061;
    _4055 = 0.0;
    _4057 = 0.0;
    _4059 = 0.0;
    _4061 = 0.0;
    OpticalJetRay ray;
    float _4021;
    float _4023;
    float _4025;
    float _4027;
    float _4029;
    float _4031;
    float _4033;
    float _4035;
    float _4037;
    float _4039;
    float _4041;
    float _4043;
    float _4045;
    float _4047;
    float _4049;
    float _4051;
    uint _4053;
    float _4056;
    float _4058;
    float _4060;
    float _4062;
    uint _4400;
    float _4020 = 0.0;
    float _4022 = 0.0;
    float _4024 = 0.0;
    float _4026 = 0.0;
    float _4028 = 0.0;
    float _4030 = 0.0;
    float _4032 = 0.0;
    float _4034 = 0.0;
    float _4036 = 0.0;
    float _4038 = 0.0;
    float _4040 = 0.0;
    float _4042 = 0.0;
    float _4044 = 0.0;
    float _4046 = 0.0;
    float _4048 = 0.0;
    float _4050 = 0.0;
    uint _4052 = 0u;
    uint _4054 = 0u;
    for (;;)
    {
        if (_4054 < (2u + min(refinements, 2u)))
        {
            bool _4070;
            if (_4054 >= 2u)
            {
                _4070 = root.status != 1u;
            }
            else
            {
                _4070 = false;
            }
            if (_4070)
            {
                _4400 = _4052;
                break;
            }
            float4 _4072 = root.enclosure;
            float2 _4076;
            if (_4054 >= 2u)
            {
                _4076 = spvFAdd(_4072.xy, _4072.zw) * 0.5;
            }
            else
            {
                _4076 = _1631;
            }
            intervalFailed = false;
            jetBranchKnown = true;
            float4 _4087;
            if (_4054 == 0u)
            {
                _4087 = box;
            }
            else
            {
                bool _4079;
                if (_4054 == 2u)
                {
                    _4079 = _4019;
                }
                else
                {
                    _4079 = false;
                }
                float4 _4085;
                if (_4079)
                {
                    _4085 = _4072;
                }
                else
                {
                    _4085 = float4(_4076.xyxy);
                }
                _4087 = _4085;
            }
            float4 param_var_box = _4087;
            ReflectionRoughFrame param_var_receiver = receiver;
            ReflectionLiquidFrame param_var_liquid = liquid;
            spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
            uint4 param_var_control = control;
            bool _4092 = optical_jet_forward(param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, ray, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (!_4092)
            {
                if (_4054 >= 2u)
                {
                    _4400 = _4052;
                    break;
                }
                results._m0[(outAt + 12u) >> 2u] = (intervalFailed ? 9u : 8u) + ((!jetBranchKnown) ? 2u : 0u);
                return;
            }
            if (_4054 == 0u)
            {
                float3 midpoint = float3(spvFMul(spvFAdd(ray.outgoing.x.v.lo, ray.outgoing.x.v.hi), 0.5), spvFMul(spvFAdd(ray.outgoing.y.v.lo, ray.outgoing.y.v.hi), 0.5), spvFMul(spvFAdd(ray.outgoing.z.v.lo, ray.outgoing.z.v.hi), 0.5));
                int _4122;
                if (abs(midpoint.x) > abs(midpoint.y))
                {
                    _4122 = 0;
                }
                else
                {
                    _4122 = 1;
                }
                uint _4123 = uint(_4122);
                uint _4131;
                if (abs(midpoint.z) > abs(midpoint[_4123]))
                {
                    _4131 = 2u;
                }
                else
                {
                    _4131 = _4123;
                }
                uint _4137 = (outAt + 32u) >> 2u;
                uint2 _4139 = as_type<uint2>(float2(ray.origin.x.v.lo, ray.origin.x.v.hi));
                results._m0[_4137] = _4139.x;
                results._m0[_4137 + 1u] = _4139.y;
                uint _4149 = (outAt + 40u) >> 2u;
                uint2 _4151 = as_type<uint2>(float2(ray.origin.y.v.lo, ray.origin.y.v.hi));
                results._m0[_4149] = _4151.x;
                results._m0[_4149 + 1u] = _4151.y;
                uint _4161 = (outAt + 48u) >> 2u;
                uint2 _4163 = as_type<uint2>(float2(ray.origin.z.v.lo, ray.origin.z.v.hi));
                results._m0[_4161] = _4163.x;
                results._m0[_4161 + 1u] = _4163.y;
                uint _4173 = (outAt + 56u) >> 2u;
                uint2 _4175 = as_type<uint2>(float2(ray.outgoing.x.v.lo, ray.outgoing.x.v.hi));
                results._m0[_4173] = _4175.x;
                results._m0[_4173 + 1u] = _4175.y;
                uint _4185 = (outAt + 64u) >> 2u;
                uint2 _4187 = as_type<uint2>(float2(ray.outgoing.y.v.lo, ray.outgoing.y.v.hi));
                results._m0[_4185] = _4187.x;
                results._m0[_4185 + 1u] = _4187.y;
                uint _4197 = (outAt + 72u) >> 2u;
                uint2 _4199 = as_type<uint2>(float2(ray.outgoing.z.v.lo, ray.outgoing.z.v.hi));
                results._m0[_4197] = _4199.x;
                results._m0[_4197 + 1u] = _4199.y;
                uint _4209 = (outAt + 80u) >> 2u;
                uint2 _4211 = as_type<uint2>(float2(ray.depth.v.lo, ray.depth.v.hi));
                results._m0[_4209] = _4211.x;
                results._m0[_4209 + 1u] = _4211.y;
                uint _4221 = (outAt + 88u) >> 2u;
                uint2 _4223 = as_type<uint2>(float2(ray.bias0.v.lo, ray.bias0.v.hi));
                results._m0[_4221] = _4223.x;
                results._m0[_4221 + 1u] = _4223.y;
                _4053 = _4131;
            }
            else
            {
                _4053 = _4052;
            }
            OpticalJetRay param_var_ray = ray;
            Interval3 param_var_target = target;
            bool param_var_finiteTerminal = finiteTarget;
            float param_var_focal = precise::max(receiver.projection.x, receiver.projection.y);
            uint param_var_omitted = _4053;
            bool _4238 = optical_jet_residual(param_var_ray, param_var_target, param_var_finiteTerminal, param_var_focal, param_var_omitted, a, b, intervalFailed, optical_product_upper, interval_divide_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (!_4238)
            {
                if (_4054 >= 2u)
                {
                    _4400 = _4053;
                    break;
                }
                results._m0[(outAt + 12u) >> 2u] = (intervalFailed ? 17u : 16u) + ((!jetBranchKnown) ? 2u : 0u);
                return;
            }
            if (_4054 == 0u)
            {
                uint _4325 = (outAt + 96u) >> 2u;
                uint2 _4327 = as_type<uint2>(float2(a.v.lo, a.v.hi));
                results._m0[_4325] = _4327.x;
                results._m0[_4325 + 1u] = _4327.y;
                uint _4337 = (outAt + 104u) >> 2u;
                uint2 _4339 = as_type<uint2>(float2(b.v.lo, b.v.hi));
                results._m0[_4337] = _4339.x;
                results._m0[_4337 + 1u] = _4339.y;
                uint _4363 = (outAt + 112u) >> 2u;
                uint2 _4365 = as_type<uint2>(float2(a.dx.lo, a.dx.hi));
                results._m0[_4363] = _4365.x;
                results._m0[_4363 + 1u] = _4365.y;
                uint _4373 = (outAt + 120u) >> 2u;
                uint2 _4375 = as_type<uint2>(float2(a.dy.lo, a.dy.hi));
                results._m0[_4373] = _4375.x;
                results._m0[_4373 + 1u] = _4375.y;
                uint _4383 = (outAt + 128u) >> 2u;
                uint2 _4385 = as_type<uint2>(float2(b.dx.lo, b.dx.hi));
                results._m0[_4383] = _4385.x;
                results._m0[_4383 + 1u] = _4385.y;
                uint _4393 = (outAt + 136u) >> 2u;
                uint2 _4395 = as_type<uint2>(float2(b.dy.lo, b.dy.hi));
                results._m0[_4393] = _4395.x;
                results._m0[_4393 + 1u] = _4395.y;
                _4021 = _4020;
                _4023 = _4022;
                _4025 = _4024;
                _4027 = _4026;
                _4029 = _4028;
                _4031 = _4030;
                _4033 = _4032;
                _4035 = _4034;
                _4037 = b.dy.lo;
                _4039 = b.dy.hi;
                _4041 = b.dx.lo;
                _4043 = b.dx.hi;
                _4045 = a.dy.lo;
                _4047 = a.dy.hi;
                _4049 = a.dx.lo;
                _4051 = a.dx.hi;
                _4056 = _4055;
                _4058 = _4057;
                _4060 = _4059;
                _4062 = _4061;
            }
            else
            {
                float _4308;
                float _4309;
                float _4310;
                float _4311;
                float _4312;
                float _4313;
                float _4314;
                float _4315;
                float _4316;
                float _4317;
                float _4318;
                float _4319;
                if (_4054 == 1u)
                {
                    float4 param_var_box_1 = box;
                    float2 param_var_centre = _1631;
                    Interval param_var_f0 = a.v;
                    Interval param_var_f1 = b.v;
                    Interval param_var_j00 = Interval{ _4048, _4050 };
                    Interval param_var_j01 = Interval{ _4044, _4046 };
                    Interval param_var_j10 = Interval{ _4040, _4042 };
                    Interval param_var_j11 = Interval{ _4036, _4038 };
                    OpticalLocalRoot _4307 = optical_local_root(param_var_box_1, param_var_centre, param_var_f0, param_var_f1, param_var_j00, param_var_j01, param_var_j10, param_var_j11, intervalFailed, optical_product_upper, jetBranchKnown);
                    root = _4307;
                    _4308 = _4020;
                    _4309 = _4022;
                    _4310 = _4024;
                    _4311 = _4026;
                    _4312 = _4028;
                    _4313 = _4030;
                    _4314 = _4032;
                    _4315 = _4034;
                    _4316 = b.v.lo;
                    _4317 = b.v.hi;
                    _4318 = a.v.lo;
                    _4319 = a.v.hi;
                }
                else
                {
                    bool _4250;
                    if (_4054 == 2u)
                    {
                        _4250 = _4019;
                    }
                    else
                    {
                        _4250 = false;
                    }
                    float _4286;
                    float _4287;
                    float _4288;
                    float _4289;
                    float _4290;
                    float _4291;
                    float _4292;
                    float _4293;
                    if (_4250)
                    {
                        _4286 = b.dy.lo;
                        _4287 = b.dy.hi;
                        _4288 = b.dx.lo;
                        _4289 = b.dx.hi;
                        _4290 = a.dy.lo;
                        _4291 = a.dy.hi;
                        _4292 = a.dx.lo;
                        _4293 = a.dx.hi;
                    }
                    else
                    {
                        float _4251;
                        float _4252;
                        float _4253;
                        float _4254;
                        float _4255;
                        float _4256;
                        float _4257;
                        float _4258;
                        if (_4019)
                        {
                            _4251 = _4020;
                            _4252 = _4022;
                            _4253 = _4024;
                            _4254 = _4026;
                            _4255 = _4028;
                            _4256 = _4030;
                            _4257 = _4032;
                            _4258 = _4034;
                        }
                        else
                        {
                            _4251 = _4036;
                            _4252 = _4038;
                            _4253 = _4040;
                            _4254 = _4042;
                            _4255 = _4044;
                            _4256 = _4046;
                            _4257 = _4048;
                            _4258 = _4050;
                        }
                        float4 param_var_box_2 = _4072;
                        float2 param_var_centre_1 = _4076;
                        Interval param_var_f0_1 = a.v;
                        Interval param_var_f1_1 = b.v;
                        Interval param_var_j00_1 = Interval{ _4257, _4258 };
                        Interval param_var_j01_1 = Interval{ _4255, _4256 };
                        Interval param_var_j10_1 = Interval{ _4253, _4254 };
                        Interval param_var_j11_1 = Interval{ _4251, _4252 };
                        OpticalLocalRoot _4267 = optical_local_root(param_var_box_2, param_var_centre_1, param_var_f0_1, param_var_f1_1, param_var_j00_1, param_var_j01_1, param_var_j10_1, param_var_j11_1, intervalFailed, optical_product_upper, jetBranchKnown);
                        OpticalLocalRoot param_var_next = _4267;
                        bool _4268 = optical_intersect_inherited_root(root, param_var_next, intervalFailed, jetBranchKnown);
                        if (!_4268)
                        {
                            _4400 = _4053;
                            break;
                        }
                        _4286 = _4020;
                        _4287 = _4022;
                        _4288 = _4024;
                        _4289 = _4026;
                        _4290 = _4028;
                        _4291 = _4030;
                        _4292 = _4032;
                        _4293 = _4034;
                    }
                    _4308 = _4286;
                    _4309 = _4287;
                    _4310 = _4288;
                    _4311 = _4289;
                    _4312 = _4290;
                    _4313 = _4291;
                    _4314 = _4292;
                    _4315 = _4293;
                    _4316 = _4055;
                    _4317 = _4057;
                    _4318 = _4059;
                    _4319 = _4061;
                }
                _4021 = _4308;
                _4023 = _4309;
                _4025 = _4310;
                _4027 = _4311;
                _4029 = _4312;
                _4031 = _4313;
                _4033 = _4314;
                _4035 = _4315;
                _4037 = _4036;
                _4039 = _4038;
                _4041 = _4040;
                _4043 = _4042;
                _4045 = _4044;
                _4047 = _4046;
                _4049 = _4048;
                _4051 = _4050;
                _4056 = _4316;
                _4058 = _4317;
                _4060 = _4318;
                _4062 = _4319;
            }
            _4020 = _4021;
            _4022 = _4023;
            _4024 = _4025;
            _4026 = _4027;
            _4028 = _4029;
            _4030 = _4031;
            _4032 = _4033;
            _4034 = _4035;
            _4036 = _4037;
            _4038 = _4039;
            _4040 = _4041;
            _4042 = _4043;
            _4044 = _4045;
            _4046 = _4047;
            _4048 = _4049;
            _4050 = _4051;
            _4052 = _4053;
            _4054++;
            _4055 = _4056;
            _4057 = _4058;
            _4059 = _4060;
            _4061 = _4062;
            continue;
        }
        else
        {
            _4400 = _4052;
            break;
        }
    }
    uint _4402 = outAt >> 2u;
    results._m0[_4402] = 1u;
    results._m0[_4402 + 1u] = root.status;
    results._m0[_4402 + 2u] = _4400;
    results._m0[_4402 + 3u] = 0u;
    uint _4410 = (outAt + 16u) >> 2u;
    uint4 _4412 = as_type<uint4>(box);
    results._m0[_4410] = _4412.x;
    results._m0[_4410 + 1u] = _4412.y;
    results._m0[_4410 + 2u] = _4412.z;
    results._m0[_4410 + 3u] = _4412.w;
    uint _4422 = (outAt + 144u) >> 2u;
    uint4 _4425 = as_type<uint4>(root.enclosure);
    results._m0[_4422] = _4425.x;
    results._m0[_4422 + 1u] = _4425.y;
    results._m0[_4422 + 2u] = _4425.z;
    results._m0[_4422 + 3u] = _4425.w;
    uint _4435 = (outAt + 160u) >> 2u;
    uint4 _4441 = as_type<uint4>(float4(root.contraction, _1631, 0.0));
    results._m0[_4435] = _4441.x;
    results._m0[_4435 + 1u] = _4441.y;
    results._m0[_4435 + 2u] = _4441.z;
    results._m0[_4435 + 3u] = _4441.w;
    uint _4451 = (outAt + 176u) >> 2u;
    uint2 _4453 = as_type<uint2>(float2(_4059, _4061));
    results._m0[_4451] = _4453.x;
    results._m0[_4451 + 1u] = _4453.y;
    uint _4459 = (outAt + 184u) >> 2u;
    uint2 _4461 = as_type<uint2>(float2(_4055, _4057));
    results._m0[_4459] = _4461.x;
    results._m0[_4459 + 1u] = _4461.y;
}

static inline __attribute__((always_inline))
void src_feature_jets_stream_main(thread const uint3& id, constant type_Settings& Settings, device type_ByteAddressBuffer& frames, device type_ByteAddressBuffer& regions, device type_ByteAddressBuffer& queries, device type_ByteAddressBuffer& optical, device type_RWByteAddressBuffer& results, constant type_RootSettings& RootSettings, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments, constant type_TargetSettings& TargetSettings)
{
    bool _2307;
    if (id.x < Settings.queryCount)
    {
        _2307 = id.x >= 64u;
    }
    else
    {
        _2307 = true;
    }
    bool _2309;
    if (!_2307)
    {
        _2309 = false;
    }
    else
    {
        _2309 = true;
    }
    if (_2309)
    {
        return;
    }
    uint _1455 = id.x * 24576u;
    uint _1456 = id.x * 6160u;
    uint _1457 = id.x * 512u;
    for (uint _2310 = 0u; _2310 < 36u; _2310++)
    {
        uint _2312 = (_1455 + (_2310 * 16u)) >> 2u;
        results._m0[_2312] = 0u;
        results._m0[_2312 + 1u] = 0u;
        results._m0[_2312 + 2u] = 0u;
        results._m0[_2312 + 3u] = 0u;
    }
    for (uint _2317 = 3u; _2317 < 128u; _2317++)
    {
        results._m0[((_1455 + (_2317 * 192u)) + 12u) >> 2u] = 0u;
    }
    if (RootSettings.rootMode != 1u)
    {
        for (uint _2325 = 0u; _2325 < 128u; _2325++)
        {
            results._m0[((_1455 + (_2325 * 192u)) + 12u) >> 2u] = 32u;
        }
        return;
    }
    uint _2332 = _1456 >> 2u;
    uint _2334 = regions._m0[_2332];
    bool _2344;
    if (Settings.queryCount <= 64u)
    {
        _2344 = RootSettings.rootCapacity != 128u;
    }
    else
    {
        _2344 = true;
    }
    bool _2349;
    if (!_2344)
    {
        _2349 = RootSettings.rootFrameStride != 512u;
    }
    else
    {
        _2349 = true;
    }
    bool _2354;
    if (!_2349)
    {
        _2354 = RootSettings.rootResultStride != 192u;
    }
    else
    {
        _2354 = true;
    }
    bool _2359;
    if (!_2354)
    {
        _2359 = RootSettings.rootMode > 1u;
    }
    else
    {
        _2359 = true;
    }
    bool _2362;
    if (!_2359)
    {
        _2362 = _2334 > 128u;
    }
    else
    {
        _2362 = true;
    }
    bool _2365;
    if (!_2362)
    {
        _2365 = regions._m0[(_1456 + 4u) >> 2u] != 0u;
    }
    else
    {
        _2365 = true;
    }
    if (_2365)
    {
        for (uint _2367 = 0u; _2367 < 128u; _2367++)
        {
            results._m0[((_1455 + (_2367 * 192u)) + 12u) >> 2u] = 32u;
        }
        return;
    }
    float4 box = float4(1000000015047466219876688855040.0, 1000000015047466219876688855040.0, -1000000015047466219876688855040.0, -1000000015047466219876688855040.0);
    bool _2374;
    _2374 = false;
    bool _2375;
    uint _2376 = 0u;
    for (;;)
    {
        if (_2376 < _2334)
        {
            uint _2378 = (((id.x * 128u) + _2376) * 32u) >> 2u;
            uint _2380 = optical._m0[_2378];
            uint _1473 = _2378 + 2u;
            bool _2389;
            if (optical._m0[_2378 + 1u] == 0u)
            {
                _2389 = optical._m0[_1473] == 0u;
            }
            else
            {
                _2389 = true;
            }
            bool _2392;
            if (!_2389)
            {
                _2392 = optical._m0[_1473] > 8192u;
            }
            else
            {
                _2392 = true;
            }
            bool _2395;
            if (!_2392)
            {
                _2395 = _2380 > optical._m0[_1473];
            }
            else
            {
                _2395 = true;
            }
            bool _2398;
            if (!_2395)
            {
                _2398 = optical._m0[_2378 + 3u] != (optical._m0[_1473] - _2380);
            }
            else
            {
                _2398 = true;
            }
            if (_2398)
            {
                results._m0[(_1455 + 12u) >> 2u] = 32u;
                return;
            }
            if (_2380 == 0u)
            {
                _2375 = _2374;
                uint _1485 = _2376 + 1u;
                _2374 = _2375;
                _2376 = _1485;
                continue;
            }
            uint _2402 = ((((id.x * 128u) + _2376) * 32u) + 16u) >> 2u;
            uint _2404 = optical._m0[_2402];
            uint _2406 = optical._m0[_2402 + 1u];
            uint _2408 = optical._m0[_2402 + 2u];
            uint _2410 = optical._m0[_2402 + 3u];
            float4 _2412 = as_type<float4>(uint4(_2404, _2406, _2408, _2410));
            bool4 _2413 = isnan(_2412);
            bool4 _2414 = isinf(_2412);
            bool _2422;
            if (all(not(bool4(_2413.x || _2414.x, _2413.y || _2414.y, _2413.z || _2414.z, _2413.w || _2414.w))))
            {
                _2422 = any(_2412.xy > _2412.zw);
            }
            else
            {
                _2422 = true;
            }
            if (_2422)
            {
                results._m0[(_1455 + 12u) >> 2u] = 64u;
                return;
            }
            box = float4(precise::min(box.xy, _2412.xy), precise::max(box.zw, _2412.zw));
            _2375 = true;
            uint _1485 = _2376 + 1u;
            _2374 = _2375;
            _2376 = _1485;
            continue;
        }
        else
        {
            break;
        }
    }
    uint _2437 = (_1457 + 496u) >> 2u;
    if (any(uint4(frames._m0[_2437], frames._m0[_2437 + 1u], frames._m0[_2437 + 2u], frames._m0[_2437 + 3u]) != uint4(1u, 0u, 0u, 0u)))
    {
        results._m0[(_1455 + 12u) >> 2u] = 64u;
        return;
    }
    uint _2451 = _1457 >> 2u;
    uint _2453 = frames._m0[_2451];
    uint _2455 = frames._m0[_2451 + 1u];
    uint _2457 = frames._m0[_2451 + 2u];
    uint _2459 = frames._m0[_2451 + 3u];
    float4 _2461 = as_type<float4>(uint4(_2453, _2455, _2457, _2459));
    uint _2462 = (_1457 + 16u) >> 2u;
    uint _2464 = frames._m0[_2462];
    uint _2466 = frames._m0[_2462 + 1u];
    uint _2468 = frames._m0[_2462 + 2u];
    uint _2470 = frames._m0[_2462 + 3u];
    float4 _2472 = as_type<float4>(uint4(_2464, _2466, _2468, _2470));
    uint _2473 = (_1457 + 32u) >> 2u;
    uint _2475 = frames._m0[_2473];
    uint _2477 = frames._m0[_2473 + 1u];
    uint _2479 = frames._m0[_2473 + 2u];
    uint _2481 = frames._m0[_2473 + 3u];
    float4 _2483 = as_type<float4>(uint4(_2475, _2477, _2479, _2481));
    uint _2484 = (_1457 + 48u) >> 2u;
    uint _2486 = frames._m0[_2484];
    uint _2488 = frames._m0[_2484 + 1u];
    uint _2490 = frames._m0[_2484 + 2u];
    uint _2492 = frames._m0[_2484 + 3u];
    float4 _2494 = as_type<float4>(uint4(_2486, _2488, _2490, _2492));
    uint _2495 = (_1457 + 64u) >> 2u;
    uint _2497 = frames._m0[_2495];
    uint _2499 = frames._m0[_2495 + 1u];
    uint _2501 = frames._m0[_2495 + 2u];
    uint _2503 = frames._m0[_2495 + 3u];
    float4 _2505 = as_type<float4>(uint4(_2497, _2499, _2501, _2503));
    uint _2506 = (_1457 + 80u) >> 2u;
    uint _2508 = frames._m0[_2506];
    uint _2510 = frames._m0[_2506 + 1u];
    uint _2512 = frames._m0[_2506 + 2u];
    uint _2514 = frames._m0[_2506 + 3u];
    float4 _2516 = as_type<float4>(uint4(_2508, _2510, _2512, _2514));
    spvUnsafeArray<ReflectionSpecularPlane, 4> planes;
    for (uint _2517 = 0u; _2517 < 4u; _2517++)
    {
        uint _2519 = ((_1457 + 96u) + (_2517 * 48u)) >> 2u;
        planes[_2517].a = as_type<float4>(uint4(frames._m0[_2519], frames._m0[_2519 + 1u], frames._m0[_2519 + 2u], frames._m0[_2519 + 3u]));
        uint _2531 = ((_1457 + 112u) + (_2517 * 48u)) >> 2u;
        planes[_2517].b = as_type<float4>(uint4(frames._m0[_2531], frames._m0[_2531 + 1u], frames._m0[_2531 + 2u], frames._m0[_2531 + 3u]));
        uint _2543 = ((_1457 + 128u) + (_2517 * 48u)) >> 2u;
        planes[_2517].c = as_type<float4>(uint4(frames._m0[_2543], frames._m0[_2543 + 1u], frames._m0[_2543 + 2u], frames._m0[_2543 + 3u]));
    }
    uint _2555 = (_1457 + 288u) >> 2u;
    uint _2557 = frames._m0[_2555];
    uint _2559 = frames._m0[_2555 + 1u];
    uint _2561 = frames._m0[_2555 + 2u];
    uint _2563 = frames._m0[_2555 + 3u];
    float4 _2565 = as_type<float4>(uint4(_2557, _2559, _2561, _2563));
    uint _2566 = (_1457 + 304u) >> 2u;
    uint _2568 = frames._m0[_2566];
    uint _2570 = frames._m0[_2566 + 1u];
    uint _2572 = frames._m0[_2566 + 2u];
    uint _2574 = frames._m0[_2566 + 3u];
    float4 _2576 = as_type<float4>(uint4(_2568, _2570, _2572, _2574));
    uint _2577 = (_1457 + 320u) >> 2u;
    uint _2579 = frames._m0[_2577];
    uint _2581 = frames._m0[_2577 + 1u];
    uint _2583 = frames._m0[_2577 + 2u];
    uint _2585 = frames._m0[_2577 + 3u];
    float4 _2587 = as_type<float4>(uint4(_2579, _2581, _2583, _2585));
    uint _2588 = (_1457 + 336u) >> 2u;
    uint _2590 = frames._m0[_2588];
    uint _2592 = frames._m0[_2588 + 1u];
    uint _2594 = frames._m0[_2588 + 2u];
    uint _2596 = frames._m0[_2588 + 3u];
    float4 _2598 = as_type<float4>(uint4(_2590, _2592, _2594, _2596));
    uint _2599 = (_1457 + 352u) >> 2u;
    uint _2601 = frames._m0[_2599];
    uint _2603 = frames._m0[_2599 + 1u];
    uint _2605 = frames._m0[_2599 + 2u];
    uint _2607 = frames._m0[_2599 + 3u];
    float4 _2609 = as_type<float4>(uint4(_2601, _2603, _2605, _2607));
    uint _2610 = (_1457 + 368u) >> 2u;
    uint _2612 = frames._m0[_2610];
    uint _2614 = frames._m0[_2610 + 1u];
    uint _2616 = frames._m0[_2610 + 2u];
    uint _2618 = frames._m0[_2610 + 3u];
    float4 _2620 = as_type<float4>(uint4(_2612, _2614, _2616, _2618));
    uint _2621 = (_1457 + 384u) >> 2u;
    uint _2623 = frames._m0[_2621];
    uint _2625 = frames._m0[_2621 + 1u];
    uint _2627 = frames._m0[_2621 + 2u];
    uint _2629 = frames._m0[_2621 + 3u];
    float4 _2631 = as_type<float4>(uint4(_2623, _2625, _2627, _2629));
    uint _2632 = (_1457 + 400u) >> 2u;
    uint _2634 = frames._m0[_2632];
    uint _2636 = frames._m0[_2632 + 1u];
    uint _2638 = frames._m0[_2632 + 2u];
    uint _2640 = frames._m0[_2632 + 3u];
    float4 _2642 = as_type<float4>(uint4(_2634, _2636, _2638, _2640));
    uint _2643 = (_1457 + 416u) >> 2u;
    uint _2645 = frames._m0[_2643];
    uint _2647 = frames._m0[_2643 + 1u];
    uint _2649 = frames._m0[_2643 + 2u];
    uint _2651 = frames._m0[_2643 + 3u];
    float4 _2653 = as_type<float4>(uint4(_2645, _2647, _2649, _2651));
    uint _2654 = (_1457 + 480u) >> 2u;
    uint _2656 = frames._m0[_2654];
    uint _1570 = _2654 + 1u;
    uint _2658 = frames._m0[_1570];
    uint _1571 = _2654 + 2u;
    uint _2660 = frames._m0[_1571];
    uint _1572 = _2654 + 3u;
    uint _2662 = frames._m0[_1572];
    uint4 _2663 = uint4(_2656, _2658, _2660, _2662);
    uint _2667 = (id.x * 48u) >> 2u;
    uint _2670 = ((id.x * 48u) + 44u) >> 2u;
    uint _2673 = (_1457 + 432u) >> 2u;
    uint _2684 = (_1457 + 448u) >> 2u;
    uint _2695 = (_1457 + 464u) >> 2u;
    bool _2710;
    if (_2656 <= 4u)
    {
        _2710 = (_2658 >> (_2656 & 31u)) != 0u;
    }
    else
    {
        _2710 = true;
    }
    bool _2713;
    if (!_2710)
    {
        _2713 = _2660 > 1u;
    }
    else
    {
        _2713 = true;
    }
    bool _2716;
    if (!_2713)
    {
        _2716 = _2662 < 1u;
    }
    else
    {
        _2716 = true;
    }
    bool _2719;
    if (!_2716)
    {
        _2719 = _2662 > 3u;
    }
    else
    {
        _2719 = true;
    }
    bool _2725;
    if (!_2719)
    {
        bool _2724;
        if (_2662 == 3u)
        {
            _2724 = _2656 != 4u;
        }
        else
        {
            _2724 = _2656 == 4u;
        }
        _2725 = _2724;
    }
    else
    {
        _2725 = true;
    }
    bool _2728;
    if (!_2725)
    {
        _2728 = queries._m0[_2667] == 4294967295u;
    }
    else
    {
        _2728 = true;
    }
    bool _2733;
    if (!_2728)
    {
        uint _2731;
        if (queries._m0[_2667] == 4294967293u)
        {
            _2731 = 1u;
        }
        else
        {
            _2731 = 0u;
        }
        _2733 = _2660 != _2731;
    }
    else
    {
        _2733 = true;
    }
    bool _2738;
    if (!_2733)
    {
        _2738 = queries._m0[_2670] >= Settings.lobes;
    }
    else
    {
        _2738 = true;
    }
    bool _2741;
    if (!_2738)
    {
        _2741 = queries._m0[_2670] > 7u;
    }
    else
    {
        _2741 = true;
    }
    bool _2748;
    if (!_2741)
    {
        _2748 = queries._m0[((id.x * 48u) + 20u) >> 2u] != (((_2662 << 8u) | (_2658 << 4u)) | _2656);
    }
    else
    {
        _2748 = true;
    }
    bool _2756;
    if (!_2748)
    {
        bool4 _2750 = isnan(_2516);
        bool4 _2751 = isinf(_2516);
        _2756 = !all(not(bool4(_2750.x || _2751.x, _2750.y || _2751.y, _2750.z || _2751.z, _2750.w || _2751.w)));
    }
    else
    {
        _2756 = true;
    }
    bool _2760;
    if (!_2756)
    {
        _2760 = _2516.x < 0.0;
    }
    else
    {
        _2760 = true;
    }
    bool _2764;
    if (!_2760)
    {
        _2764 = _2516.x > 1.0;
    }
    else
    {
        _2764 = true;
    }
    bool _2769;
    if (!_2764)
    {
        _2769 = _2516.y != float(queries._m0[_2670]);
    }
    else
    {
        _2769 = true;
    }
    bool _2774;
    if (!_2769)
    {
        _2774 = any(_2516.zw != float2(0.0));
    }
    else
    {
        _2774 = true;
    }
    bool _2782;
    if (!_2774)
    {
        bool4 _2776 = isnan(_2494);
        bool4 _2777 = isinf(_2494);
        _2782 = !all(not(bool4(_2776.x || _2777.x, _2776.y || _2777.y, _2776.z || _2777.z, _2776.w || _2777.w)));
    }
    else
    {
        _2782 = true;
    }
    bool _2787;
    if (!_2782)
    {
        _2787 = any(_2494.xy <= float2(0.0));
    }
    else
    {
        _2787 = true;
    }
    bool _2795;
    if (!_2787)
    {
        bool4 _2789 = isnan(_2505);
        bool4 _2790 = isinf(_2505);
        _2795 = !all(not(bool4(_2789.x || _2790.x, _2789.y || _2790.y, _2789.z || _2790.z, _2789.w || _2790.w)));
    }
    else
    {
        _2795 = true;
    }
    bool _2807;
    if (!_2795)
    {
        _2807 = any(_2505.xy != float2(float(Settings.width), float(Settings.height)));
    }
    else
    {
        _2807 = true;
    }
    bool _2812;
    if (!_2807)
    {
        _2812 = any(_2505.xy < float2(1.0));
    }
    else
    {
        _2812 = true;
    }
    bool _2817;
    if (!_2812)
    {
        _2817 = any(_2505.xy > float2(16384.0));
    }
    else
    {
        _2817 = true;
    }
    bool _2821;
    if (!_2817)
    {
        _2821 = _2505.z <= 0.0;
    }
    else
    {
        _2821 = true;
    }
    bool _2826;
    if (!_2821)
    {
        _2826 = _2505.w <= _2505.z;
    }
    else
    {
        _2826 = true;
    }
    bool _2834;
    if (!_2826)
    {
        bool4 _2828 = isnan(_2653);
        bool4 _2829 = isinf(_2653);
        _2834 = !all(not(bool4(_2828.x || _2829.x, _2828.y || _2829.y, _2828.z || _2829.z, _2828.w || _2829.w)));
    }
    else
    {
        _2834 = true;
    }
    bool _2841;
    if (!_2834)
    {
        int _2838;
        if (_2662 == 1u)
        {
            _2838 = 1;
        }
        else
        {
            _2838 = 0;
        }
        _2841 = _2653.w != float(_2838);
    }
    else
    {
        _2841 = true;
    }
    bool _2846;
    if (!_2841)
    {
        float3 param_var_f = _2653.xyz;
        uint param_var_kind = _2662;
        _2846 = !feature_valid(param_var_f, param_var_kind);
    }
    else
    {
        _2846 = true;
    }
    bool _2854;
    if (!_2846)
    {
        bool _2853;
        if (_2653.w != 0.0)
        {
            ReflectionSpecularPlane param_var_plane = ReflectionSpecularPlane{ as_type<float4>(uint4(frames._m0[_2673], frames._m0[_2673 + 1u], frames._m0[_2673 + 2u], frames._m0[_2673 + 3u])), as_type<float4>(uint4(frames._m0[_2684], frames._m0[_2684 + 1u], frames._m0[_2684 + 2u], frames._m0[_2684 + 3u])), as_type<float4>(uint4(frames._m0[_2695], frames._m0[_2695 + 1u], frames._m0[_2695 + 2u], frames._m0[_2695 + 3u])) };
            _2853 = !reflection_specular_plane_valid(param_var_plane);
        }
        else
        {
            _2853 = false;
        }
        _2854 = _2853;
    }
    else
    {
        _2854 = true;
    }
    bool _2861;
    if (!_2854)
    {
        bool _2860;
        if (_2660 == 0u)
        {
            ReflectionRoughFrame param_var_old = ReflectionRoughFrame{ _2461, _2472, _2483, _2494, _2505, _2516 };
            _2860 = !reflection_rough_frame_valid(param_var_old);
        }
        else
        {
            _2860 = false;
        }
        _2861 = _2860;
    }
    else
    {
        _2861 = true;
    }
    if (_2861)
    {
        results._m0[(_1455 + 12u) >> 2u] = 64u;
        return;
    }
    bool _2866;
    if (_2660 == 0u)
    {
        _2866 = _2658 != 0u;
    }
    else
    {
        _2866 = true;
    }
    if (_2866)
    {
        ReflectionLiquidFrame param_var_old_1 = ReflectionLiquidFrame{ _2565, _2576, _2587, _2598, _2609, _2620, _2631, _2642 };
        bool _2871;
        if (reflection_liquid_frame_valid(param_var_old_1))
        {
            _2871 = any(_2494 != _2620);
        }
        else
        {
            _2871 = true;
        }
        bool _2875;
        if (!_2871)
        {
            _2875 = any(_2505 != _2631);
        }
        else
        {
            _2875 = true;
        }
        if (_2875)
        {
            results._m0[(_1455 + 12u) >> 2u] = 64u;
            return;
        }
    }
    for (uint _2878 = 0u; _2878 < _2656; _2878++)
    {
        bool _2888;
        if ((_2658 & (1u << (_2878 & 31u))) == 0u)
        {
            ReflectionSpecularPlane param_var_plane_1 = planes[_2878];
            _2888 = !reflection_specular_plane_valid(param_var_plane_1);
        }
        else
        {
            _2888 = false;
        }
        if (_2888)
        {
            results._m0[(_1455 + 12u) >> 2u] = 64u;
            return;
        }
    }
    if (!_2374)
    {
        results._m0[(_1455 + 12u) >> 2u] = 4096u;
        return;
    }
    bool4 _2895 = isnan(box);
    bool4 _2896 = isinf(box);
    bool _2905;
    if (all(not(bool4(_2895.x || _2896.x, _2895.y || _2896.y, _2895.z || _2896.z, _2895.w || _2896.w))))
    {
        _2905 = any(box.xy > box.zw);
    }
    else
    {
        _2905 = true;
    }
    if (_2905)
    {
        results._m0[(_1455 + 12u) >> 2u] = 64u;
        return;
    }
    intervalFailed = false;
    uint param_var_at = _1457;
    float4 param_var_feature = _2653;
    Interval3 _2908 = native_target(param_var_at, param_var_feature, frames, intervalFailed, optical_product_upper, interval_divide_upper, TargetSettings);
    if (intervalFailed)
    {
        results._m0[(_1455 + 12u) >> 2u] = 128u;
        return;
    }
    for (uint _2912 = 0u; _2912 < 2u; _2912++)
    {
        for (uint _2914 = 0u; _2914 < 12u; _2914++)
        {
            uint _2916 = (_1455 + (_2914 * 16u)) >> 2u;
            results._m0[_2916] = 0u;
            results._m0[_2916 + 1u] = 0u;
            results._m0[_2916 + 2u] = 0u;
            results._m0[_2916 + 3u] = 0u;
        }
        uint param_var_outAt = _1455;
        float4 param_var_box = box;
        ReflectionRoughFrame param_var_receiver = ReflectionRoughFrame{ _2461, _2472, _2483, _2494, _2505, _2516 };
        ReflectionLiquidFrame param_var_liquid = ReflectionLiquidFrame{ _2565, _2576, _2587, _2598, _2609, _2620, _2631, _2642 };
        spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
        uint4 param_var_control = _2663;
        Interval3 param_var_target = _2908;
        bool param_var_finiteTarget = _2653.w != 0.0;
        uint param_var_refinements = 2u;
        write_optical_local_root(param_var_outAt, param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, param_var_target, param_var_finiteTarget, param_var_refinements, results, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
        bool _2933;
        if (_2912 == 0u)
        {
            _2933 = results._m0[_1455 >> 2u] != 1u;
        }
        else
        {
            _2933 = true;
        }
        bool _2939;
        if (!_2933)
        {
            _2939 = results._m0[(_1455 + 4u) >> 2u] != 0u;
        }
        else
        {
            _2939 = true;
        }
        bool _2945;
        if (!_2939)
        {
            _2945 = results._m0[(_1455 + 12u) >> 2u] != 0u;
        }
        else
        {
            _2945 = true;
        }
        if (_2945)
        {
            return;
        }
        intervalFailed = false;
        Interval _2299 = Interval{ box.x, box.x };
        Interval _2300 = Interval{ as_type<float>(3187671040u), as_type<float>(3187671040u) };
        Interval _2952 = iadd(_2299, _2300, intervalFailed);
        Interval _2297 = Interval{ box.y, box.y };
        Interval _2298 = Interval{ as_type<float>(3187671040u), as_type<float>(3187671040u) };
        Interval _2960 = iadd(_2297, _2298, intervalFailed);
        Interval param_var_a = Interval{ box.z, box.z };
        Interval param_var_b = Interval{ 0.125, 0.125 };
        Interval _2965 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval param_var_a_1 = Interval{ box.w, box.w };
        Interval param_var_b_1 = Interval{ 0.125, 0.125 };
        Interval _2970 = iadd(param_var_a_1, param_var_b_1, intervalFailed);
        if (intervalFailed)
        {
            return;
        }
        float4 _2987 = float4(precise::min(box.xy, precise::max(float2(0.5), float2(_2952.lo, _2960.lo))), precise::max(box.zw, precise::min(spvFSub(_2505.xy, float2(0.5)), float2(_2965.hi, _2970.hi))));
        bool4 _2988 = isnan(_2987);
        bool4 _2989 = isinf(_2987);
        bool _2998;
        if (all(not(bool4(_2988.x || _2989.x, _2988.y || _2989.y, _2988.z || _2989.z, _2988.w || _2989.w))))
        {
            _2998 = any(_2987.xy > box.xy);
        }
        else
        {
            _2998 = true;
        }
        bool _3005;
        if (!_2998)
        {
            _3005 = any(_2987.zw < box.zw);
        }
        else
        {
            _3005 = true;
        }
        bool _3010;
        if (!_3005)
        {
            _3010 = all(_2987 == box);
        }
        else
        {
            _3010 = true;
        }
        if (_3010)
        {
            return;
        }
        box = _2987;
    }
}

kernel void feature_jets_stream_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& frames [[buffer(1)]], device type_ByteAddressBuffer& regions [[buffer(2)]], device type_ByteAddressBuffer& queries [[buffer(3)]], device type_ByteAddressBuffer& optical [[buffer(4)]], device type_RWByteAddressBuffer& results [[buffer(5)]], constant type_RootSettings& RootSettings [[buffer(6)]], constant type_TargetSettings& TargetSettings [[buffer(7)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
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

