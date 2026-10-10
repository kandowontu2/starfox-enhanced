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

constant spvUnsafeArray<int2, 8> _2234 = spvUnsafeArray<int2, 8>({ int2(500, 0), int2(-500, 0), int2(0, 500), int2(0, -500), int2(612), int2(-612, 612), int2(612, -612), int2(-612) });
constant spvUnsafeArray<float, 9> _2286 = spvUnsafeArray<float, 9>({ 1.0, -0.16666667163372039794921875, 0.008333333767950534820556640625, -0.00019841270113829523324966430664063, 2.7557318844628753140568733215332e-06, -2.5052107943679402524139732122421e-08, 1.6059044372074282591711380518973e-10, -7.6471636098127127034729255683487e-13, 2.8114573589663703623298118827734e-15 });

static inline __attribute__((always_inline))
bool feature_valid(thread const float3& f, thread const uint& kind)
{
    bool3 _3018 = isnan(f);
    bool3 _3019 = isinf(f);
    if (!all(not(bool3(_3018.x || _3019.x, _3018.y || _3019.y, _3018.z || _3019.z))))
    {
        return false;
    }
    if (kind == 1u)
    {
        bool _3033;
        if (f.z == 0.0)
        {
            _3033 = all(f.xy >= float2(0.0));
        }
        else
        {
            _3033 = false;
        }
        bool _3039;
        if (_3033)
        {
            _3039 = spvFAdd(f.x, f.y) <= 1.0;
        }
        else
        {
            _3039 = false;
        }
        return _3039;
    }
    bool _3044;
    if (kind != 2u)
    {
        _3044 = kind == 3u;
    }
    else
    {
        _3044 = true;
    }
    bool _3049;
    if (_3044)
    {
        _3049 = abs(spvFSub(dot(f, f), 1.0)) < 0.00010099999781232327222824096679688;
    }
    else
    {
        _3049 = false;
    }
    return _3049;
}

static inline __attribute__((always_inline))
bool reflection_specular_plane_valid(thread const ReflectionSpecularPlane& plane)
{
    bool _3060;
    if (plane.a.w == 2.0)
    {
        _3060 = plane.b.w == 2.0;
    }
    else
    {
        _3060 = false;
    }
    bool _3065;
    if (_3060)
    {
        _3065 = plane.c.w == 2.0;
    }
    else
    {
        _3065 = false;
    }
    bool _3082;
    if (!_3065)
    {
        bool _3075;
        if ((isunordered(plane.a.w, 1.0) || plane.a.w == 1.0))
        {
            _3075 = plane.b.w != 1.0;
        }
        else
        {
            _3075 = true;
        }
        bool _3081;
        if (!_3075)
        {
            _3081 = plane.c.w != 1.0;
        }
        else
        {
            _3081 = true;
        }
        _3082 = _3081;
    }
    else
    {
        _3082 = false;
    }
    bool _3092;
    if (!_3082)
    {
        bool4 _3086 = isnan(plane.a);
        bool4 _3087 = isinf(plane.a);
        _3092 = !all(not(bool4(_3086.x || _3087.x, _3086.y || _3087.y, _3086.z || _3087.z, _3086.w || _3087.w)));
    }
    else
    {
        _3092 = true;
    }
    bool _3102;
    if (!_3092)
    {
        bool4 _3096 = isnan(plane.b);
        bool4 _3097 = isinf(plane.b);
        _3102 = !all(not(bool4(_3096.x || _3097.x, _3096.y || _3097.y, _3096.z || _3097.z, _3096.w || _3097.w)));
    }
    else
    {
        _3102 = true;
    }
    bool _3112;
    if (!_3102)
    {
        bool4 _3106 = isnan(plane.c);
        bool4 _3107 = isinf(plane.c);
        _3112 = !all(not(bool4(_3106.x || _3107.x, _3106.y || _3107.y, _3106.z || _3107.z, _3106.w || _3107.w)));
    }
    else
    {
        _3112 = true;
    }
    bool _3120;
    if (!_3112)
    {
        _3120 = any(abs(plane.a.xyz) > float3(999999995904.0));
    }
    else
    {
        _3120 = true;
    }
    bool _3128;
    if (!_3120)
    {
        _3128 = any(abs(plane.b.xyz) > float3(999999995904.0));
    }
    else
    {
        _3128 = true;
    }
    bool _3136;
    if (!_3128)
    {
        _3136 = any(abs(plane.c.xyz) > float3(999999995904.0));
    }
    else
    {
        _3136 = true;
    }
    if (_3136)
    {
        return false;
    }
    bool _3142;
    if (_3065)
    {
        _3142 = any(plane.c.xyz != float3(0.0));
    }
    else
    {
        _3142 = false;
    }
    if (_3142)
    {
        return false;
    }
    float3 _3159;
    if (_3065)
    {
        _3159 = plane.b.xyz;
    }
    else
    {
        _3159 = cross(spvFSub(plane.b.xyz, plane.a.xyz), spvFSub(plane.c.xyz, plane.a.xyz));
    }
    float _1619 = dot(_3159, _3159);
    bool3 _3160 = isnan(_3159);
    bool3 _3161 = isinf(_3159);
    bool _3169;
    if (all(not(bool3(_3160.x || _3161.x, _3160.y || _3161.y, _3160.z || _3161.z))))
    {
        _3169 = !(isnan(_1619) || isinf(_1619));
    }
    else
    {
        _3169 = false;
    }
    bool _3171;
    if (_3169)
    {
        _3171 = _1619 > 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3171 = false;
    }
    return _3171;
}

static inline __attribute__((always_inline))
bool reflection_rough_frame_valid(thread const ReflectionRoughFrame& old)
{
    bool _3182;
    if (old.a.w == 2.0)
    {
        _3182 = old.b.w == 2.0;
    }
    else
    {
        _3182 = false;
    }
    bool _3187;
    if (_3182)
    {
        _3187 = old.c.w == 2.0;
    }
    else
    {
        _3187 = false;
    }
    bool _3204;
    if (!_3187)
    {
        bool _3197;
        if ((isunordered(old.a.w, 1.0) || old.a.w == 1.0))
        {
            _3197 = old.b.w != 1.0;
        }
        else
        {
            _3197 = true;
        }
        bool _3203;
        if (!_3197)
        {
            _3203 = old.c.w != 1.0;
        }
        else
        {
            _3203 = true;
        }
        _3204 = _3203;
    }
    else
    {
        _3204 = false;
    }
    bool _3214;
    if (!_3204)
    {
        bool4 _3208 = isnan(old.a);
        bool4 _3209 = isinf(old.a);
        _3214 = !all(not(bool4(_3208.x || _3209.x, _3208.y || _3209.y, _3208.z || _3209.z, _3208.w || _3209.w)));
    }
    else
    {
        _3214 = true;
    }
    bool _3224;
    if (!_3214)
    {
        bool4 _3218 = isnan(old.b);
        bool4 _3219 = isinf(old.b);
        _3224 = !all(not(bool4(_3218.x || _3219.x, _3218.y || _3219.y, _3218.z || _3219.z, _3218.w || _3219.w)));
    }
    else
    {
        _3224 = true;
    }
    bool _3234;
    if (!_3224)
    {
        bool4 _3228 = isnan(old.c);
        bool4 _3229 = isinf(old.c);
        _3234 = !all(not(bool4(_3228.x || _3229.x, _3228.y || _3229.y, _3228.z || _3229.z, _3228.w || _3229.w)));
    }
    else
    {
        _3234 = true;
    }
    bool _3244;
    if (!_3234)
    {
        bool4 _3238 = isnan(old.projection);
        bool4 _3239 = isinf(old.projection);
        _3244 = !all(not(bool4(_3238.x || _3239.x, _3238.y || _3239.y, _3238.z || _3239.z, _3238.w || _3239.w)));
    }
    else
    {
        _3244 = true;
    }
    bool _3254;
    if (!_3244)
    {
        bool4 _3248 = isnan(old.extentClip);
        bool4 _3249 = isinf(old.extentClip);
        _3254 = !all(not(bool4(_3248.x || _3249.x, _3248.y || _3249.y, _3248.z || _3249.z, _3248.w || _3249.w)));
    }
    else
    {
        _3254 = true;
    }
    bool _3264;
    if (!_3254)
    {
        bool4 _3258 = isnan(old.settings);
        bool4 _3259 = isinf(old.settings);
        _3264 = !all(not(bool4(_3258.x || _3259.x, _3258.y || _3259.y, _3258.z || _3259.z, _3258.w || _3259.w)));
    }
    else
    {
        _3264 = true;
    }
    bool _3272;
    if (!_3264)
    {
        _3272 = any(abs(old.a.xyz) > float3(999999995904.0));
    }
    else
    {
        _3272 = true;
    }
    bool _3280;
    if (!_3272)
    {
        _3280 = any(abs(old.b.xyz) > float3(999999995904.0));
    }
    else
    {
        _3280 = true;
    }
    bool _3288;
    if (!_3280)
    {
        _3288 = any(abs(old.c.xyz) > float3(999999995904.0));
    }
    else
    {
        _3288 = true;
    }
    bool _3295;
    if (!_3288)
    {
        _3295 = any(old.projection.xy <= float2(0.0));
    }
    else
    {
        _3295 = true;
    }
    bool _3302;
    if (!_3295)
    {
        _3302 = any(abs(old.projection) > float4(999999995904.0));
    }
    else
    {
        _3302 = true;
    }
    bool _3309;
    if (!_3302)
    {
        _3309 = any(old.extentClip.xy < float2(1.0));
    }
    else
    {
        _3309 = true;
    }
    bool _3316;
    if (!_3309)
    {
        _3316 = any(old.extentClip.xy > float2(16384.0));
    }
    else
    {
        _3316 = true;
    }
    bool _3322;
    if (!_3316)
    {
        _3322 = old.extentClip.z <= 0.0;
    }
    else
    {
        _3322 = true;
    }
    bool _3331;
    if (!_3322)
    {
        _3331 = old.extentClip.w <= old.extentClip.z;
    }
    else
    {
        _3331 = true;
    }
    bool _3337;
    if (!_3331)
    {
        _3337 = old.extentClip.w > 999999995904.0;
    }
    else
    {
        _3337 = true;
    }
    bool _3343;
    if (!_3337)
    {
        _3343 = old.settings.x < 0.0;
    }
    else
    {
        _3343 = true;
    }
    bool _3349;
    if (!_3343)
    {
        _3349 = old.settings.x > 1.0;
    }
    else
    {
        _3349 = true;
    }
    bool _3355;
    if (!_3349)
    {
        _3355 = old.settings.y < 0.0;
    }
    else
    {
        _3355 = true;
    }
    bool _3361;
    if (!_3355)
    {
        _3361 = old.settings.y > 7.0;
    }
    else
    {
        _3361 = true;
    }
    bool _3371;
    if (!_3361)
    {
        _3371 = floor(old.settings.y) != old.settings.y;
    }
    else
    {
        _3371 = true;
    }
    bool _3378;
    if (!_3371)
    {
        _3378 = any(old.settings.zw != float2(0.0));
    }
    else
    {
        _3378 = true;
    }
    if (_3378)
    {
        return false;
    }
    bool _3384;
    if (_3187)
    {
        _3384 = any(old.c.xyz != float3(0.0));
    }
    else
    {
        _3384 = false;
    }
    if (_3384)
    {
        return false;
    }
    float3 _3401;
    if (_3187)
    {
        _3401 = old.b.xyz;
    }
    else
    {
        _3401 = cross(spvFSub(old.b.xyz, old.a.xyz), spvFSub(old.c.xyz, old.a.xyz));
    }
    float _1622 = dot(_3401, _3401);
    bool3 _3402 = isnan(_3401);
    bool3 _3403 = isinf(_3401);
    bool _3411;
    if (all(not(bool3(_3402.x || _3403.x, _3402.y || _3403.y, _3402.z || _3403.z))))
    {
        _3411 = !(isnan(_1622) || isinf(_1622));
    }
    else
    {
        _3411 = false;
    }
    bool _3413;
    if (_3411)
    {
        _3413 = _1622 > 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3413 = false;
    }
    return _3413;
}

static inline __attribute__((always_inline))
bool reflection_liquid_frame_valid(thread const ReflectionLiquidFrame& old)
{
    float _1623 = dot(old.planeNormal.xyz, old.planeNormal.xyz);
    bool4 _3422 = isnan(old.planePoint);
    bool4 _3423 = isinf(old.planePoint);
    bool _3435;
    if (all(not(bool4(_3422.x || _3423.x, _3422.y || _3423.y, _3422.z || _3423.z, _3422.w || _3423.w))))
    {
        bool4 _3429 = isnan(old.planeNormal);
        bool4 _3430 = isinf(old.planeNormal);
        _3435 = !all(not(bool4(_3429.x || _3430.x, _3429.y || _3430.y, _3429.z || _3430.z, _3429.w || _3430.w)));
    }
    else
    {
        _3435 = true;
    }
    bool _3445;
    if (!_3435)
    {
        bool4 _3439 = isnan(old.rotation0);
        bool4 _3440 = isinf(old.rotation0);
        _3445 = !all(not(bool4(_3439.x || _3440.x, _3439.y || _3440.y, _3439.z || _3440.z, _3439.w || _3440.w)));
    }
    else
    {
        _3445 = true;
    }
    bool _3455;
    if (!_3445)
    {
        bool4 _3449 = isnan(old.rotation1);
        bool4 _3450 = isinf(old.rotation1);
        _3455 = !all(not(bool4(_3449.x || _3450.x, _3449.y || _3450.y, _3449.z || _3450.z, _3449.w || _3450.w)));
    }
    else
    {
        _3455 = true;
    }
    bool _3465;
    if (!_3455)
    {
        bool4 _3459 = isnan(old.rotation2);
        bool4 _3460 = isinf(old.rotation2);
        _3465 = !all(not(bool4(_3459.x || _3460.x, _3459.y || _3460.y, _3459.z || _3460.z, _3459.w || _3460.w)));
    }
    else
    {
        _3465 = true;
    }
    bool _3475;
    if (!_3465)
    {
        bool4 _3469 = isnan(old.projection);
        bool4 _3470 = isinf(old.projection);
        _3475 = !all(not(bool4(_3469.x || _3470.x, _3469.y || _3470.y, _3469.z || _3470.z, _3469.w || _3470.w)));
    }
    else
    {
        _3475 = true;
    }
    bool _3485;
    if (!_3475)
    {
        bool4 _3479 = isnan(old.extentClip);
        bool4 _3480 = isinf(old.extentClip);
        _3485 = !all(not(bool4(_3479.x || _3480.x, _3479.y || _3480.y, _3479.z || _3480.z, _3479.w || _3480.w)));
    }
    else
    {
        _3485 = true;
    }
    bool _3495;
    if (!_3485)
    {
        bool4 _3489 = isnan(old.settings);
        bool4 _3490 = isinf(old.settings);
        _3495 = !all(not(bool4(_3489.x || _3490.x, _3489.y || _3490.y, _3489.z || _3490.z, _3489.w || _3490.w)));
    }
    else
    {
        _3495 = true;
    }
    bool _3503;
    if (!_3495)
    {
        _3503 = any(abs(old.planePoint.xyz) > float3(999999995904.0));
    }
    else
    {
        _3503 = true;
    }
    bool _3508;
    if (!_3503)
    {
        _3508 = isnan(_1623) || isinf(_1623);
    }
    else
    {
        _3508 = true;
    }
    bool _3511;
    if (!_3508)
    {
        _3511 = _1623 <= 9.9999996826552253889678874634872e-21;
    }
    else
    {
        _3511 = true;
    }
    bool _3518;
    if (!_3511)
    {
        _3518 = any(abs(old.rotation0) > float4(999999995904.0));
    }
    else
    {
        _3518 = true;
    }
    bool _3525;
    if (!_3518)
    {
        _3525 = any(abs(old.rotation1) > float4(999999995904.0));
    }
    else
    {
        _3525 = true;
    }
    bool _3532;
    if (!_3525)
    {
        _3532 = any(abs(old.rotation2) > float4(999999995904.0));
    }
    else
    {
        _3532 = true;
    }
    bool _3539;
    if (!_3532)
    {
        _3539 = any(old.projection.xy <= float2(0.0));
    }
    else
    {
        _3539 = true;
    }
    bool _3546;
    if (!_3539)
    {
        _3546 = any(old.projection.xy > float2(999999995904.0));
    }
    else
    {
        _3546 = true;
    }
    bool _3553;
    if (!_3546)
    {
        _3553 = any(old.extentClip.xy < float2(1.0));
    }
    else
    {
        _3553 = true;
    }
    bool _3560;
    if (!_3553)
    {
        _3560 = any(old.extentClip.xy > float2(16384.0));
    }
    else
    {
        _3560 = true;
    }
    bool _3566;
    if (!_3560)
    {
        _3566 = old.extentClip.z <= 0.0;
    }
    else
    {
        _3566 = true;
    }
    bool _3575;
    if (!_3566)
    {
        _3575 = old.extentClip.w <= old.extentClip.z;
    }
    else
    {
        _3575 = true;
    }
    bool _3581;
    if (!_3575)
    {
        _3581 = old.extentClip.w > 999999995904.0;
    }
    else
    {
        _3581 = true;
    }
    bool _3587;
    if (!_3581)
    {
        _3587 = old.settings.x < 0.0;
    }
    else
    {
        _3587 = true;
    }
    bool _3593;
    if (!_3587)
    {
        _3593 = old.settings.x > 999999995904.0;
    }
    else
    {
        _3593 = true;
    }
    bool _3604;
    if (!_3593)
    {
        bool _3603;
        if (old.settings.y != 0.0)
        {
            _3603 = old.settings.y != 3.0;
        }
        else
        {
            _3603 = false;
        }
        _3604 = _3603;
    }
    else
    {
        _3604 = true;
    }
    if (_3604)
    {
        return false;
    }
    float _1624 = dot(old.rotation0.xyz, cross(old.rotation1.xyz, old.rotation2.xyz));
    bool _3621;
    if (!(isnan(_1624) || isinf(_1624)))
    {
        _3621 = abs(_1624) > 9.9999999600419720025001879548654e-13;
    }
    else
    {
        _3621 = false;
    }
    return _3621;
}

static inline __attribute__((always_inline))
bool interval_exact_point(thread const Interval& a, thread const float& x)
{
    if (x == 0.0)
    {
        return ((as_type<uint>(a.lo) | as_type<uint>(a.hi)) & 2147483647u) == 0u;
    }
    bool _11388;
    if (as_type<uint>(a.lo) == as_type<uint>(x))
    {
        _11388 = as_type<uint>(a.hi) == as_type<uint>(x);
    }
    else
    {
        _11388 = false;
    }
    return _11388;
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
    uint _15914;
    if (x > 0.0)
    {
        _15914 = 1u;
    }
    else
    {
        _15914 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _15914);
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
    uint _15928;
    if (x < 0.0)
    {
        _15928 = 1u;
    }
    else
    {
        _15928 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _15928);
}

static inline __attribute__((always_inline))
float optical_add_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    uint _11391 = as_type<uint>(a) & 2147483647u;
    uint _11394 = as_type<uint>(b) & 2147483647u;
    bool _11397;
    if (_11391 < 2139095040u)
    {
        _11397 = _11394 < 2139095040u;
    }
    else
    {
        _11397 = false;
    }
    if (_11397)
    {
        if (_11391 == 0u)
        {
            return b;
        }
        if (_11394 == 0u)
        {
            return a;
        }
        bool _11404;
        if (_11391 >= 8388608u)
        {
            _11404 = _11394 < 8388608u;
        }
        else
        {
            _11404 = true;
        }
        if (_11404)
        {
            intervalFailed = true;
        }
        bool _11407;
        if (_11391 >= 562036736u)
        {
            _11407 = _11391 <= 1568669696u;
        }
        else
        {
            _11407 = false;
        }
        bool _11409;
        if (_11407)
        {
            _11409 = _11394 >= 562036736u;
        }
        else
        {
            _11409 = false;
        }
        bool _11411;
        if (_11409)
        {
            _11411 = _11394 <= 1568669696u;
        }
        else
        {
            _11411 = false;
        }
        if (_11411)
        {
            float _11415;
            if (_11391 >= _11394)
            {
                _11415 = a;
            }
            else
            {
                _11415 = b;
            }
            float _11419;
            if (_11391 >= _11394)
            {
                _11419 = b;
            }
            else
            {
                _11419 = a;
            }
            float _1705 = spvFAdd(_11415, _11419);
            float _1707 = spvFSub(_11419, spvFSub(_1705, _11415));
            if (upper)
            {
                float _11423;
                if (_1707 > 0.0)
                {
                    float param_var_x = _1705;
                    float _11422 = interval_up(param_var_x, intervalFailed);
                    _11423 = _11422;
                }
                else
                {
                    _11423 = _1705;
                }
                return _11423;
            }
            float _11426;
            if (_1707 < 0.0)
            {
                float param_var_x_1 = _1705;
                float _11425 = interval_down(param_var_x_1, intervalFailed);
                _11426 = _11425;
            }
            else
            {
                _11426 = _1705;
            }
            return _11426;
        }
    }
    float _1708 = spvFAdd(a, b);
    float _11432;
    if (upper)
    {
        float param_var_x_2 = _1708;
        float _11430 = interval_up(param_var_x_2, intervalFailed);
        _11432 = _11430;
    }
    else
    {
        float param_var_x_3 = _1708;
        float _11431 = interval_down(param_var_x_3, intervalFailed);
        _11432 = _11431;
    }
    return _11432;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _4485;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _4485 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _4485 = false;
    }
    bool _4489;
    if (_4485)
    {
        _4489 = a.lo <= a.hi;
    }
    else
    {
        _4489 = false;
    }
    bool _4508;
    if (_4489)
    {
        bool _4503;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _4503 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _4503 = false;
        }
        bool _4507;
        if (_4503)
        {
            _4507 = b.lo <= b.hi;
        }
        else
        {
            _4507 = false;
        }
        _4508 = _4507;
    }
    else
    {
        _4508 = false;
    }
    if (_4508)
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
    float _4519 = optical_add_bound(param_var_a_2, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_3 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _4524 = optical_add_bound(param_var_a_3, param_var_b_1, param_var_upper_1, intervalFailed);
    return Interval{ _4519, _4524 };
}

static inline __attribute__((always_inline))
float optical_product_bounds(thread const float& a, thread const float& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    uint _15931 = as_type<uint>(a);
    uint _15932 = _15931 & 2147483647u;
    uint _15934 = as_type<uint>(b);
    uint _15935 = _15934 & 2147483647u;
    bool _15938;
    if (_15932 < 2139095040u)
    {
        _15938 = _15935 < 2139095040u;
    }
    else
    {
        _15938 = false;
    }
    if (_15938)
    {
        bool _15941;
        if (_15932 != 0u)
        {
            _15941 = _15935 == 0u;
        }
        else
        {
            _15941 = true;
        }
        if (_15941)
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
            float _15958 = as_type<float>(as_type<uint>(b) ^ 2147483648u);
            optical_product_upper = _15958;
            return _15958;
        }
        if (as_type<uint>(b) == 3212836864u)
        {
            float _15965 = as_type<float>(as_type<uint>(a) ^ 2147483648u);
            optical_product_upper = _15965;
            return _15965;
        }
        bool _15968;
        if (_15932 >= 8388608u)
        {
            _15968 = _15935 < 8388608u;
        }
        else
        {
            _15968 = true;
        }
        if (_15968)
        {
            intervalFailed = true;
        }
        bool _15971;
        if (_15932 >= 813694976u)
        {
            _15971 = _15932 <= 1317011456u;
        }
        else
        {
            _15971 = false;
        }
        bool _15973;
        if (_15971)
        {
            _15973 = _15935 >= 813694976u;
        }
        else
        {
            _15973 = false;
        }
        bool _15975;
        if (_15973)
        {
            _15975 = _15935 <= 1317011456u;
        }
        else
        {
            _15975 = false;
        }
        if (_15975)
        {
            float _1714 = spvFMul(a, b);
            uint _15982 = _15931 & 65535u;
            uint _15983 = ((_15931 & 8388607u) | 8388608u) >> 16u;
            uint _15984 = _15934 & 65535u;
            uint _15985 = ((_15934 & 8388607u) | 8388608u) >> 16u;
            uint _1715 = _15982 * _15984;
            uint _1718 = (_15982 * _15985) + (_15983 * _15984);
            uint _1719 = _1715 + (_1718 << 16u);
            uint _15989;
            if (_1719 < _1715)
            {
                _15989 = 1u;
            }
            else
            {
                _15989 = 0u;
            }
            uint _1722 = ((_15983 * _15985) + (_1718 >> 16u)) + _15989;
            uint _15990 = as_type<uint>(_1714);
            uint _1724 = (((_15990 & 2147483647u) >> 23u) - (_15932 >> 23u)) - (_15935 >> 23u);
            uint _1725 = _1724 + 150u;
            bool _15997;
            if (_1725 >= 23u)
            {
                _15997 = _1725 > 24u;
            }
            else
            {
                _15997 = true;
            }
            if (_15997)
            {
                intervalFailed = true;
                float param_var_x = _1714;
                float _15998 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _15998;
                float param_var_x_1 = _1714;
                float _15999 = interval_down(param_var_x_1, intervalFailed);
                return _15999;
            }
            uint _16001 = (_15990 & 8388607u) | 8388608u;
            uint _16003 = _16001 << (_1725 & 31u);
            uint _16005 = _16001 >> ((4294967178u - _1724) & 31u);
            bool _16010;
            if (_1722 <= _16005)
            {
                bool _16009;
                if (_1722 == _16005)
                {
                    _16009 = _1719 > _16003;
                }
                else
                {
                    _16009 = false;
                }
                _16010 = _16009;
            }
            else
            {
                _16010 = true;
            }
            bool _16015;
            if (_1722 >= _16005)
            {
                bool _16014;
                if (_1722 == _16005)
                {
                    _16014 = _1719 < _16003;
                }
                else
                {
                    _16014 = false;
                }
                _16015 = _16014;
            }
            else
            {
                _16015 = true;
            }
            bool _16022 = ((as_type<uint>(a) ^ as_type<uint>(b)) & 2147483648u) != 0u;
            bool _16023;
            if (_16022)
            {
                _16023 = _16015;
            }
            else
            {
                _16023 = _16010;
            }
            float _16025;
            if (_16023)
            {
                float param_var_x_2 = _1714;
                float _16024 = interval_up(param_var_x_2, intervalFailed);
                _16025 = _16024;
            }
            else
            {
                _16025 = _1714;
            }
            optical_product_upper = _16025;
            bool _16026;
            if (_16022)
            {
                _16026 = _16010;
            }
            else
            {
                _16026 = _16015;
            }
            float _16028;
            if (_16026)
            {
                float param_var_x_3 = _1714;
                float _16027 = interval_down(param_var_x_3, intervalFailed);
                _16028 = _16027;
            }
            else
            {
                _16028 = _1714;
            }
            return _16028;
        }
    }
    float _1727 = spvFMul(a, b);
    float param_var_x_4 = _1727;
    float _16031 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _16031;
    float param_var_x_5 = _1727;
    float _16032 = interval_down(param_var_x_5, intervalFailed);
    return _16032;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _11446;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11446 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11446 = false;
    }
    bool _11450;
    if (_11446)
    {
        _11450 = a.lo <= a.hi;
    }
    else
    {
        _11450 = false;
    }
    bool _11469;
    if (_11450)
    {
        bool _11464;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11464 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11464 = false;
        }
        bool _11468;
        if (_11464)
        {
            _11468 = b.lo <= b.hi;
        }
        else
        {
            _11468 = false;
        }
        _11469 = _11468;
    }
    else
    {
        _11469 = false;
    }
    if (_11469)
    {
        Interval param_var_a = a;
        float param_var_x = 0.0;
        bool _11475;
        if (!interval_exact_point(param_var_a, param_var_x))
        {
            Interval param_var_a_1 = b;
            float param_var_x_1 = 0.0;
            _11475 = interval_exact_point(param_var_a_1, param_var_x_1);
        }
        else
        {
            _11475 = true;
        }
        if (_11475)
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
    bool _11550;
    if (_11542)
    {
        _11550 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    else
    {
        _11550 = false;
    }
    bool _11558;
    if (_11542)
    {
        _11558 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    else
    {
        _11558 = false;
    }
    float param_var_a_6 = a.lo;
    float param_var_b = b.lo;
    float _11563 = optical_product_bounds(param_var_a_6, param_var_b, intervalFailed, optical_product_upper);
    float _11572;
    float _11573;
    if (!_11558)
    {
        float param_var_a_7 = a.lo;
        float param_var_b_1 = b.hi;
        float _11570 = optical_product_bounds(param_var_a_7, param_var_b_1, intervalFailed, optical_product_upper);
        _11572 = optical_product_upper;
        _11573 = _11570;
    }
    else
    {
        _11572 = optical_product_upper;
        _11573 = _11563;
    }
    float _11581;
    float _11582;
    if (!_11550)
    {
        float param_var_a_8 = a.hi;
        float param_var_b_2 = b.lo;
        float _11579 = optical_product_bounds(param_var_a_8, param_var_b_2, intervalFailed, optical_product_upper);
        _11581 = optical_product_upper;
        _11582 = _11579;
    }
    else
    {
        _11581 = optical_product_upper;
        _11582 = _11563;
    }
    float _11592;
    float _11593;
    if (!_11550)
    {
        float _11590;
        float _11591;
        if (_11558)
        {
            _11590 = _11581;
            _11591 = _11582;
        }
        else
        {
            float param_var_a_9 = a.hi;
            float param_var_b_3 = b.hi;
            float _11588 = optical_product_bounds(param_var_a_9, param_var_b_3, intervalFailed, optical_product_upper);
            _11590 = optical_product_upper;
            _11591 = _11588;
        }
        _11592 = _11590;
        _11593 = _11591;
    }
    else
    {
        _11592 = _11572;
        _11593 = _11573;
    }
    return Interval{ precise::min(precise::min(_11563, _11573), precise::min(_11582, _11593)), precise::max(precise::max(optical_product_upper, _11572), precise::max(_11581, _11592)) };
}

static inline __attribute__((always_inline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float _21469;
    _21469 = precise::sqrt(a);
    float _21470;
    for (uint _21471 = 0u; _21471 < 8u; _21469 = _21470, _21471++)
    {
        float _1807 = spvFMul(_21469, _21469);
        float _1808 = spvFMul(_21469, 4097.0);
        float _1810 = spvFSub(_1808, spvFSub(_1808, _21469));
        float _1811 = spvFSub(_21469, _1810);
        float _1812 = spvFMul(_21469, 4097.0);
        float _1814 = spvFSub(_1812, spvFSub(_1812, _21469));
        float _1815 = spvFSub(_21469, _1814);
        float _1829 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1810, _1814), _1807), spvFMul(_1810, _1815)), spvFMul(_1811, _1814)), spvFMul(_1811, _1815)), spvFMul(_21469, 0.0)), spvFMul(0.0, _21469)), spvFMul(0.0, 0.0));
        float _1830 = spvFAdd(_1807, _1829);
        float _1832 = spvFSub(_1829, spvFSub(_1830, _1807));
        bool _21480;
        if (!(isnan(_1830) || isinf(_1830)))
        {
            _21480 = isnan(_1832) || isinf(_1832);
        }
        else
        {
            _21480 = true;
        }
        if (_21480)
        {
            intervalFailed = true;
            return _21469;
        }
        bool _21487;
        if ((isunordered(_1830, a) || _1830 >= a))
        {
            bool _21486;
            if (_1830 == a)
            {
                _21486 = _1832 < 0.0;
            }
            else
            {
                _21486 = false;
            }
            _21487 = _21486;
        }
        else
        {
            _21487 = true;
        }
        bool _21494;
        if ((isunordered(a, _1830) || a >= _1830))
        {
            bool _21493;
            if (a == _1830)
            {
                _21493 = 0.0 < _1832;
            }
            else
            {
                _21493 = false;
            }
            _21494 = _21493;
        }
        else
        {
            _21494 = true;
        }
        bool _21498;
        if (upper)
        {
            _21498 = !_21487;
        }
        else
        {
            _21498 = !_21494;
        }
        if (_21498)
        {
            return _21469;
        }
        if (upper)
        {
            float param_var_x = _21469;
            float _21500 = interval_up(param_var_x, intervalFailed);
            _21470 = _21500;
        }
        else
        {
            float param_var_x_1 = _21469;
            float _21501 = interval_down(param_var_x_1, intervalFailed);
            _21470 = precise::max(0.0, _21501);
        }
    }
    intervalFailed = true;
    return _21469;
}

static inline __attribute__((always_inline))
Interval isqrt(thread const Interval& a, thread bool& intervalFailed)
{
    bool _16039;
    if ((isunordered(a.hi, 0.0) || a.hi >= 0.0))
    {
        _16039 = a.hi > 1000000015047466219876688855040.0;
    }
    else
    {
        _16039 = true;
    }
    if (_16039)
    {
        intervalFailed = true;
        return Interval{ 0.0, 1000000015047466219876688855040.0 };
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _16043 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _16043;
    float _16044 = interval_down(param_var_x, intervalFailed);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _16049 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _16049;
    float _16050 = interval_up(param_var_x_1, intervalFailed);
    return Interval{ precise::max(0.0, _16044), _16050 };
}

static inline __attribute__((always_inline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float _21505;
    _21505 = a / b;
    float _21506;
    for (uint _21507 = 0u; _21507 < 8u; _21505 = _21506, _21507++)
    {
        bool _21515;
        if (!(isnan(_21505) || isinf(_21505)))
        {
            _21515 = abs(_21505) > 1000000015047466219876688855040.0;
        }
        else
        {
            _21515 = true;
        }
        if (_21515)
        {
            intervalFailed = true;
            return _21505;
        }
        float _1857 = spvFMul(_21505, b);
        float _1858 = spvFMul(_21505, 4097.0);
        float _1860 = spvFSub(_1858, spvFSub(_1858, _21505));
        float _1861 = spvFSub(_21505, _1860);
        float _1862 = spvFMul(b, 4097.0);
        float _1864 = spvFSub(_1862, spvFSub(_1862, b));
        float _1865 = spvFSub(b, _1864);
        float _1879 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1860, _1864), _1857), spvFMul(_1860, _1865)), spvFMul(_1861, _1864)), spvFMul(_1861, _1865)), spvFMul(_21505, 0.0)), spvFMul(0.0, b)), spvFMul(0.0, 0.0));
        float _1880 = spvFAdd(_1857, _1879);
        float _1882 = spvFSub(_1879, spvFSub(_1880, _1857));
        bool _21524;
        if (!(isnan(_1880) || isinf(_1880)))
        {
            _21524 = isnan(_1882) || isinf(_1882);
        }
        else
        {
            _21524 = true;
        }
        if (_21524)
        {
            intervalFailed = true;
            return _21505;
        }
        bool _21531;
        if ((isunordered(_1880, a) || _1880 >= a))
        {
            bool _21530;
            if (_1880 == a)
            {
                _21530 = _1882 < 0.0;
            }
            else
            {
                _21530 = false;
            }
            _21531 = _21530;
        }
        else
        {
            _21531 = true;
        }
        bool _21538;
        if ((isunordered(a, _1880) || a >= _1880))
        {
            bool _21537;
            if (a == _1880)
            {
                _21537 = 0.0 < _1882;
            }
            else
            {
                _21537 = false;
            }
            _21538 = _21537;
        }
        else
        {
            _21538 = true;
        }
        if (b < 0.0)
        {
            bool _21544;
            if (upper)
            {
                _21544 = !_21538;
            }
            else
            {
                _21544 = !_21531;
            }
            if (_21544)
            {
                return _21505;
            }
        }
        else
        {
            bool _21548;
            if (upper)
            {
                _21548 = !_21531;
            }
            else
            {
                _21548 = !_21538;
            }
            if (_21548)
            {
                return _21505;
            }
        }
        if (upper)
        {
            float param_var_x = _21505;
            float _21550 = interval_up(param_var_x, intervalFailed);
            _21506 = _21550;
        }
        else
        {
            float param_var_x_1 = _21505;
            float _21551 = interval_down(param_var_x_1, intervalFailed);
            _21506 = _21551;
        }
    }
    intervalFailed = true;
    return _21505;
}

static inline __attribute__((always_inline))
float interval_divide_pair(thread const float& alo, thread const float& ahi, thread const float& blo, thread const float& bhi, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _16064;
    if (!(isnan(alo) || isinf(alo)))
    {
        _16064 = !(isnan(ahi) || isinf(ahi));
    }
    else
    {
        _16064 = false;
    }
    bool _16068;
    if (_16064)
    {
        _16068 = alo <= ahi;
    }
    else
    {
        _16068 = false;
    }
    bool _16086;
    if (_16068)
    {
        bool _16081;
        if (!(isnan(blo) || isinf(blo)))
        {
            _16081 = !(isnan(bhi) || isinf(bhi));
        }
        else
        {
            _16081 = false;
        }
        bool _16085;
        if (_16081)
        {
            _16085 = blo <= bhi;
        }
        else
        {
            _16085 = false;
        }
        _16086 = _16085;
    }
    else
    {
        _16086 = false;
    }
    bool _16092;
    if (_16086)
    {
        _16092 = as_type<uint>(alo) == as_type<uint>(ahi);
    }
    else
    {
        _16092 = false;
    }
    bool _16098;
    if (_16086)
    {
        _16098 = as_type<uint>(blo) == as_type<uint>(bhi);
    }
    else
    {
        _16098 = false;
    }
    float param_var_a = alo;
    float param_var_b = blo;
    bool param_var_upper = false;
    float _16101 = quotient_bound(param_var_a, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_1 = alo;
    float param_var_b_1 = blo;
    bool param_var_upper_1 = true;
    float _16104 = quotient_bound(param_var_a_1, param_var_b_1, param_var_upper_1, intervalFailed);
    float _16112;
    float _16113;
    if (!_16098)
    {
        float param_var_a_2 = alo;
        float param_var_b_2 = bhi;
        bool param_var_upper_2 = false;
        float _16108 = quotient_bound(param_var_a_2, param_var_b_2, param_var_upper_2, intervalFailed);
        float param_var_a_3 = alo;
        float param_var_b_3 = bhi;
        bool param_var_upper_3 = true;
        float _16111 = quotient_bound(param_var_a_3, param_var_b_3, param_var_upper_3, intervalFailed);
        _16112 = _16111;
        _16113 = _16108;
    }
    else
    {
        _16112 = _16104;
        _16113 = _16101;
    }
    float _16121;
    float _16122;
    if (!_16092)
    {
        float param_var_a_4 = ahi;
        float param_var_b_4 = blo;
        bool param_var_upper_4 = false;
        float _16117 = quotient_bound(param_var_a_4, param_var_b_4, param_var_upper_4, intervalFailed);
        float param_var_a_5 = ahi;
        float param_var_b_5 = blo;
        bool param_var_upper_5 = true;
        float _16120 = quotient_bound(param_var_a_5, param_var_b_5, param_var_upper_5, intervalFailed);
        _16121 = _16120;
        _16122 = _16117;
    }
    else
    {
        _16121 = _16104;
        _16122 = _16101;
    }
    float _16132;
    float _16133;
    if (!_16092)
    {
        float _16130;
        float _16131;
        if (_16098)
        {
            _16130 = _16121;
            _16131 = _16122;
        }
        else
        {
            float param_var_a_6 = ahi;
            float param_var_b_6 = bhi;
            bool param_var_upper_6 = false;
            float _16126 = quotient_bound(param_var_a_6, param_var_b_6, param_var_upper_6, intervalFailed);
            float param_var_a_7 = ahi;
            float param_var_b_7 = bhi;
            bool param_var_upper_7 = true;
            float _16129 = quotient_bound(param_var_a_7, param_var_b_7, param_var_upper_7, intervalFailed);
            _16130 = _16129;
            _16131 = _16126;
        }
        _16132 = _16130;
        _16133 = _16131;
    }
    else
    {
        _16132 = _16112;
        _16133 = _16113;
    }
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _16104), _16112), _16121), _16132);
    return precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _16101), _16113), _16122), _16133);
}

static inline __attribute__((always_inline))
Interval idiv(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _11608;
    if (b.lo <= 0.0)
    {
        _11608 = b.hi >= 0.0;
    }
    else
    {
        _11608 = false;
    }
    bool _11618;
    if (!_11608)
    {
        _11618 = precise::max(abs(b.lo), abs(b.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _11618 = true;
    }
    bool _11628;
    if (!_11618)
    {
        _11628 = precise::max(abs(a.lo), abs(a.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _11628 = true;
    }
    if (_11628)
    {
        intervalFailed = true;
        return Interval{ -1000000015047466219876688855040.0, 1000000015047466219876688855040.0 };
    }
    bool _11642;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _11642 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _11642 = false;
    }
    bool _11646;
    if (_11642)
    {
        _11646 = a.lo <= a.hi;
    }
    else
    {
        _11646 = false;
    }
    bool _11665;
    if (_11646)
    {
        bool _11660;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _11660 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _11660 = false;
        }
        bool _11664;
        if (_11660)
        {
            _11664 = b.lo <= b.hi;
        }
        else
        {
            _11664 = false;
        }
        _11665 = _11664;
    }
    else
    {
        _11665 = false;
    }
    if (_11665)
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
    float _11691 = interval_divide_pair(param_var_alo, param_var_ahi, param_var_blo, param_var_bhi, intervalFailed, interval_divide_upper);
    float param_var_x_3 = _11691;
    float _11692 = interval_down(param_var_x_3, intervalFailed);
    float param_var_x_4 = interval_divide_upper;
    float _11694 = interval_up(param_var_x_4, intervalFailed);
    return Interval{ _11692, _11694 };
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, device type_ByteAddressBuffer& frames, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        Interval param_var_a = Interval{ feature.x, feature.x };
        Interval param_var_b = Interval{ feature.y, feature.y };
        Interval _3742 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval _3731 = Interval{ 1.0, 1.0 };
        Interval _3732 = Interval{ as_type<float>(as_type<uint>(_3742.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_3742.lo) ^ 2147483648u) };
        Interval _3752 = iadd(_3731, _3732, intervalFailed);
        uint _3754 = (at + 432u) >> 2u;
        uint _3756 = frames._m0[_3754];
        uint _3758 = frames._m0[_3754 + 1u];
        uint _3760 = frames._m0[_3754 + 2u];
        uint _3762 = frames._m0[_3754 + 3u];
        float4 _3764 = as_type<float4>(uint4(_3756, _3758, _3760, _3762));
        float _3765 = _3764.x;
        float _3766 = _3764.y;
        float _3767 = _3764.z;
        Interval _3725 = Interval{ _3765, _3765 };
        Interval _3726 = _3752;
        Interval _3769 = imul(_3725, _3726, intervalFailed, optical_product_upper);
        Interval _3727 = Interval{ _3766, _3766 };
        Interval _3728 = _3752;
        Interval _3771 = imul(_3727, _3728, intervalFailed, optical_product_upper);
        Interval _3729 = Interval{ _3767, _3767 };
        Interval _3730 = _3752;
        Interval _3773 = imul(_3729, _3730, intervalFailed, optical_product_upper);
        uint _3775 = (at + 448u) >> 2u;
        uint _3777 = frames._m0[_3775];
        uint _3779 = frames._m0[_3775 + 1u];
        uint _3781 = frames._m0[_3775 + 2u];
        uint _3783 = frames._m0[_3775 + 3u];
        float4 _3785 = as_type<float4>(uint4(_3777, _3779, _3781, _3783));
        float _3786 = _3785.x;
        float _3787 = _3785.y;
        float _3788 = _3785.z;
        Interval _3719 = Interval{ _3786, _3786 };
        Interval _3720 = Interval{ feature.x, feature.x };
        Interval _3791 = imul(_3719, _3720, intervalFailed, optical_product_upper);
        Interval _3721 = Interval{ _3787, _3787 };
        Interval _3722 = Interval{ feature.x, feature.x };
        Interval _3794 = imul(_3721, _3722, intervalFailed, optical_product_upper);
        Interval _3723 = Interval{ _3788, _3788 };
        Interval _3724 = Interval{ feature.x, feature.x };
        Interval _3797 = imul(_3723, _3724, intervalFailed, optical_product_upper);
        Interval _3713 = _3769;
        Interval _3714 = _3791;
        Interval _3798 = iadd(_3713, _3714, intervalFailed);
        Interval _3715 = _3771;
        Interval _3716 = _3794;
        Interval _3799 = iadd(_3715, _3716, intervalFailed);
        Interval _3717 = _3773;
        Interval _3718 = _3797;
        Interval _3800 = iadd(_3717, _3718, intervalFailed);
        uint _3802 = (at + 464u) >> 2u;
        uint _3804 = frames._m0[_3802];
        uint _3806 = frames._m0[_3802 + 1u];
        uint _3808 = frames._m0[_3802 + 2u];
        uint _3810 = frames._m0[_3802 + 3u];
        float4 _3812 = as_type<float4>(uint4(_3804, _3806, _3808, _3810));
        float _3813 = _3812.x;
        float _3814 = _3812.y;
        float _3815 = _3812.z;
        Interval _3707 = Interval{ _3813, _3813 };
        Interval _3708 = Interval{ feature.y, feature.y };
        Interval _3818 = imul(_3707, _3708, intervalFailed, optical_product_upper);
        Interval _3709 = Interval{ _3814, _3814 };
        Interval _3710 = Interval{ feature.y, feature.y };
        Interval _3821 = imul(_3709, _3710, intervalFailed, optical_product_upper);
        Interval _3711 = Interval{ _3815, _3815 };
        Interval _3712 = Interval{ feature.y, feature.y };
        Interval _3824 = imul(_3711, _3712, intervalFailed, optical_product_upper);
        Interval _3701 = _3798;
        Interval _3702 = _3818;
        Interval _3825 = iadd(_3701, _3702, intervalFailed);
        Interval _3703 = _3799;
        Interval _3704 = _3821;
        Interval _3826 = iadd(_3703, _3704, intervalFailed);
        Interval _3705 = _3800;
        Interval _3706 = _3824;
        Interval _3827 = iadd(_3705, _3706, intervalFailed);
        return Interval3{ _3825, _3826, _3827 };
    }
    Interval _3691 = Interval{ TargetSettings.targetCurrentCube[0].x, TargetSettings.targetCurrentCube[0].x };
    Interval _3692 = Interval{ feature.x, feature.x };
    Interval _3841 = imul(_3691, _3692, intervalFailed, optical_product_upper);
    Interval _3693 = _3841;
    Interval _3694 = Interval{ TargetSettings.targetCurrentCube[0].y, TargetSettings.targetCurrentCube[0].y };
    Interval _3695 = Interval{ feature.y, feature.y };
    Interval _3844 = imul(_3694, _3695, intervalFailed, optical_product_upper);
    Interval _3696 = _3844;
    Interval _3845 = iadd(_3693, _3696, intervalFailed);
    Interval _3697 = _3845;
    Interval _3698 = Interval{ TargetSettings.targetCurrentCube[0].z, TargetSettings.targetCurrentCube[0].z };
    Interval _3699 = Interval{ feature.z, feature.z };
    Interval _3848 = imul(_3698, _3699, intervalFailed, optical_product_upper);
    Interval _3700 = _3848;
    Interval _3849 = iadd(_3697, _3700, intervalFailed);
    Interval _3681 = Interval{ TargetSettings.targetCurrentCube[1].x, TargetSettings.targetCurrentCube[1].x };
    Interval _3682 = Interval{ feature.x, feature.x };
    Interval _3858 = imul(_3681, _3682, intervalFailed, optical_product_upper);
    Interval _3683 = _3858;
    Interval _3684 = Interval{ TargetSettings.targetCurrentCube[1].y, TargetSettings.targetCurrentCube[1].y };
    Interval _3685 = Interval{ feature.y, feature.y };
    Interval _3861 = imul(_3684, _3685, intervalFailed, optical_product_upper);
    Interval _3686 = _3861;
    Interval _3862 = iadd(_3683, _3686, intervalFailed);
    Interval _3687 = _3862;
    Interval _3688 = Interval{ TargetSettings.targetCurrentCube[1].z, TargetSettings.targetCurrentCube[1].z };
    Interval _3689 = Interval{ feature.z, feature.z };
    Interval _3865 = imul(_3688, _3689, intervalFailed, optical_product_upper);
    Interval _3690 = _3865;
    Interval _3866 = iadd(_3687, _3690, intervalFailed);
    Interval _3671 = Interval{ TargetSettings.targetCurrentCube[2].x, TargetSettings.targetCurrentCube[2].x };
    Interval _3672 = Interval{ feature.x, feature.x };
    Interval _3875 = imul(_3671, _3672, intervalFailed, optical_product_upper);
    Interval _3673 = _3875;
    Interval _3674 = Interval{ TargetSettings.targetCurrentCube[2].y, TargetSettings.targetCurrentCube[2].y };
    Interval _3675 = Interval{ feature.y, feature.y };
    Interval _3878 = imul(_3674, _3675, intervalFailed, optical_product_upper);
    Interval _3676 = _3878;
    Interval _3879 = iadd(_3673, _3676, intervalFailed);
    Interval _3677 = _3879;
    Interval _3678 = Interval{ TargetSettings.targetCurrentCube[2].z, TargetSettings.targetCurrentCube[2].z };
    Interval _3679 = Interval{ feature.z, feature.z };
    Interval _3882 = imul(_3678, _3679, intervalFailed, optical_product_upper);
    Interval _3680 = _3882;
    Interval _3883 = iadd(_3677, _3680, intervalFailed);
    Interval _3661 = Interval{ TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[0].x };
    Interval _3662 = _3849;
    Interval _3897 = imul(_3661, _3662, intervalFailed, optical_product_upper);
    Interval _3663 = _3897;
    Interval _3664 = Interval{ TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[1].x };
    Interval _3665 = _3866;
    Interval _3899 = imul(_3664, _3665, intervalFailed, optical_product_upper);
    Interval _3666 = _3899;
    Interval _3900 = iadd(_3663, _3666, intervalFailed);
    Interval _3667 = _3900;
    Interval _3668 = Interval{ TargetSettings.targetPreviousCube[2].x, TargetSettings.targetPreviousCube[2].x };
    Interval _3669 = _3883;
    Interval _3902 = imul(_3668, _3669, intervalFailed, optical_product_upper);
    Interval _3670 = _3902;
    Interval _3903 = iadd(_3667, _3670, intervalFailed);
    Interval _3651 = Interval{ TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[0].y };
    Interval _3652 = _3849;
    Interval _3917 = imul(_3651, _3652, intervalFailed, optical_product_upper);
    Interval _3653 = _3917;
    Interval _3654 = Interval{ TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[1].y };
    Interval _3655 = _3866;
    Interval _3919 = imul(_3654, _3655, intervalFailed, optical_product_upper);
    Interval _3656 = _3919;
    Interval _3920 = iadd(_3653, _3656, intervalFailed);
    Interval _3657 = _3920;
    Interval _3658 = Interval{ TargetSettings.targetPreviousCube[2].y, TargetSettings.targetPreviousCube[2].y };
    Interval _3659 = _3883;
    Interval _3922 = imul(_3658, _3659, intervalFailed, optical_product_upper);
    Interval _3660 = _3922;
    Interval _3923 = iadd(_3657, _3660, intervalFailed);
    Interval _3641 = Interval{ TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[0].z };
    Interval _3642 = _3849;
    Interval _3937 = imul(_3641, _3642, intervalFailed, optical_product_upper);
    Interval _3643 = _3937;
    Interval _3644 = Interval{ TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[1].z };
    Interval _3645 = _3866;
    Interval _3939 = imul(_3644, _3645, intervalFailed, optical_product_upper);
    Interval _3646 = _3939;
    Interval _3940 = iadd(_3643, _3646, intervalFailed);
    Interval _3647 = _3940;
    Interval _3648 = Interval{ TargetSettings.targetPreviousCube[2].z, TargetSettings.targetPreviousCube[2].z };
    Interval _3649 = _3883;
    Interval _3942 = imul(_3648, _3649, intervalFailed, optical_product_upper);
    Interval _3650 = _3942;
    Interval _3943 = iadd(_3647, _3650, intervalFailed);
    Interval _3639 = Interval{ 1.0, 1.0 };
    bool _3950;
    if (_3903.lo <= 0.0)
    {
        _3950 = _3903.hi >= 0.0;
    }
    else
    {
        _3950 = false;
    }
    float _3957;
    if (_3950)
    {
        _3957 = 0.0;
    }
    else
    {
        _3957 = precise::min(abs(_3903.lo), abs(_3903.hi));
    }
    float _3960 = precise::max(abs(_3903.lo), abs(_3903.hi));
    float _3632 = spvFMul(_3957, _3957);
    float _3961 = interval_down(_3632, intervalFailed);
    float _3633 = spvFMul(_3960, _3960);
    float _3963 = interval_up(_3633, intervalFailed);
    Interval _3634 = Interval{ precise::max(0.0, _3961), _3963 };
    bool _3971;
    if (_3923.lo <= 0.0)
    {
        _3971 = _3923.hi >= 0.0;
    }
    else
    {
        _3971 = false;
    }
    float _3978;
    if (_3971)
    {
        _3978 = 0.0;
    }
    else
    {
        _3978 = precise::min(abs(_3923.lo), abs(_3923.hi));
    }
    float _3981 = precise::max(abs(_3923.lo), abs(_3923.hi));
    float _3630 = spvFMul(_3978, _3978);
    float _3982 = interval_down(_3630, intervalFailed);
    float _3631 = spvFMul(_3981, _3981);
    float _3984 = interval_up(_3631, intervalFailed);
    Interval _3635 = Interval{ precise::max(0.0, _3982), _3984 };
    Interval _3986 = iadd(_3634, _3635, intervalFailed);
    Interval _3636 = _3986;
    bool _3993;
    if (_3943.lo <= 0.0)
    {
        _3993 = _3943.hi >= 0.0;
    }
    else
    {
        _3993 = false;
    }
    float _4000;
    if (_3993)
    {
        _4000 = 0.0;
    }
    else
    {
        _4000 = precise::min(abs(_3943.lo), abs(_3943.hi));
    }
    float _4003 = precise::max(abs(_3943.lo), abs(_3943.hi));
    float _3628 = spvFMul(_4000, _4000);
    float _4004 = interval_down(_3628, intervalFailed);
    float _3629 = spvFMul(_4003, _4003);
    float _4006 = interval_up(_3629, intervalFailed);
    Interval _3637 = Interval{ precise::max(0.0, _4004), _4006 };
    Interval _4008 = iadd(_3636, _3637, intervalFailed);
    Interval _3638 = _4008;
    Interval _4009 = isqrt(_3638, intervalFailed);
    Interval _3640 = _4009;
    Interval _4010 = idiv(_3639, _3640, intervalFailed, interval_divide_upper);
    Interval _3622 = _3903;
    Interval _3623 = _4010;
    Interval _4011 = imul(_3622, _3623, intervalFailed, optical_product_upper);
    Interval _3624 = _3923;
    Interval _3625 = _4010;
    Interval _4012 = imul(_3624, _3625, intervalFailed, optical_product_upper);
    Interval _3626 = _3943;
    Interval _3627 = _4010;
    Interval _4013 = imul(_3626, _3627, intervalFailed, optical_product_upper);
    return Interval3{ _4011, _4012, _4013 };
}

static inline __attribute__((always_inline))
Interval jet_add_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _16170;
    if (a.lo == 0.0)
    {
        _16170 = a.hi == 0.0;
    }
    else
    {
        _16170 = false;
    }
    if (_16170)
    {
        return b;
    }
    bool _16179;
    if (b.lo == 0.0)
    {
        _16179 = b.hi == 0.0;
    }
    else
    {
        _16179 = false;
    }
    if (_16179)
    {
        return a;
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16183 = iadd(param_var_a, param_var_b, intervalFailed);
    return _16183;
}

static inline __attribute__((always_inline))
Interval jet_mul_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _16149;
    if (a.lo == 0.0)
    {
        _16149 = a.hi == 0.0;
    }
    else
    {
        _16149 = false;
    }
    bool _16159;
    if (!_16149)
    {
        bool _16158;
        if (b.lo == 0.0)
        {
            _16158 = b.hi == 0.0;
        }
        else
        {
            _16158 = false;
        }
        _16159 = _16158;
    }
    else
    {
        _16159 = true;
    }
    if (_16159)
    {
        return Interval{ 0.0, 0.0 };
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16162 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    return _16162;
}

static inline __attribute__((always_inline))
Interval jet_div_derivative(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    bool _16191;
    if (a.lo == 0.0)
    {
        _16191 = a.hi == 0.0;
    }
    else
    {
        _16191 = false;
    }
    bool _16201;
    if (_16191)
    {
        bool _16199;
        if (b.lo <= 0.0)
        {
            _16199 = b.hi >= 0.0;
        }
        else
        {
            _16199 = false;
        }
        _16201 = !_16199;
    }
    else
    {
        _16201 = false;
    }
    if (_16201)
    {
        return Interval{ 0.0, 0.0 };
    }
    Interval param_var_a = a;
    Interval param_var_b = b;
    Interval _16205 = idiv(param_var_a, param_var_b, intervalFailed, interval_divide_upper);
    bool _16216;
    if (!intervalFailed)
    {
        _16216 = intervalFailed;
    }
    else
    {
        _16216 = false;
    }
    bool _16221;
    if (_16216)
    {
        _16221 = jetFailureSite == 0u;
    }
    else
    {
        _16221 = false;
    }
    if (_16221)
    {
        jetFailureSite = 2u;
        jetFailureArguments = float4(a.lo, a.hi, b.lo, b.hi);
    }
    return _16205;
}

static inline __attribute__((always_inline))
Interval3 plane_normal(thread const ReflectionSpecularPlane& p, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    bool _16303;
    if (p.a.w == 2.0)
    {
        _16303 = p.b.w == 2.0;
    }
    else
    {
        _16303 = false;
    }
    bool _16308;
    if (_16303)
    {
        _16308 = p.c.w == 2.0;
    }
    else
    {
        _16308 = false;
    }
    if (_16308)
    {
        Interval _16291 = Interval{ 1.0, 1.0 };
        bool _16318;
        if (p.b.x <= 0.0)
        {
            _16318 = p.b.x >= 0.0;
        }
        else
        {
            _16318 = false;
        }
        float _16325;
        if (_16318)
        {
            _16325 = 0.0;
        }
        else
        {
            _16325 = precise::min(abs(p.b.x), abs(p.b.x));
        }
        float _16328 = precise::max(abs(p.b.x), abs(p.b.x));
        float _16284 = spvFMul(_16325, _16325);
        float _16329 = interval_down(_16284, intervalFailed);
        float _16285 = spvFMul(_16328, _16328);
        float _16331 = interval_up(_16285, intervalFailed);
        Interval _16286 = Interval{ precise::max(0.0, _16329), _16331 };
        bool _16337;
        if (p.b.y <= 0.0)
        {
            _16337 = p.b.y >= 0.0;
        }
        else
        {
            _16337 = false;
        }
        float _16344;
        if (_16337)
        {
            _16344 = 0.0;
        }
        else
        {
            _16344 = precise::min(abs(p.b.y), abs(p.b.y));
        }
        float _16347 = precise::max(abs(p.b.y), abs(p.b.y));
        float _16282 = spvFMul(_16344, _16344);
        float _16348 = interval_down(_16282, intervalFailed);
        float _16283 = spvFMul(_16347, _16347);
        float _16350 = interval_up(_16283, intervalFailed);
        Interval _16287 = Interval{ precise::max(0.0, _16348), _16350 };
        Interval _16352 = iadd(_16286, _16287, intervalFailed);
        Interval _16288 = _16352;
        bool _16357;
        if (p.b.z <= 0.0)
        {
            _16357 = p.b.z >= 0.0;
        }
        else
        {
            _16357 = false;
        }
        float _16364;
        if (_16357)
        {
            _16364 = 0.0;
        }
        else
        {
            _16364 = precise::min(abs(p.b.z), abs(p.b.z));
        }
        float _16367 = precise::max(abs(p.b.z), abs(p.b.z));
        float _16280 = spvFMul(_16364, _16364);
        float _16368 = interval_down(_16280, intervalFailed);
        float _16281 = spvFMul(_16367, _16367);
        float _16370 = interval_up(_16281, intervalFailed);
        Interval _16289 = Interval{ precise::max(0.0, _16368), _16370 };
        Interval _16372 = iadd(_16288, _16289, intervalFailed);
        Interval _16290 = _16372;
        Interval _16373 = isqrt(_16290, intervalFailed);
        Interval _16292 = _16373;
        Interval _16374 = idiv(_16291, _16292, intervalFailed, interval_divide_upper);
        Interval _16274 = Interval{ p.b.x, p.b.x };
        Interval _16275 = _16374;
        Interval _16376 = imul(_16274, _16275, intervalFailed, optical_product_upper);
        Interval _16276 = Interval{ p.b.y, p.b.y };
        Interval _16277 = _16374;
        Interval _16378 = imul(_16276, _16277, intervalFailed, optical_product_upper);
        Interval _16278 = Interval{ p.b.z, p.b.z };
        Interval _16279 = _16374;
        Interval _16380 = imul(_16278, _16279, intervalFailed, optical_product_upper);
        return Interval3{ _16376, _16378, _16380 };
    }
    Interval _16268 = Interval{ p.b.x, p.b.x };
    Interval _16269 = Interval{ as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u) };
    Interval _16412 = iadd(_16268, _16269, intervalFailed);
    Interval _16270 = Interval{ p.b.y, p.b.y };
    Interval _16271 = Interval{ as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u) };
    Interval _16415 = iadd(_16270, _16271, intervalFailed);
    Interval _16272 = Interval{ p.b.z, p.b.z };
    Interval _16273 = Interval{ as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u) };
    Interval _16418 = iadd(_16272, _16273, intervalFailed);
    Interval _16262 = Interval{ p.c.x, p.c.x };
    Interval _16263 = Interval{ as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.x) ^ 2147483648u) };
    Interval _16449 = iadd(_16262, _16263, intervalFailed);
    Interval _16264 = Interval{ p.c.y, p.c.y };
    Interval _16265 = Interval{ as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.y) ^ 2147483648u) };
    Interval _16452 = iadd(_16264, _16265, intervalFailed);
    Interval _16266 = Interval{ p.c.z, p.c.z };
    Interval _16267 = Interval{ as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(p.a.z) ^ 2147483648u) };
    Interval _16455 = iadd(_16266, _16267, intervalFailed);
    Interval _16250 = _16415;
    Interval _16251 = _16455;
    Interval _16456 = imul(_16250, _16251, intervalFailed, optical_product_upper);
    Interval _16252 = _16418;
    Interval _16253 = _16452;
    Interval _16457 = imul(_16252, _16253, intervalFailed, optical_product_upper);
    Interval _16248 = _16456;
    Interval _16249 = Interval{ as_type<float>(as_type<uint>(_16457.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16457.lo) ^ 2147483648u) };
    Interval _16467 = iadd(_16248, _16249, intervalFailed);
    Interval _16254 = _16418;
    Interval _16255 = _16449;
    Interval _16468 = imul(_16254, _16255, intervalFailed, optical_product_upper);
    Interval _16256 = _16412;
    Interval _16257 = _16455;
    Interval _16469 = imul(_16256, _16257, intervalFailed, optical_product_upper);
    Interval _16246 = _16468;
    Interval _16247 = Interval{ as_type<float>(as_type<uint>(_16469.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16469.lo) ^ 2147483648u) };
    Interval _16479 = iadd(_16246, _16247, intervalFailed);
    Interval _16258 = _16412;
    Interval _16259 = _16452;
    Interval _16480 = imul(_16258, _16259, intervalFailed, optical_product_upper);
    Interval _16260 = _16415;
    Interval _16261 = _16449;
    Interval _16481 = imul(_16260, _16261, intervalFailed, optical_product_upper);
    Interval _16244 = _16480;
    Interval _16245 = Interval{ as_type<float>(as_type<uint>(_16481.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_16481.lo) ^ 2147483648u) };
    Interval _16491 = iadd(_16244, _16245, intervalFailed);
    Interval _16242 = Interval{ 1.0, 1.0 };
    bool _16498;
    if (_16467.lo <= 0.0)
    {
        _16498 = _16467.hi >= 0.0;
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
        _16505 = precise::min(abs(_16467.lo), abs(_16467.hi));
    }
    float _16508 = precise::max(abs(_16467.lo), abs(_16467.hi));
    float _16235 = spvFMul(_16505, _16505);
    float _16509 = interval_down(_16235, intervalFailed);
    float _16236 = spvFMul(_16508, _16508);
    float _16511 = interval_up(_16236, intervalFailed);
    Interval _16237 = Interval{ precise::max(0.0, _16509), _16511 };
    bool _16519;
    if (_16479.lo <= 0.0)
    {
        _16519 = _16479.hi >= 0.0;
    }
    else
    {
        _16519 = false;
    }
    float _16526;
    if (_16519)
    {
        _16526 = 0.0;
    }
    else
    {
        _16526 = precise::min(abs(_16479.lo), abs(_16479.hi));
    }
    float _16529 = precise::max(abs(_16479.lo), abs(_16479.hi));
    float _16233 = spvFMul(_16526, _16526);
    float _16530 = interval_down(_16233, intervalFailed);
    float _16234 = spvFMul(_16529, _16529);
    float _16532 = interval_up(_16234, intervalFailed);
    Interval _16238 = Interval{ precise::max(0.0, _16530), _16532 };
    Interval _16534 = iadd(_16237, _16238, intervalFailed);
    Interval _16239 = _16534;
    bool _16541;
    if (_16491.lo <= 0.0)
    {
        _16541 = _16491.hi >= 0.0;
    }
    else
    {
        _16541 = false;
    }
    float _16548;
    if (_16541)
    {
        _16548 = 0.0;
    }
    else
    {
        _16548 = precise::min(abs(_16491.lo), abs(_16491.hi));
    }
    float _16551 = precise::max(abs(_16491.lo), abs(_16491.hi));
    float _16231 = spvFMul(_16548, _16548);
    float _16552 = interval_down(_16231, intervalFailed);
    float _16232 = spvFMul(_16551, _16551);
    float _16554 = interval_up(_16232, intervalFailed);
    Interval _16240 = Interval{ precise::max(0.0, _16552), _16554 };
    Interval _16556 = iadd(_16239, _16240, intervalFailed);
    Interval _16241 = _16556;
    Interval _16557 = isqrt(_16241, intervalFailed);
    Interval _16243 = _16557;
    Interval _16558 = idiv(_16242, _16243, intervalFailed, interval_divide_upper);
    Interval _16225 = _16467;
    Interval _16226 = _16558;
    Interval _16559 = imul(_16225, _16226, intervalFailed, optical_product_upper);
    Interval _16227 = _16479;
    Interval _16228 = _16558;
    Interval _16560 = imul(_16227, _16228, intervalFailed, optical_product_upper);
    Interval _16229 = _16491;
    Interval _16230 = _16558;
    Interval _16561 = imul(_16229, _16230, intervalFailed, optical_product_upper);
    return Interval3{ _16559, _16560, _16561 };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_plane_normal(thread const ReflectionSpecularPlane& plane, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    ReflectionSpecularPlane param_var_p = plane;
    Interval3 _11697 = plane_normal(param_var_p, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _11720;
    if (!intervalFailed)
    {
        bool _11713;
        if (plane.a.w == 2.0)
        {
            _11713 = plane.b.w == 2.0;
        }
        else
        {
            _11713 = false;
        }
        bool _11718;
        if (_11713)
        {
            _11718 = plane.c.w == 2.0;
        }
        else
        {
            _11718 = false;
        }
        _11720 = !_11718;
    }
    else
    {
        _11720 = false;
    }
    if (_11720)
    {
        for (uint _11721 = 0u; _11721 < 3u; _11721++)
        {
            bool _11733;
            if ((isunordered(plane.a[_11721], plane.b[_11721]) || plane.a[_11721] == plane.b[_11721]))
            {
                _11733 = plane.a[_11721] != plane.c[_11721];
            }
            else
            {
                _11733 = true;
            }
            if (_11733)
            {
                continue;
            }
            float _11744;
            float _11745;
            if (_11721 == 0u)
            {
                _11744 = _11697.x.hi;
                _11745 = _11697.x.lo;
            }
            else
            {
                float _11742;
                float _11743;
                if (_11721 == 1u)
                {
                    _11742 = _11697.y.hi;
                    _11743 = _11697.y.lo;
                }
                else
                {
                    _11742 = _11697.z.hi;
                    _11743 = _11697.z.lo;
                }
                _11744 = _11742;
                _11745 = _11743;
            }
            bool _11748;
            if (_11745 <= 0.0)
            {
                _11748 = _11744 >= 0.0;
            }
            else
            {
                _11748 = false;
            }
            if (_11748)
            {
                continue;
            }
            float3 exact = float3(0.0);
            int _11750;
            if (_11745 > 0.0)
            {
                _11750 = 1;
            }
            else
            {
                _11750 = -1;
            }
            exact[_11721] = float(_11750);
            return OpticalJet3{ OpticalJet{ Interval{ exact.x, exact.x }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ exact.y, exact.y }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ exact.z, exact.z }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
        }
    }
    return OpticalJet3{ OpticalJet{ _11697.x, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ _11697.y, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ _11697.z, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_oriented(thread const OpticalJet3& n, thread const OpticalJet3& direction, thread bool& intervalFailed, thread float& optical_product_upper, thread bool& jetBranchKnown)
{
    Interval _11853 = n.x.v;
    Interval _11854 = direction.x.v;
    Interval _11881 = imul(_11853, _11854, intervalFailed, optical_product_upper);
    Interval _11855 = n.x.dx;
    Interval _11856 = direction.x.v;
    Interval _11882 = jet_mul_derivative(_11855, _11856, intervalFailed, optical_product_upper);
    Interval _11857 = _11882;
    Interval _11858 = n.x.v;
    Interval _11859 = direction.x.dx;
    Interval _11883 = jet_mul_derivative(_11858, _11859, intervalFailed, optical_product_upper);
    Interval _11860 = _11883;
    Interval _11884 = jet_add_derivative(_11857, _11860, intervalFailed);
    Interval _11861 = n.x.dy;
    Interval _11862 = direction.x.v;
    Interval _11885 = jet_mul_derivative(_11861, _11862, intervalFailed, optical_product_upper);
    Interval _11863 = _11885;
    Interval _11864 = n.x.v;
    Interval _11865 = direction.x.dy;
    Interval _11886 = jet_mul_derivative(_11864, _11865, intervalFailed, optical_product_upper);
    Interval _11866 = _11886;
    Interval _11887 = jet_add_derivative(_11863, _11866, intervalFailed);
    Interval _11839 = n.y.v;
    Interval _11840 = direction.y.v;
    Interval _11894 = imul(_11839, _11840, intervalFailed, optical_product_upper);
    Interval _11841 = n.y.dx;
    Interval _11842 = direction.y.v;
    Interval _11895 = jet_mul_derivative(_11841, _11842, intervalFailed, optical_product_upper);
    Interval _11843 = _11895;
    Interval _11844 = n.y.v;
    Interval _11845 = direction.y.dx;
    Interval _11896 = jet_mul_derivative(_11844, _11845, intervalFailed, optical_product_upper);
    Interval _11846 = _11896;
    Interval _11897 = jet_add_derivative(_11843, _11846, intervalFailed);
    Interval _11847 = n.y.dy;
    Interval _11848 = direction.y.v;
    Interval _11898 = jet_mul_derivative(_11847, _11848, intervalFailed, optical_product_upper);
    Interval _11849 = _11898;
    Interval _11850 = n.y.v;
    Interval _11851 = direction.y.dy;
    Interval _11899 = jet_mul_derivative(_11850, _11851, intervalFailed, optical_product_upper);
    Interval _11852 = _11899;
    Interval _11900 = jet_add_derivative(_11849, _11852, intervalFailed);
    Interval _11833 = _11881;
    Interval _11834 = _11894;
    Interval _11901 = iadd(_11833, _11834, intervalFailed);
    Interval _11835 = _11884;
    Interval _11836 = _11897;
    Interval _11902 = jet_add_derivative(_11835, _11836, intervalFailed);
    Interval _11837 = _11887;
    Interval _11838 = _11900;
    Interval _11903 = jet_add_derivative(_11837, _11838, intervalFailed);
    Interval _11819 = n.z.v;
    Interval _11820 = direction.z.v;
    Interval _11910 = imul(_11819, _11820, intervalFailed, optical_product_upper);
    Interval _11821 = n.z.dx;
    Interval _11822 = direction.z.v;
    Interval _11911 = jet_mul_derivative(_11821, _11822, intervalFailed, optical_product_upper);
    Interval _11823 = _11911;
    Interval _11824 = n.z.v;
    Interval _11825 = direction.z.dx;
    Interval _11912 = jet_mul_derivative(_11824, _11825, intervalFailed, optical_product_upper);
    Interval _11826 = _11912;
    Interval _11913 = jet_add_derivative(_11823, _11826, intervalFailed);
    Interval _11827 = n.z.dy;
    Interval _11828 = direction.z.v;
    Interval _11914 = jet_mul_derivative(_11827, _11828, intervalFailed, optical_product_upper);
    Interval _11829 = _11914;
    Interval _11830 = n.z.v;
    Interval _11831 = direction.z.dy;
    Interval _11915 = jet_mul_derivative(_11830, _11831, intervalFailed, optical_product_upper);
    Interval _11832 = _11915;
    Interval _11916 = jet_add_derivative(_11829, _11832, intervalFailed);
    Interval _11813 = _11901;
    Interval _11814 = _11910;
    Interval _11917 = iadd(_11813, _11814, intervalFailed);
    Interval _11815 = _11902;
    Interval _11816 = _11913;
    __attribute__((unused)) Interval _11918 = jet_add_derivative(_11815, _11816, intervalFailed);
    Interval _11817 = _11903;
    Interval _11818 = _11916;
    __attribute__((unused)) Interval _11919 = jet_add_derivative(_11817, _11818, intervalFailed);
    if (_11917.lo > 0.0)
    {
        Interval _11799 = n.x.v;
        Interval _11800 = Interval{ -1.0, -1.0 };
        Interval _11930 = imul(_11799, _11800, intervalFailed, optical_product_upper);
        Interval _11801 = n.x.dx;
        Interval _11802 = Interval{ -1.0, -1.0 };
        Interval _11931 = jet_mul_derivative(_11801, _11802, intervalFailed, optical_product_upper);
        Interval _11803 = _11931;
        Interval _11804 = n.x.v;
        Interval _11805 = Interval{ 0.0, 0.0 };
        Interval _11932 = jet_mul_derivative(_11804, _11805, intervalFailed, optical_product_upper);
        Interval _11806 = _11932;
        Interval _11933 = jet_add_derivative(_11803, _11806, intervalFailed);
        Interval _11807 = n.x.dy;
        Interval _11808 = Interval{ -1.0, -1.0 };
        Interval _11934 = jet_mul_derivative(_11807, _11808, intervalFailed, optical_product_upper);
        Interval _11809 = _11934;
        Interval _11810 = n.x.v;
        Interval _11811 = Interval{ 0.0, 0.0 };
        Interval _11935 = jet_mul_derivative(_11810, _11811, intervalFailed, optical_product_upper);
        Interval _11812 = _11935;
        Interval _11936 = jet_add_derivative(_11809, _11812, intervalFailed);
        Interval _11785 = n.y.v;
        Interval _11786 = Interval{ -1.0, -1.0 };
        Interval _11940 = imul(_11785, _11786, intervalFailed, optical_product_upper);
        Interval _11787 = n.y.dx;
        Interval _11788 = Interval{ -1.0, -1.0 };
        Interval _11941 = jet_mul_derivative(_11787, _11788, intervalFailed, optical_product_upper);
        Interval _11789 = _11941;
        Interval _11790 = n.y.v;
        Interval _11791 = Interval{ 0.0, 0.0 };
        Interval _11942 = jet_mul_derivative(_11790, _11791, intervalFailed, optical_product_upper);
        Interval _11792 = _11942;
        Interval _11943 = jet_add_derivative(_11789, _11792, intervalFailed);
        Interval _11793 = n.y.dy;
        Interval _11794 = Interval{ -1.0, -1.0 };
        Interval _11944 = jet_mul_derivative(_11793, _11794, intervalFailed, optical_product_upper);
        Interval _11795 = _11944;
        Interval _11796 = n.y.v;
        Interval _11797 = Interval{ 0.0, 0.0 };
        Interval _11945 = jet_mul_derivative(_11796, _11797, intervalFailed, optical_product_upper);
        Interval _11798 = _11945;
        Interval _11946 = jet_add_derivative(_11795, _11798, intervalFailed);
        Interval _11771 = n.z.v;
        Interval _11772 = Interval{ -1.0, -1.0 };
        Interval _11950 = imul(_11771, _11772, intervalFailed, optical_product_upper);
        Interval _11773 = n.z.dx;
        Interval _11774 = Interval{ -1.0, -1.0 };
        Interval _11951 = jet_mul_derivative(_11773, _11774, intervalFailed, optical_product_upper);
        Interval _11775 = _11951;
        Interval _11776 = n.z.v;
        Interval _11777 = Interval{ 0.0, 0.0 };
        Interval _11952 = jet_mul_derivative(_11776, _11777, intervalFailed, optical_product_upper);
        Interval _11778 = _11952;
        Interval _11953 = jet_add_derivative(_11775, _11778, intervalFailed);
        Interval _11779 = n.z.dy;
        Interval _11780 = Interval{ -1.0, -1.0 };
        Interval _11954 = jet_mul_derivative(_11779, _11780, intervalFailed, optical_product_upper);
        Interval _11781 = _11954;
        Interval _11782 = n.z.v;
        Interval _11783 = Interval{ 0.0, 0.0 };
        Interval _11955 = jet_mul_derivative(_11782, _11783, intervalFailed, optical_product_upper);
        Interval _11784 = _11955;
        Interval _11956 = jet_add_derivative(_11781, _11784, intervalFailed);
        return OpticalJet3{ OpticalJet{ _11930, _11933, _11936 }, OpticalJet{ _11940, _11943, _11946 }, OpticalJet{ _11950, _11953, _11956 } };
    }
    if (_11917.hi < 0.0)
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
    bool _12041;
    if (a.v.lo <= 0.0)
    {
        _12041 = a.v.hi >= 0.0;
    }
    else
    {
        _12041 = false;
    }
    float _12048;
    if (_12041)
    {
        _12048 = 0.0;
    }
    else
    {
        _12048 = precise::min(abs(a.v.lo), abs(a.v.hi));
    }
    return OpticalJet{ Interval{ _12048, precise::max(abs(a.v.lo), abs(a.v.hi)) }, Interval{ precise::min(a.dx.lo, as_type<float>(as_type<uint>(a.dx.hi) ^ 2147483648u)), precise::max(a.dx.hi, as_type<float>(as_type<uint>(a.dx.lo) ^ 2147483648u)) }, Interval{ precise::min(a.dy.lo, as_type<float>(as_type<uint>(a.dy.hi) ^ 2147483648u)), precise::max(a.dy.hi, as_type<float>(as_type<uint>(a.dy.lo) ^ 2147483648u)) } };
}

static inline __attribute__((always_inline))
Interval iratio(thread const float& n, thread const float& d, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    if (d == 10.0)
    {
        Interval param_var_a = Interval{ n, n };
        Interval param_var_b = Interval{ as_type<float>(1036831948u), as_type<float>(1036831950u) };
        Interval _16570 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        return _16570;
    }
    if (d == 100.0)
    {
        Interval param_var_a_1 = Interval{ n, n };
        Interval param_var_b_1 = Interval{ as_type<float>(1008981769u), as_type<float>(1008981771u) };
        Interval _16578 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        return _16578;
    }
    if (d == 1000.0)
    {
        Interval param_var_a_2 = Interval{ n, n };
        Interval param_var_b_2 = Interval{ as_type<float>(981668462u), as_type<float>(981668464u) };
        Interval _16586 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        return _16586;
    }
    if (d == 10000.0)
    {
        Interval param_var_a_3 = Interval{ n, n };
        Interval param_var_b_3 = Interval{ as_type<float>(953267990u), as_type<float>(953267992u) };
        Interval _16594 = imul(param_var_a_3, param_var_b_3, intervalFailed, optical_product_upper);
        return _16594;
    }
    if (d == 100000.0)
    {
        Interval param_var_a_4 = Interval{ n, n };
        Interval param_var_b_4 = Interval{ as_type<float>(925353387u), as_type<float>(925353389u) };
        Interval _16602 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
        return _16602;
    }
    if (d == 128.0)
    {
        Interval param_var_a_5 = Interval{ n, n };
        Interval param_var_b_5 = Interval{ as_type<float>(1006632960u), as_type<float>(1006632960u) };
        Interval _16610 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
        return _16610;
    }
    if (d == 65535.0)
    {
        Interval param_var_a_6 = Interval{ n, n };
        Interval param_var_b_6 = Interval{ as_type<float>(931135615u), as_type<float>(931135617u) };
        Interval _16618 = imul(param_var_a_6, param_var_b_6, intervalFailed, optical_product_upper);
        return _16618;
    }
    Interval param_var_a_7 = Interval{ n, n };
    Interval param_var_b_7 = Interval{ d, d };
    Interval _16623 = idiv(param_var_a_7, param_var_b_7, intervalFailed, interval_divide_upper);
    return _16623;
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
    bool _21633;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _21633 = isnan(a.hi) || isinf(a.hi);
    }
    else
    {
        _21633 = true;
    }
    bool _21643;
    if (!_21633)
    {
        _21643 = precise::max(abs(a.lo), abs(a.hi)) > 1048576.0;
    }
    else
    {
        _21643 = true;
    }
    if (_21643)
    {
        intervalFailed = true;
        return Interval{ -1.0, 1.0 };
    }
    float _1737 = spvFMul(floor(spvFAdd(spvFMul(spvFAdd(a.lo, a.hi), 0.5) / 6.283185482025146484375, 0.5)), 2.0);
    Interval param_var_a = Interval{ _1737, _1737 };
    float _21620 = 3.1415927410125732421875;
    float _21651 = interval_down(_21620, intervalFailed);
    float _21621 = 3.1415927410125732421875;
    float _21652 = interval_up(_21621, intervalFailed);
    Interval param_var_b = Interval{ _21651, _21652 };
    Interval _21654 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    Interval _21618 = a;
    Interval _21619 = Interval{ as_type<float>(as_type<uint>(_21654.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21654.lo) ^ 2147483648u) };
    Interval _21664 = iadd(_21618, _21619, intervalFailed);
    float _21616 = 3.1415927410125732421875;
    float _21667 = interval_down(_21616, intervalFailed);
    float _21617 = 3.1415927410125732421875;
    float _21668 = interval_up(_21617, intervalFailed);
    Interval param_var_a_1 = Interval{ _21667, _21668 };
    Interval param_var_b_1 = Interval{ 0.5, 0.5 };
    Interval _21670 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
    float _21712;
    float _21713;
    if (_21664.lo > _21670.hi)
    {
        float _21614 = 3.1415927410125732421875;
        float _21673 = interval_down(_21614, intervalFailed);
        float _21615 = 3.1415927410125732421875;
        float _21674 = interval_up(_21615, intervalFailed);
        Interval _21612 = Interval{ _21673, _21674 };
        Interval _21613 = Interval{ as_type<float>(as_type<uint>(_21664.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21664.lo) ^ 2147483648u) };
        Interval _21685 = iadd(_21612, _21613, intervalFailed);
        _21712 = _21685.hi;
        _21713 = _21685.lo;
    }
    else
    {
        float _21710;
        float _21711;
        if (_21664.hi < (-_21670.hi))
        {
            float _21610 = 3.1415927410125732421875;
            float _21689 = interval_down(_21610, intervalFailed);
            float _21611 = 3.1415927410125732421875;
            float _21690 = interval_up(_21611, intervalFailed);
            Interval _21608 = Interval{ as_type<float>(as_type<uint>(_21690) ^ 2147483648u), as_type<float>(as_type<uint>(_21689) ^ 2147483648u) };
            Interval _21609 = Interval{ as_type<float>(as_type<uint>(_21664.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21664.lo) ^ 2147483648u) };
            Interval _21707 = iadd(_21608, _21609, intervalFailed);
            _21710 = _21707.hi;
            _21711 = _21707.lo;
        }
        else
        {
            _21710 = _21664.hi;
            _21711 = _21664.lo;
        }
        _21712 = _21710;
        _21713 = _21711;
    }
    float _1739 = -_21670.hi;
    bool _21716;
    if ((isunordered(_21713, _1739) || _21713 >= _1739))
    {
        _21716 = _21712 > _21670.hi;
    }
    else
    {
        _21716 = true;
    }
    if (_21716)
    {
        return Interval{ -1.0, 1.0 };
    }
    bool _21721;
    if (_21713 <= 0.0)
    {
        _21721 = _21712 >= 0.0;
    }
    else
    {
        _21721 = false;
    }
    float _21728;
    if (_21721)
    {
        _21728 = 0.0;
    }
    else
    {
        _21728 = precise::min(abs(_21713), abs(_21712));
    }
    float _21731 = precise::max(abs(_21713), abs(_21712));
    float _21606 = spvFMul(_21728, _21728);
    float _21732 = interval_down(_21606, intervalFailed);
    float _21733 = precise::max(0.0, _21732);
    float _21607 = spvFMul(_21731, _21731);
    float _21734 = interval_up(_21607, intervalFailed);
    float _21604 = _2286[8];
    float _21737 = interval_down(_21604, intervalFailed);
    float _21605 = _2286[8];
    float _21738 = interval_up(_21605, intervalFailed);
    float _21739;
    float _21741;
    _21739 = _21737;
    _21741 = _21738;
    float _21740;
    float _21742;
    for (int _21743 = 7; _21743 >= 0; _21739 = _21740, _21741 = _21742, _21743--)
    {
        Interval param_var_a_2 = Interval{ _21739, _21741 };
        Interval param_var_b_2 = Interval{ _21733, _21734 };
        Interval _21747 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
        Interval param_var_a_3 = _21747;
        float _21602 = _2286[_21743];
        float _21750 = interval_down(_21602, intervalFailed);
        float _21603 = _2286[_21743];
        float _21751 = interval_up(_21603, intervalFailed);
        Interval param_var_b_3 = Interval{ _21750, _21751 };
        Interval _21753 = iadd(param_var_a_3, param_var_b_3, intervalFailed);
        _21740 = _21753.lo;
        _21742 = _21753.hi;
    }
    Interval param_var_a_4 = Interval{ _21713, _21712 };
    Interval param_var_b_4 = Interval{ _21739, _21741 };
    Interval _21756 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
    Interval param_var_a_5 = _21756;
    Interval param_var_b_5 = Interval{ -3.9999999840167888010000751819462e-12, 3.9999999840167888010000751819462e-12 };
    Interval _21757 = iadd(param_var_a_5, param_var_b_5, intervalFailed);
    return Interval{ precise::min(precise::max(_21757.lo, -1.0), 1.0), precise::min(precise::max(_21757.hi, -1.0), 1.0) };
}

static inline __attribute__((always_inline))
float sine_bounds(thread const float& lo, thread const float& hi, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_sine_upper)
{
    Interval param_var_a = Interval{ lo, hi };
    Interval _21599 = isin_body(param_var_a, intervalFailed, optical_product_upper);
    interval_sine_upper = _21599.hi;
    return _21599.lo;
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
    float _18424 = 6.0;
    float _18425 = 1000.0;
    Interval _18431 = iratio(_18424, _18425, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18436;
    if (!intervalFailed)
    {
        _18436 = intervalFailed;
    }
    else
    {
        _18436 = false;
    }
    bool _18441;
    if (_18436)
    {
        _18441 = jetFailureSite == 0u;
    }
    else
    {
        _18441 = false;
    }
    if (_18441)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(6.0, 6.0, 1000.0, 1000.0);
    }
    Interval _18410 = x.v;
    Interval _18411 = _18431;
    Interval _18444 = imul(_18410, _18411, intervalFailed, optical_product_upper);
    Interval _18412 = x.dx;
    Interval _18413 = _18431;
    Interval _18445 = jet_mul_derivative(_18412, _18413, intervalFailed, optical_product_upper);
    Interval _18414 = _18445;
    Interval _18415 = x.v;
    Interval _18416 = Interval{ 0.0, 0.0 };
    Interval _18446 = jet_mul_derivative(_18415, _18416, intervalFailed, optical_product_upper);
    Interval _18417 = _18446;
    Interval _18447 = jet_add_derivative(_18414, _18417, intervalFailed);
    Interval _18418 = x.dy;
    Interval _18419 = _18431;
    Interval _18448 = jet_mul_derivative(_18418, _18419, intervalFailed, optical_product_upper);
    Interval _18420 = _18448;
    Interval _18421 = x.v;
    Interval _18422 = Interval{ 0.0, 0.0 };
    Interval _18449 = jet_mul_derivative(_18421, _18422, intervalFailed, optical_product_upper);
    Interval _18423 = _18449;
    Interval _18450 = jet_add_derivative(_18420, _18423, intervalFailed);
    float _18408 = 8.0;
    float _18409 = 1000.0;
    Interval _18456 = iratio(_18408, _18409, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18461;
    if (!intervalFailed)
    {
        _18461 = intervalFailed;
    }
    else
    {
        _18461 = false;
    }
    bool _18466;
    if (_18461)
    {
        _18466 = jetFailureSite == 0u;
    }
    else
    {
        _18466 = false;
    }
    if (_18466)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(8.0, 8.0, 1000.0, 1000.0);
    }
    Interval _18394 = z.v;
    Interval _18395 = _18456;
    Interval _18469 = imul(_18394, _18395, intervalFailed, optical_product_upper);
    Interval _18396 = z.dx;
    Interval _18397 = _18456;
    Interval _18470 = jet_mul_derivative(_18396, _18397, intervalFailed, optical_product_upper);
    Interval _18398 = _18470;
    Interval _18399 = z.v;
    Interval _18400 = Interval{ 0.0, 0.0 };
    Interval _18471 = jet_mul_derivative(_18399, _18400, intervalFailed, optical_product_upper);
    Interval _18401 = _18471;
    Interval _18472 = jet_add_derivative(_18398, _18401, intervalFailed);
    Interval _18402 = z.dy;
    Interval _18403 = _18456;
    Interval _18473 = jet_mul_derivative(_18402, _18403, intervalFailed, optical_product_upper);
    Interval _18404 = _18473;
    Interval _18405 = z.v;
    Interval _18406 = Interval{ 0.0, 0.0 };
    Interval _18474 = jet_mul_derivative(_18405, _18406, intervalFailed, optical_product_upper);
    Interval _18407 = _18474;
    Interval _18475 = jet_add_derivative(_18404, _18407, intervalFailed);
    Interval _18388 = _18444;
    Interval _18389 = Interval{ as_type<float>(as_type<uint>(_18469.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18469.lo) ^ 2147483648u) };
    Interval _18501 = iadd(_18388, _18389, intervalFailed);
    Interval _18390 = _18447;
    Interval _18391 = Interval{ as_type<float>(as_type<uint>(_18472.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18472.lo) ^ 2147483648u) };
    Interval _18503 = jet_add_derivative(_18390, _18391, intervalFailed);
    Interval _18392 = _18450;
    Interval _18393 = Interval{ as_type<float>(as_type<uint>(_18475.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18475.lo) ^ 2147483648u) };
    Interval _18505 = jet_add_derivative(_18392, _18393, intervalFailed);
    float _18386 = 11.0;
    float _18387 = 100.0;
    Interval _18511 = iratio(_18386, _18387, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18516;
    if (!intervalFailed)
    {
        _18516 = intervalFailed;
    }
    else
    {
        _18516 = false;
    }
    bool _18521;
    if (_18516)
    {
        _18521 = jetFailureSite == 0u;
    }
    else
    {
        _18521 = false;
    }
    if (_18521)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 100.0, 100.0);
    }
    Interval _18372 = t.v;
    Interval _18373 = _18511;
    Interval _18524 = imul(_18372, _18373, intervalFailed, optical_product_upper);
    Interval _18374 = t.dx;
    Interval _18375 = _18511;
    Interval _18525 = jet_mul_derivative(_18374, _18375, intervalFailed, optical_product_upper);
    Interval _18376 = _18525;
    Interval _18377 = t.v;
    Interval _18378 = Interval{ 0.0, 0.0 };
    Interval _18526 = jet_mul_derivative(_18377, _18378, intervalFailed, optical_product_upper);
    Interval _18379 = _18526;
    Interval _18527 = jet_add_derivative(_18376, _18379, intervalFailed);
    Interval _18380 = t.dy;
    Interval _18381 = _18511;
    Interval _18528 = jet_mul_derivative(_18380, _18381, intervalFailed, optical_product_upper);
    Interval _18382 = _18528;
    Interval _18383 = t.v;
    Interval _18384 = Interval{ 0.0, 0.0 };
    Interval _18529 = jet_mul_derivative(_18383, _18384, intervalFailed, optical_product_upper);
    Interval _18385 = _18529;
    Interval _18530 = jet_add_derivative(_18382, _18385, intervalFailed);
    Interval _18366 = _18501;
    Interval _18367 = _18524;
    Interval _18531 = iadd(_18366, _18367, intervalFailed);
    Interval _18368 = _18503;
    Interval _18369 = _18527;
    Interval _18532 = jet_add_derivative(_18368, _18369, intervalFailed);
    Interval _18370 = _18505;
    Interval _18371 = _18530;
    Interval _18533 = jet_add_derivative(_18370, _18371, intervalFailed);
    float _18364 = 10.0;
    float _18365 = 1000.0;
    Interval _18536 = iratio(_18364, _18365, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18541;
    if (!intervalFailed)
    {
        _18541 = intervalFailed;
    }
    else
    {
        _18541 = false;
    }
    bool _18546;
    if (_18541)
    {
        _18546 = jetFailureSite == 0u;
    }
    else
    {
        _18546 = false;
    }
    if (_18546)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(10.0, 10.0, 1000.0, 1000.0);
    }
    Interval _18350 = footprint.v;
    Interval _18351 = _18536;
    Interval _18552 = imul(_18350, _18351, intervalFailed, optical_product_upper);
    Interval _18352 = footprint.dx;
    Interval _18353 = _18536;
    Interval _18553 = jet_mul_derivative(_18352, _18353, intervalFailed, optical_product_upper);
    Interval _18354 = _18553;
    Interval _18355 = footprint.v;
    Interval _18356 = Interval{ 0.0, 0.0 };
    Interval _18554 = jet_mul_derivative(_18355, _18356, intervalFailed, optical_product_upper);
    Interval _18357 = _18554;
    Interval _18555 = jet_add_derivative(_18354, _18357, intervalFailed);
    Interval _18358 = footprint.dy;
    Interval _18359 = _18536;
    Interval _18556 = jet_mul_derivative(_18358, _18359, intervalFailed, optical_product_upper);
    Interval _18360 = _18556;
    Interval _18361 = footprint.v;
    Interval _18362 = Interval{ 0.0, 0.0 };
    Interval _18557 = jet_mul_derivative(_18361, _18362, intervalFailed, optical_product_upper);
    Interval _18363 = _18557;
    Interval _18558 = jet_add_derivative(_18360, _18363, intervalFailed);
    bool _18565;
    if (_18552.lo <= 0.0)
    {
        _18565 = _18552.hi >= 0.0;
    }
    else
    {
        _18565 = false;
    }
    float _18572;
    if (_18565)
    {
        _18572 = 0.0;
    }
    else
    {
        _18572 = precise::min(abs(_18552.lo), abs(_18552.hi));
    }
    float _18575 = precise::max(abs(_18552.lo), abs(_18552.hi));
    float _18340 = spvFMul(_18572, _18572);
    float _18576 = interval_down(_18340, intervalFailed);
    float _18577 = precise::max(0.0, _18576);
    float _18341 = spvFMul(_18575, _18575);
    float _18578 = interval_up(_18341, intervalFailed);
    Interval _18342 = Interval{ 2.0, 2.0 };
    Interval _18343 = _18552;
    Interval _18579 = imul(_18342, _18343, intervalFailed, optical_product_upper);
    Interval _18344 = _18579;
    Interval _18345 = _18555;
    Interval _18580 = jet_mul_derivative(_18344, _18345, intervalFailed, optical_product_upper);
    Interval _18346 = Interval{ 2.0, 2.0 };
    Interval _18347 = _18552;
    Interval _18581 = imul(_18346, _18347, intervalFailed, optical_product_upper);
    Interval _18348 = _18581;
    Interval _18349 = _18558;
    Interval _18582 = jet_mul_derivative(_18348, _18349, intervalFailed, optical_product_upper);
    bool _18587;
    if (_18577 <= 0.0)
    {
        _18587 = _18578 >= 0.0;
    }
    else
    {
        _18587 = false;
    }
    float _18594;
    if (_18587)
    {
        _18594 = 0.0;
    }
    else
    {
        _18594 = precise::min(abs(_18577), abs(_18578));
    }
    float _18597 = precise::max(abs(_18577), abs(_18578));
    float _18330 = spvFMul(_18594, _18594);
    float _18598 = interval_down(_18330, intervalFailed);
    float _18331 = spvFMul(_18597, _18597);
    float _18600 = interval_up(_18331, intervalFailed);
    Interval _18332 = Interval{ 2.0, 2.0 };
    Interval _18333 = Interval{ _18577, _18578 };
    Interval _18602 = imul(_18332, _18333, intervalFailed, optical_product_upper);
    Interval _18334 = _18602;
    Interval _18335 = _18580;
    Interval _18603 = jet_mul_derivative(_18334, _18335, intervalFailed, optical_product_upper);
    Interval _18336 = Interval{ 2.0, 2.0 };
    Interval _18337 = Interval{ _18577, _18578 };
    Interval _18605 = imul(_18336, _18337, intervalFailed, optical_product_upper);
    Interval _18338 = _18605;
    Interval _18339 = _18582;
    Interval _18606 = jet_mul_derivative(_18338, _18339, intervalFailed, optical_product_upper);
    Interval _18324 = Interval{ 1.0, 1.0 };
    Interval _18325 = Interval{ precise::max(0.0, _18598), _18600 };
    Interval _18608 = iadd(_18324, _18325, intervalFailed);
    Interval _18326 = Interval{ 0.0, 0.0 };
    Interval _18327 = _18603;
    Interval _18609 = jet_add_derivative(_18326, _18327, intervalFailed);
    Interval _18328 = Interval{ 0.0, 0.0 };
    Interval _18329 = _18606;
    Interval _18610 = jet_add_derivative(_18328, _18329, intervalFailed);
    Interval _18310 = Interval{ 1.0, 1.0 };
    Interval _18311 = _18608;
    Interval _18612 = idiv(_18310, _18311, intervalFailed, interval_divide_upper);
    bool _18619;
    if (!intervalFailed)
    {
        _18619 = intervalFailed;
    }
    else
    {
        _18619 = false;
    }
    bool _18624;
    if (_18619)
    {
        _18624 = jetFailureSite == 0u;
    }
    else
    {
        _18624 = false;
    }
    if (_18624)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _18608.lo, _18608.hi);
    }
    Interval _18312 = Interval{ 0.0, 0.0 };
    Interval _18313 = _18612;
    Interval _18314 = _18609;
    Interval _18628 = jet_mul_derivative(_18313, _18314, intervalFailed, optical_product_upper);
    Interval _18315 = Interval{ as_type<float>(as_type<uint>(_18628.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18628.lo) ^ 2147483648u) };
    Interval _18638 = jet_add_derivative(_18312, _18315, intervalFailed);
    Interval _18316 = _18638;
    Interval _18317 = _18608;
    Interval _18639 = jet_div_derivative(_18316, _18317, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18318 = Interval{ 0.0, 0.0 };
    Interval _18319 = _18612;
    Interval _18320 = _18610;
    Interval _18640 = jet_mul_derivative(_18319, _18320, intervalFailed, optical_product_upper);
    Interval _18321 = Interval{ as_type<float>(as_type<uint>(_18640.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18640.lo) ^ 2147483648u) };
    Interval _18650 = jet_add_derivative(_18318, _18321, intervalFailed);
    Interval _18322 = _18650;
    Interval _18323 = _18608;
    Interval _18651 = jet_div_derivative(_18322, _18323, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18308 = 18.0;
    float _18309 = 1000.0;
    Interval _18657 = iratio(_18308, _18309, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18662;
    if (!intervalFailed)
    {
        _18662 = intervalFailed;
    }
    else
    {
        _18662 = false;
    }
    bool _18667;
    if (_18662)
    {
        _18667 = jetFailureSite == 0u;
    }
    else
    {
        _18667 = false;
    }
    if (_18667)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
    }
    Interval _18294 = x.v;
    Interval _18295 = _18657;
    Interval _18670 = imul(_18294, _18295, intervalFailed, optical_product_upper);
    Interval _18296 = x.dx;
    Interval _18297 = _18657;
    Interval _18671 = jet_mul_derivative(_18296, _18297, intervalFailed, optical_product_upper);
    Interval _18298 = _18671;
    Interval _18299 = x.v;
    Interval _18300 = Interval{ 0.0, 0.0 };
    Interval _18672 = jet_mul_derivative(_18299, _18300, intervalFailed, optical_product_upper);
    Interval _18301 = _18672;
    Interval _18673 = jet_add_derivative(_18298, _18301, intervalFailed);
    Interval _18302 = x.dy;
    Interval _18303 = _18657;
    Interval _18674 = jet_mul_derivative(_18302, _18303, intervalFailed, optical_product_upper);
    Interval _18304 = _18674;
    Interval _18305 = x.v;
    Interval _18306 = Interval{ 0.0, 0.0 };
    Interval _18675 = jet_mul_derivative(_18305, _18306, intervalFailed, optical_product_upper);
    Interval _18307 = _18675;
    Interval _18676 = jet_add_derivative(_18304, _18307, intervalFailed);
    float _18292 = 11.0;
    float _18293 = 1000.0;
    Interval _18682 = iratio(_18292, _18293, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18687;
    if (!intervalFailed)
    {
        _18687 = intervalFailed;
    }
    else
    {
        _18687 = false;
    }
    bool _18692;
    if (_18687)
    {
        _18692 = jetFailureSite == 0u;
    }
    else
    {
        _18692 = false;
    }
    if (_18692)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
    }
    Interval _18278 = z.v;
    Interval _18279 = _18682;
    Interval _18695 = imul(_18278, _18279, intervalFailed, optical_product_upper);
    Interval _18280 = z.dx;
    Interval _18281 = _18682;
    Interval _18696 = jet_mul_derivative(_18280, _18281, intervalFailed, optical_product_upper);
    Interval _18282 = _18696;
    Interval _18283 = z.v;
    Interval _18284 = Interval{ 0.0, 0.0 };
    Interval _18697 = jet_mul_derivative(_18283, _18284, intervalFailed, optical_product_upper);
    Interval _18285 = _18697;
    Interval _18698 = jet_add_derivative(_18282, _18285, intervalFailed);
    Interval _18286 = z.dy;
    Interval _18287 = _18682;
    Interval _18699 = jet_mul_derivative(_18286, _18287, intervalFailed, optical_product_upper);
    Interval _18288 = _18699;
    Interval _18289 = z.v;
    Interval _18290 = Interval{ 0.0, 0.0 };
    Interval _18700 = jet_mul_derivative(_18289, _18290, intervalFailed, optical_product_upper);
    Interval _18291 = _18700;
    Interval _18701 = jet_add_derivative(_18288, _18291, intervalFailed);
    Interval _18272 = _18670;
    Interval _18273 = _18695;
    Interval _18702 = iadd(_18272, _18273, intervalFailed);
    Interval _18274 = _18673;
    Interval _18275 = _18698;
    Interval _18703 = jet_add_derivative(_18274, _18275, intervalFailed);
    Interval _18276 = _18676;
    Interval _18277 = _18701;
    Interval _18704 = jet_add_derivative(_18276, _18277, intervalFailed);
    float _18270 = 45.0;
    float _18271 = 100.0;
    Interval _18710 = iratio(_18270, _18271, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18715;
    if (!intervalFailed)
    {
        _18715 = intervalFailed;
    }
    else
    {
        _18715 = false;
    }
    bool _18720;
    if (_18715)
    {
        _18720 = jetFailureSite == 0u;
    }
    else
    {
        _18720 = false;
    }
    if (_18720)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(45.0, 45.0, 100.0, 100.0);
    }
    Interval _18256 = t.v;
    Interval _18257 = _18710;
    Interval _18723 = imul(_18256, _18257, intervalFailed, optical_product_upper);
    Interval _18258 = t.dx;
    Interval _18259 = _18710;
    Interval _18724 = jet_mul_derivative(_18258, _18259, intervalFailed, optical_product_upper);
    Interval _18260 = _18724;
    Interval _18261 = t.v;
    Interval _18262 = Interval{ 0.0, 0.0 };
    Interval _18725 = jet_mul_derivative(_18261, _18262, intervalFailed, optical_product_upper);
    Interval _18263 = _18725;
    Interval _18726 = jet_add_derivative(_18260, _18263, intervalFailed);
    Interval _18264 = t.dy;
    Interval _18265 = _18710;
    Interval _18727 = jet_mul_derivative(_18264, _18265, intervalFailed, optical_product_upper);
    Interval _18266 = _18727;
    Interval _18267 = t.v;
    Interval _18268 = Interval{ 0.0, 0.0 };
    Interval _18728 = jet_mul_derivative(_18267, _18268, intervalFailed, optical_product_upper);
    Interval _18269 = _18728;
    Interval _18729 = jet_add_derivative(_18266, _18269, intervalFailed);
    Interval _18250 = _18702;
    Interval _18251 = Interval{ as_type<float>(as_type<uint>(_18723.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18723.lo) ^ 2147483648u) };
    Interval _18755 = iadd(_18250, _18251, intervalFailed);
    Interval _18252 = _18703;
    Interval _18253 = Interval{ as_type<float>(as_type<uint>(_18726.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18726.lo) ^ 2147483648u) };
    Interval _18757 = jet_add_derivative(_18252, _18253, intervalFailed);
    Interval _18254 = _18704;
    Interval _18255 = Interval{ as_type<float>(as_type<uint>(_18729.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18729.lo) ^ 2147483648u) };
    Interval _18759 = jet_add_derivative(_18254, _18255, intervalFailed);
    float _18248 = 65.0;
    float _18249 = 100.0;
    Interval _18761 = iratio(_18248, _18249, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18766;
    if (!intervalFailed)
    {
        _18766 = intervalFailed;
    }
    else
    {
        _18766 = false;
    }
    bool _18771;
    if (_18766)
    {
        _18771 = jetFailureSite == 0u;
    }
    else
    {
        _18771 = false;
    }
    if (_18771)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(65.0, 65.0, 100.0, 100.0);
    }
    Interval _18240 = _18531;
    float _18238 = 3.1415927410125732421875;
    float _18774 = interval_down(_18238, intervalFailed);
    float _18239 = 3.1415927410125732421875;
    float _18775 = interval_up(_18239, intervalFailed);
    Interval _18241 = Interval{ _18774, _18775 };
    Interval _18242 = Interval{ 0.5, 0.5 };
    Interval _18777 = imul(_18241, _18242, intervalFailed, optical_product_upper);
    Interval _18243 = _18777;
    Interval _18778 = iadd(_18240, _18243, intervalFailed);
    float _18236 = _18778.lo;
    float _18237 = _18778.hi;
    float _18781 = sine_bounds(_18236, _18237, intervalFailed, optical_product_upper, interval_sine_upper);
    float _18234 = _18531.lo;
    float _18235 = _18531.hi;
    float _18785 = sine_bounds(_18234, _18235, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _18244 = Interval{ _18781, interval_sine_upper };
    Interval _18245 = _18532;
    Interval _18788 = jet_mul_derivative(_18244, _18245, intervalFailed, optical_product_upper);
    Interval _18246 = Interval{ _18781, interval_sine_upper };
    Interval _18247 = _18533;
    Interval _18790 = jet_mul_derivative(_18246, _18247, intervalFailed, optical_product_upper);
    Interval _18220 = _18761;
    Interval _18221 = Interval{ _18785, interval_sine_upper };
    Interval _18792 = imul(_18220, _18221, intervalFailed, optical_product_upper);
    Interval _18222 = Interval{ 0.0, 0.0 };
    Interval _18223 = Interval{ _18785, interval_sine_upper };
    Interval _18794 = jet_mul_derivative(_18222, _18223, intervalFailed, optical_product_upper);
    Interval _18224 = _18794;
    Interval _18225 = _18761;
    Interval _18226 = _18788;
    Interval _18795 = jet_mul_derivative(_18225, _18226, intervalFailed, optical_product_upper);
    Interval _18227 = _18795;
    Interval _18796 = jet_add_derivative(_18224, _18227, intervalFailed);
    Interval _18228 = Interval{ 0.0, 0.0 };
    Interval _18229 = Interval{ _18785, interval_sine_upper };
    Interval _18798 = jet_mul_derivative(_18228, _18229, intervalFailed, optical_product_upper);
    Interval _18230 = _18798;
    Interval _18231 = _18761;
    Interval _18232 = _18790;
    Interval _18799 = jet_mul_derivative(_18231, _18232, intervalFailed, optical_product_upper);
    Interval _18233 = _18799;
    Interval _18800 = jet_add_derivative(_18230, _18233, intervalFailed);
    Interval _18206 = _18792;
    Interval _18207 = _18612;
    Interval _18801 = imul(_18206, _18207, intervalFailed, optical_product_upper);
    Interval _18208 = _18796;
    Interval _18209 = _18612;
    Interval _18802 = jet_mul_derivative(_18208, _18209, intervalFailed, optical_product_upper);
    Interval _18210 = _18802;
    Interval _18211 = _18792;
    Interval _18212 = _18639;
    Interval _18803 = jet_mul_derivative(_18211, _18212, intervalFailed, optical_product_upper);
    Interval _18213 = _18803;
    Interval _18804 = jet_add_derivative(_18210, _18213, intervalFailed);
    Interval _18214 = _18800;
    Interval _18215 = _18612;
    Interval _18805 = jet_mul_derivative(_18214, _18215, intervalFailed, optical_product_upper);
    Interval _18216 = _18805;
    Interval _18217 = _18792;
    Interval _18218 = _18651;
    Interval _18806 = jet_mul_derivative(_18217, _18218, intervalFailed, optical_product_upper);
    Interval _18219 = _18806;
    Interval _18807 = jet_add_derivative(_18216, _18219, intervalFailed);
    Interval _18200 = _18755;
    Interval _18201 = _18801;
    Interval _18808 = iadd(_18200, _18201, intervalFailed);
    Interval _18202 = _18757;
    Interval _18203 = _18804;
    Interval _18809 = jet_add_derivative(_18202, _18203, intervalFailed);
    Interval _18204 = _18759;
    Interval _18205 = _18807;
    Interval _18810 = jet_add_derivative(_18204, _18205, intervalFailed);
    float _18198 = 47.0;
    float _18199 = 1000.0;
    Interval _18816 = iratio(_18198, _18199, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18821;
    if (!intervalFailed)
    {
        _18821 = intervalFailed;
    }
    else
    {
        _18821 = false;
    }
    bool _18826;
    if (_18821)
    {
        _18826 = jetFailureSite == 0u;
    }
    else
    {
        _18826 = false;
    }
    if (_18826)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(47.0, 47.0, 1000.0, 1000.0);
    }
    Interval _18184 = x.v;
    Interval _18185 = _18816;
    Interval _18829 = imul(_18184, _18185, intervalFailed, optical_product_upper);
    Interval _18186 = x.dx;
    Interval _18187 = _18816;
    Interval _18830 = jet_mul_derivative(_18186, _18187, intervalFailed, optical_product_upper);
    Interval _18188 = _18830;
    Interval _18189 = x.v;
    Interval _18190 = Interval{ 0.0, 0.0 };
    Interval _18831 = jet_mul_derivative(_18189, _18190, intervalFailed, optical_product_upper);
    Interval _18191 = _18831;
    Interval _18832 = jet_add_derivative(_18188, _18191, intervalFailed);
    Interval _18192 = x.dy;
    Interval _18193 = _18816;
    Interval _18833 = jet_mul_derivative(_18192, _18193, intervalFailed, optical_product_upper);
    Interval _18194 = _18833;
    Interval _18195 = x.v;
    Interval _18196 = Interval{ 0.0, 0.0 };
    Interval _18834 = jet_mul_derivative(_18195, _18196, intervalFailed, optical_product_upper);
    Interval _18197 = _18834;
    Interval _18835 = jet_add_derivative(_18194, _18197, intervalFailed);
    float _18182 = 25.0;
    float _18183 = 1000.0;
    Interval _18841 = iratio(_18182, _18183, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18846;
    if (!intervalFailed)
    {
        _18846 = intervalFailed;
    }
    else
    {
        _18846 = false;
    }
    bool _18851;
    if (_18846)
    {
        _18851 = jetFailureSite == 0u;
    }
    else
    {
        _18851 = false;
    }
    if (_18851)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
    }
    Interval _18168 = z.v;
    Interval _18169 = _18841;
    Interval _18854 = imul(_18168, _18169, intervalFailed, optical_product_upper);
    Interval _18170 = z.dx;
    Interval _18171 = _18841;
    Interval _18855 = jet_mul_derivative(_18170, _18171, intervalFailed, optical_product_upper);
    Interval _18172 = _18855;
    Interval _18173 = z.v;
    Interval _18174 = Interval{ 0.0, 0.0 };
    Interval _18856 = jet_mul_derivative(_18173, _18174, intervalFailed, optical_product_upper);
    Interval _18175 = _18856;
    Interval _18857 = jet_add_derivative(_18172, _18175, intervalFailed);
    Interval _18176 = z.dy;
    Interval _18177 = _18841;
    Interval _18858 = jet_mul_derivative(_18176, _18177, intervalFailed, optical_product_upper);
    Interval _18178 = _18858;
    Interval _18179 = z.v;
    Interval _18180 = Interval{ 0.0, 0.0 };
    Interval _18859 = jet_mul_derivative(_18179, _18180, intervalFailed, optical_product_upper);
    Interval _18181 = _18859;
    Interval _18860 = jet_add_derivative(_18178, _18181, intervalFailed);
    Interval _18162 = _18829;
    Interval _18163 = Interval{ as_type<float>(as_type<uint>(_18854.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18854.lo) ^ 2147483648u) };
    Interval _18886 = iadd(_18162, _18163, intervalFailed);
    Interval _18164 = _18832;
    Interval _18165 = Interval{ as_type<float>(as_type<uint>(_18857.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18857.lo) ^ 2147483648u) };
    Interval _18888 = jet_add_derivative(_18164, _18165, intervalFailed);
    Interval _18166 = _18835;
    Interval _18167 = Interval{ as_type<float>(as_type<uint>(_18860.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18860.lo) ^ 2147483648u) };
    Interval _18890 = jet_add_derivative(_18166, _18167, intervalFailed);
    float _18160 = 60.0;
    float _18161 = 100.0;
    Interval _18896 = iratio(_18160, _18161, intervalFailed, optical_product_upper, interval_divide_upper);
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
        jetFailureArguments = float4(60.0, 60.0, 100.0, 100.0);
    }
    Interval _18146 = t.v;
    Interval _18147 = _18896;
    Interval _18909 = imul(_18146, _18147, intervalFailed, optical_product_upper);
    Interval _18148 = t.dx;
    Interval _18149 = _18896;
    Interval _18910 = jet_mul_derivative(_18148, _18149, intervalFailed, optical_product_upper);
    Interval _18150 = _18910;
    Interval _18151 = t.v;
    Interval _18152 = Interval{ 0.0, 0.0 };
    Interval _18911 = jet_mul_derivative(_18151, _18152, intervalFailed, optical_product_upper);
    Interval _18153 = _18911;
    Interval _18912 = jet_add_derivative(_18150, _18153, intervalFailed);
    Interval _18154 = t.dy;
    Interval _18155 = _18896;
    Interval _18913 = jet_mul_derivative(_18154, _18155, intervalFailed, optical_product_upper);
    Interval _18156 = _18913;
    Interval _18157 = t.v;
    Interval _18158 = Interval{ 0.0, 0.0 };
    Interval _18914 = jet_mul_derivative(_18157, _18158, intervalFailed, optical_product_upper);
    Interval _18159 = _18914;
    Interval _18915 = jet_add_derivative(_18156, _18159, intervalFailed);
    Interval _18140 = _18886;
    Interval _18141 = _18909;
    Interval _18916 = iadd(_18140, _18141, intervalFailed);
    Interval _18142 = _18888;
    Interval _18143 = _18912;
    Interval _18917 = jet_add_derivative(_18142, _18143, intervalFailed);
    Interval _18144 = _18890;
    Interval _18145 = _18915;
    Interval _18918 = jet_add_derivative(_18144, _18145, intervalFailed);
    float _18138 = 22.0;
    float _18139 = 1000.0;
    Interval _18924 = iratio(_18138, _18139, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18929;
    if (!intervalFailed)
    {
        _18929 = intervalFailed;
    }
    else
    {
        _18929 = false;
    }
    bool _18934;
    if (_18929)
    {
        _18934 = jetFailureSite == 0u;
    }
    else
    {
        _18934 = false;
    }
    if (_18934)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
    }
    Interval _18124 = z.v;
    Interval _18125 = _18924;
    Interval _18937 = imul(_18124, _18125, intervalFailed, optical_product_upper);
    Interval _18126 = z.dx;
    Interval _18127 = _18924;
    Interval _18938 = jet_mul_derivative(_18126, _18127, intervalFailed, optical_product_upper);
    Interval _18128 = _18938;
    Interval _18129 = z.v;
    Interval _18130 = Interval{ 0.0, 0.0 };
    Interval _18939 = jet_mul_derivative(_18129, _18130, intervalFailed, optical_product_upper);
    Interval _18131 = _18939;
    Interval _18940 = jet_add_derivative(_18128, _18131, intervalFailed);
    Interval _18132 = z.dy;
    Interval _18133 = _18924;
    Interval _18941 = jet_mul_derivative(_18132, _18133, intervalFailed, optical_product_upper);
    Interval _18134 = _18941;
    Interval _18135 = z.v;
    Interval _18136 = Interval{ 0.0, 0.0 };
    Interval _18942 = jet_mul_derivative(_18135, _18136, intervalFailed, optical_product_upper);
    Interval _18137 = _18942;
    Interval _18943 = jet_add_derivative(_18134, _18137, intervalFailed);
    float _18122 = 9.0;
    float _18123 = 1000.0;
    Interval _18949 = iratio(_18122, _18123, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _18954;
    if (!intervalFailed)
    {
        _18954 = intervalFailed;
    }
    else
    {
        _18954 = false;
    }
    bool _18959;
    if (_18954)
    {
        _18959 = jetFailureSite == 0u;
    }
    else
    {
        _18959 = false;
    }
    if (_18959)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(9.0, 9.0, 1000.0, 1000.0);
    }
    Interval _18108 = x.v;
    Interval _18109 = _18949;
    Interval _18962 = imul(_18108, _18109, intervalFailed, optical_product_upper);
    Interval _18110 = x.dx;
    Interval _18111 = _18949;
    Interval _18963 = jet_mul_derivative(_18110, _18111, intervalFailed, optical_product_upper);
    Interval _18112 = _18963;
    Interval _18113 = x.v;
    Interval _18114 = Interval{ 0.0, 0.0 };
    Interval _18964 = jet_mul_derivative(_18113, _18114, intervalFailed, optical_product_upper);
    Interval _18115 = _18964;
    Interval _18965 = jet_add_derivative(_18112, _18115, intervalFailed);
    Interval _18116 = x.dy;
    Interval _18117 = _18949;
    Interval _18966 = jet_mul_derivative(_18116, _18117, intervalFailed, optical_product_upper);
    Interval _18118 = _18966;
    Interval _18119 = x.v;
    Interval _18120 = Interval{ 0.0, 0.0 };
    Interval _18967 = jet_mul_derivative(_18119, _18120, intervalFailed, optical_product_upper);
    Interval _18121 = _18967;
    Interval _18968 = jet_add_derivative(_18118, _18121, intervalFailed);
    Interval _18102 = _18937;
    Interval _18103 = Interval{ as_type<float>(as_type<uint>(_18962.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18962.lo) ^ 2147483648u) };
    Interval _18994 = iadd(_18102, _18103, intervalFailed);
    Interval _18104 = _18940;
    Interval _18105 = Interval{ as_type<float>(as_type<uint>(_18965.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18965.lo) ^ 2147483648u) };
    Interval _18996 = jet_add_derivative(_18104, _18105, intervalFailed);
    Interval _18106 = _18943;
    Interval _18107 = Interval{ as_type<float>(as_type<uint>(_18968.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_18968.lo) ^ 2147483648u) };
    Interval _18998 = jet_add_derivative(_18106, _18107, intervalFailed);
    float _18100 = 32.0;
    float _18101 = 100.0;
    Interval _19004 = iratio(_18100, _18101, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19009;
    if (!intervalFailed)
    {
        _19009 = intervalFailed;
    }
    else
    {
        _19009 = false;
    }
    bool _19014;
    if (_19009)
    {
        _19014 = jetFailureSite == 0u;
    }
    else
    {
        _19014 = false;
    }
    if (_19014)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(32.0, 32.0, 100.0, 100.0);
    }
    Interval _18086 = t.v;
    Interval _18087 = _19004;
    Interval _19017 = imul(_18086, _18087, intervalFailed, optical_product_upper);
    Interval _18088 = t.dx;
    Interval _18089 = _19004;
    Interval _19018 = jet_mul_derivative(_18088, _18089, intervalFailed, optical_product_upper);
    Interval _18090 = _19018;
    Interval _18091 = t.v;
    Interval _18092 = Interval{ 0.0, 0.0 };
    Interval _19019 = jet_mul_derivative(_18091, _18092, intervalFailed, optical_product_upper);
    Interval _18093 = _19019;
    Interval _19020 = jet_add_derivative(_18090, _18093, intervalFailed);
    Interval _18094 = t.dy;
    Interval _18095 = _19004;
    Interval _19021 = jet_mul_derivative(_18094, _18095, intervalFailed, optical_product_upper);
    Interval _18096 = _19021;
    Interval _18097 = t.v;
    Interval _18098 = Interval{ 0.0, 0.0 };
    Interval _19022 = jet_mul_derivative(_18097, _18098, intervalFailed, optical_product_upper);
    Interval _18099 = _19022;
    Interval _19023 = jet_add_derivative(_18096, _18099, intervalFailed);
    Interval _18080 = _18994;
    Interval _18081 = Interval{ as_type<float>(as_type<uint>(_19017.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19017.lo) ^ 2147483648u) };
    Interval _19049 = iadd(_18080, _18081, intervalFailed);
    Interval _18082 = _18996;
    Interval _18083 = Interval{ as_type<float>(as_type<uint>(_19020.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19020.lo) ^ 2147483648u) };
    Interval _19051 = jet_add_derivative(_18082, _18083, intervalFailed);
    Interval _18084 = _18998;
    Interval _18085 = Interval{ as_type<float>(as_type<uint>(_19023.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19023.lo) ^ 2147483648u) };
    Interval _19053 = jet_add_derivative(_18084, _18085, intervalFailed);
    float _18078 = 22.0;
    float _18079 = 1000.0;
    Interval _19056 = iratio(_18078, _18079, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19061;
    if (!intervalFailed)
    {
        _19061 = intervalFailed;
    }
    else
    {
        _19061 = false;
    }
    bool _19066;
    if (_19061)
    {
        _19066 = jetFailureSite == 0u;
    }
    else
    {
        _19066 = false;
    }
    if (_19066)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
    }
    Interval _18064 = footprint.v;
    Interval _18065 = _19056;
    Interval _19072 = imul(_18064, _18065, intervalFailed, optical_product_upper);
    Interval _18066 = footprint.dx;
    Interval _18067 = _19056;
    Interval _19073 = jet_mul_derivative(_18066, _18067, intervalFailed, optical_product_upper);
    Interval _18068 = _19073;
    Interval _18069 = footprint.v;
    Interval _18070 = Interval{ 0.0, 0.0 };
    Interval _19074 = jet_mul_derivative(_18069, _18070, intervalFailed, optical_product_upper);
    Interval _18071 = _19074;
    Interval _19075 = jet_add_derivative(_18068, _18071, intervalFailed);
    Interval _18072 = footprint.dy;
    Interval _18073 = _19056;
    Interval _19076 = jet_mul_derivative(_18072, _18073, intervalFailed, optical_product_upper);
    Interval _18074 = _19076;
    Interval _18075 = footprint.v;
    Interval _18076 = Interval{ 0.0, 0.0 };
    Interval _19077 = jet_mul_derivative(_18075, _18076, intervalFailed, optical_product_upper);
    Interval _18077 = _19077;
    Interval _19078 = jet_add_derivative(_18074, _18077, intervalFailed);
    bool _19085;
    if (_19072.lo <= 0.0)
    {
        _19085 = _19072.hi >= 0.0;
    }
    else
    {
        _19085 = false;
    }
    float _19092;
    if (_19085)
    {
        _19092 = 0.0;
    }
    else
    {
        _19092 = precise::min(abs(_19072.lo), abs(_19072.hi));
    }
    float _19095 = precise::max(abs(_19072.lo), abs(_19072.hi));
    float _18054 = spvFMul(_19092, _19092);
    float _19096 = interval_down(_18054, intervalFailed);
    float _19097 = precise::max(0.0, _19096);
    float _18055 = spvFMul(_19095, _19095);
    float _19098 = interval_up(_18055, intervalFailed);
    Interval _18056 = Interval{ 2.0, 2.0 };
    Interval _18057 = _19072;
    Interval _19099 = imul(_18056, _18057, intervalFailed, optical_product_upper);
    Interval _18058 = _19099;
    Interval _18059 = _19075;
    Interval _19100 = jet_mul_derivative(_18058, _18059, intervalFailed, optical_product_upper);
    Interval _18060 = Interval{ 2.0, 2.0 };
    Interval _18061 = _19072;
    Interval _19101 = imul(_18060, _18061, intervalFailed, optical_product_upper);
    Interval _18062 = _19101;
    Interval _18063 = _19078;
    Interval _19102 = jet_mul_derivative(_18062, _18063, intervalFailed, optical_product_upper);
    bool _19107;
    if (_19097 <= 0.0)
    {
        _19107 = _19098 >= 0.0;
    }
    else
    {
        _19107 = false;
    }
    float _19114;
    if (_19107)
    {
        _19114 = 0.0;
    }
    else
    {
        _19114 = precise::min(abs(_19097), abs(_19098));
    }
    float _19117 = precise::max(abs(_19097), abs(_19098));
    float _18044 = spvFMul(_19114, _19114);
    float _19118 = interval_down(_18044, intervalFailed);
    float _18045 = spvFMul(_19117, _19117);
    float _19120 = interval_up(_18045, intervalFailed);
    Interval _18046 = Interval{ 2.0, 2.0 };
    Interval _18047 = Interval{ _19097, _19098 };
    Interval _19122 = imul(_18046, _18047, intervalFailed, optical_product_upper);
    Interval _18048 = _19122;
    Interval _18049 = _19100;
    Interval _19123 = jet_mul_derivative(_18048, _18049, intervalFailed, optical_product_upper);
    Interval _18050 = Interval{ 2.0, 2.0 };
    Interval _18051 = Interval{ _19097, _19098 };
    Interval _19125 = imul(_18050, _18051, intervalFailed, optical_product_upper);
    Interval _18052 = _19125;
    Interval _18053 = _19102;
    Interval _19126 = jet_mul_derivative(_18052, _18053, intervalFailed, optical_product_upper);
    Interval _18038 = Interval{ 1.0, 1.0 };
    Interval _18039 = Interval{ precise::max(0.0, _19118), _19120 };
    Interval _19128 = iadd(_18038, _18039, intervalFailed);
    Interval _18040 = Interval{ 0.0, 0.0 };
    Interval _18041 = _19123;
    Interval _19129 = jet_add_derivative(_18040, _18041, intervalFailed);
    Interval _18042 = Interval{ 0.0, 0.0 };
    Interval _18043 = _19126;
    Interval _19130 = jet_add_derivative(_18042, _18043, intervalFailed);
    Interval _18024 = Interval{ 1.0, 1.0 };
    Interval _18025 = _19128;
    Interval _19132 = idiv(_18024, _18025, intervalFailed, interval_divide_upper);
    bool _19139;
    if (!intervalFailed)
    {
        _19139 = intervalFailed;
    }
    else
    {
        _19139 = false;
    }
    bool _19144;
    if (_19139)
    {
        _19144 = jetFailureSite == 0u;
    }
    else
    {
        _19144 = false;
    }
    if (_19144)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19128.lo, _19128.hi);
    }
    Interval _18026 = Interval{ 0.0, 0.0 };
    Interval _18027 = _19132;
    Interval _18028 = _19129;
    Interval _19148 = jet_mul_derivative(_18027, _18028, intervalFailed, optical_product_upper);
    Interval _18029 = Interval{ as_type<float>(as_type<uint>(_19148.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19148.lo) ^ 2147483648u) };
    Interval _19158 = jet_add_derivative(_18026, _18029, intervalFailed);
    Interval _18030 = _19158;
    Interval _18031 = _19128;
    Interval _19159 = jet_div_derivative(_18030, _18031, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _18032 = Interval{ 0.0, 0.0 };
    Interval _18033 = _19132;
    Interval _18034 = _19130;
    Interval _19160 = jet_mul_derivative(_18033, _18034, intervalFailed, optical_product_upper);
    Interval _18035 = Interval{ as_type<float>(as_type<uint>(_19160.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19160.lo) ^ 2147483648u) };
    Interval _19170 = jet_add_derivative(_18032, _18035, intervalFailed);
    Interval _18036 = _19170;
    Interval _18037 = _19128;
    Interval _19171 = jet_div_derivative(_18036, _18037, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _18022 = 54.0;
    float _18023 = 1000.0;
    Interval _19174 = iratio(_18022, _18023, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19179;
    if (!intervalFailed)
    {
        _19179 = intervalFailed;
    }
    else
    {
        _19179 = false;
    }
    bool _19184;
    if (_19179)
    {
        _19184 = jetFailureSite == 0u;
    }
    else
    {
        _19184 = false;
    }
    if (_19184)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
    }
    Interval _18008 = footprint.v;
    Interval _18009 = _19174;
    Interval _19190 = imul(_18008, _18009, intervalFailed, optical_product_upper);
    Interval _18010 = footprint.dx;
    Interval _18011 = _19174;
    Interval _19191 = jet_mul_derivative(_18010, _18011, intervalFailed, optical_product_upper);
    Interval _18012 = _19191;
    Interval _18013 = footprint.v;
    Interval _18014 = Interval{ 0.0, 0.0 };
    Interval _19192 = jet_mul_derivative(_18013, _18014, intervalFailed, optical_product_upper);
    Interval _18015 = _19192;
    Interval _19193 = jet_add_derivative(_18012, _18015, intervalFailed);
    Interval _18016 = footprint.dy;
    Interval _18017 = _19174;
    Interval _19194 = jet_mul_derivative(_18016, _18017, intervalFailed, optical_product_upper);
    Interval _18018 = _19194;
    Interval _18019 = footprint.v;
    Interval _18020 = Interval{ 0.0, 0.0 };
    Interval _19195 = jet_mul_derivative(_18019, _18020, intervalFailed, optical_product_upper);
    Interval _18021 = _19195;
    Interval _19196 = jet_add_derivative(_18018, _18021, intervalFailed);
    bool _19203;
    if (_19190.lo <= 0.0)
    {
        _19203 = _19190.hi >= 0.0;
    }
    else
    {
        _19203 = false;
    }
    float _19210;
    if (_19203)
    {
        _19210 = 0.0;
    }
    else
    {
        _19210 = precise::min(abs(_19190.lo), abs(_19190.hi));
    }
    float _19213 = precise::max(abs(_19190.lo), abs(_19190.hi));
    float _17998 = spvFMul(_19210, _19210);
    float _19214 = interval_down(_17998, intervalFailed);
    float _19215 = precise::max(0.0, _19214);
    float _17999 = spvFMul(_19213, _19213);
    float _19216 = interval_up(_17999, intervalFailed);
    Interval _18000 = Interval{ 2.0, 2.0 };
    Interval _18001 = _19190;
    Interval _19217 = imul(_18000, _18001, intervalFailed, optical_product_upper);
    Interval _18002 = _19217;
    Interval _18003 = _19193;
    Interval _19218 = jet_mul_derivative(_18002, _18003, intervalFailed, optical_product_upper);
    Interval _18004 = Interval{ 2.0, 2.0 };
    Interval _18005 = _19190;
    Interval _19219 = imul(_18004, _18005, intervalFailed, optical_product_upper);
    Interval _18006 = _19219;
    Interval _18007 = _19196;
    Interval _19220 = jet_mul_derivative(_18006, _18007, intervalFailed, optical_product_upper);
    bool _19225;
    if (_19215 <= 0.0)
    {
        _19225 = _19216 >= 0.0;
    }
    else
    {
        _19225 = false;
    }
    float _19232;
    if (_19225)
    {
        _19232 = 0.0;
    }
    else
    {
        _19232 = precise::min(abs(_19215), abs(_19216));
    }
    float _19235 = precise::max(abs(_19215), abs(_19216));
    float _17988 = spvFMul(_19232, _19232);
    float _19236 = interval_down(_17988, intervalFailed);
    float _17989 = spvFMul(_19235, _19235);
    float _19238 = interval_up(_17989, intervalFailed);
    Interval _17990 = Interval{ 2.0, 2.0 };
    Interval _17991 = Interval{ _19215, _19216 };
    Interval _19240 = imul(_17990, _17991, intervalFailed, optical_product_upper);
    Interval _17992 = _19240;
    Interval _17993 = _19218;
    Interval _19241 = jet_mul_derivative(_17992, _17993, intervalFailed, optical_product_upper);
    Interval _17994 = Interval{ 2.0, 2.0 };
    Interval _17995 = Interval{ _19215, _19216 };
    Interval _19243 = imul(_17994, _17995, intervalFailed, optical_product_upper);
    Interval _17996 = _19243;
    Interval _17997 = _19220;
    Interval _19244 = jet_mul_derivative(_17996, _17997, intervalFailed, optical_product_upper);
    Interval _17982 = Interval{ 1.0, 1.0 };
    Interval _17983 = Interval{ precise::max(0.0, _19236), _19238 };
    Interval _19246 = iadd(_17982, _17983, intervalFailed);
    Interval _17984 = Interval{ 0.0, 0.0 };
    Interval _17985 = _19241;
    Interval _19247 = jet_add_derivative(_17984, _17985, intervalFailed);
    Interval _17986 = Interval{ 0.0, 0.0 };
    Interval _17987 = _19244;
    Interval _19248 = jet_add_derivative(_17986, _17987, intervalFailed);
    Interval _17968 = Interval{ 1.0, 1.0 };
    Interval _17969 = _19246;
    Interval _19250 = idiv(_17968, _17969, intervalFailed, interval_divide_upper);
    bool _19257;
    if (!intervalFailed)
    {
        _19257 = intervalFailed;
    }
    else
    {
        _19257 = false;
    }
    bool _19262;
    if (_19257)
    {
        _19262 = jetFailureSite == 0u;
    }
    else
    {
        _19262 = false;
    }
    if (_19262)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19246.lo, _19246.hi);
    }
    Interval _17970 = Interval{ 0.0, 0.0 };
    Interval _17971 = _19250;
    Interval _17972 = _19247;
    Interval _19266 = jet_mul_derivative(_17971, _17972, intervalFailed, optical_product_upper);
    Interval _17973 = Interval{ as_type<float>(as_type<uint>(_19266.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19266.lo) ^ 2147483648u) };
    Interval _19276 = jet_add_derivative(_17970, _17973, intervalFailed);
    Interval _17974 = _19276;
    Interval _17975 = _19246;
    Interval _19277 = jet_div_derivative(_17974, _17975, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17976 = Interval{ 0.0, 0.0 };
    Interval _17977 = _19250;
    Interval _17978 = _19248;
    Interval _19278 = jet_mul_derivative(_17977, _17978, intervalFailed, optical_product_upper);
    Interval _17979 = Interval{ as_type<float>(as_type<uint>(_19278.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19278.lo) ^ 2147483648u) };
    Interval _19288 = jet_add_derivative(_17976, _17979, intervalFailed);
    Interval _17980 = _19288;
    Interval _17981 = _19246;
    Interval _19289 = jet_div_derivative(_17980, _17981, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _17966 = 24.0;
    float _17967 = 1000.0;
    Interval _19292 = iratio(_17966, _17967, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19297;
    if (!intervalFailed)
    {
        _19297 = intervalFailed;
    }
    else
    {
        _19297 = false;
    }
    bool _19302;
    if (_19297)
    {
        _19302 = jetFailureSite == 0u;
    }
    else
    {
        _19302 = false;
    }
    if (_19302)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(24.0, 24.0, 1000.0, 1000.0);
    }
    Interval _17952 = footprint.v;
    Interval _17953 = _19292;
    Interval _19308 = imul(_17952, _17953, intervalFailed, optical_product_upper);
    Interval _17954 = footprint.dx;
    Interval _17955 = _19292;
    Interval _19309 = jet_mul_derivative(_17954, _17955, intervalFailed, optical_product_upper);
    Interval _17956 = _19309;
    Interval _17957 = footprint.v;
    Interval _17958 = Interval{ 0.0, 0.0 };
    Interval _19310 = jet_mul_derivative(_17957, _17958, intervalFailed, optical_product_upper);
    Interval _17959 = _19310;
    Interval _19311 = jet_add_derivative(_17956, _17959, intervalFailed);
    Interval _17960 = footprint.dy;
    Interval _17961 = _19292;
    Interval _19312 = jet_mul_derivative(_17960, _17961, intervalFailed, optical_product_upper);
    Interval _17962 = _19312;
    Interval _17963 = footprint.v;
    Interval _17964 = Interval{ 0.0, 0.0 };
    Interval _19313 = jet_mul_derivative(_17963, _17964, intervalFailed, optical_product_upper);
    Interval _17965 = _19313;
    Interval _19314 = jet_add_derivative(_17962, _17965, intervalFailed);
    bool _19321;
    if (_19308.lo <= 0.0)
    {
        _19321 = _19308.hi >= 0.0;
    }
    else
    {
        _19321 = false;
    }
    float _19328;
    if (_19321)
    {
        _19328 = 0.0;
    }
    else
    {
        _19328 = precise::min(abs(_19308.lo), abs(_19308.hi));
    }
    float _19331 = precise::max(abs(_19308.lo), abs(_19308.hi));
    float _17942 = spvFMul(_19328, _19328);
    float _19332 = interval_down(_17942, intervalFailed);
    float _19333 = precise::max(0.0, _19332);
    float _17943 = spvFMul(_19331, _19331);
    float _19334 = interval_up(_17943, intervalFailed);
    Interval _17944 = Interval{ 2.0, 2.0 };
    Interval _17945 = _19308;
    Interval _19335 = imul(_17944, _17945, intervalFailed, optical_product_upper);
    Interval _17946 = _19335;
    Interval _17947 = _19311;
    Interval _19336 = jet_mul_derivative(_17946, _17947, intervalFailed, optical_product_upper);
    Interval _17948 = Interval{ 2.0, 2.0 };
    Interval _17949 = _19308;
    Interval _19337 = imul(_17948, _17949, intervalFailed, optical_product_upper);
    Interval _17950 = _19337;
    Interval _17951 = _19314;
    Interval _19338 = jet_mul_derivative(_17950, _17951, intervalFailed, optical_product_upper);
    bool _19343;
    if (_19333 <= 0.0)
    {
        _19343 = _19334 >= 0.0;
    }
    else
    {
        _19343 = false;
    }
    float _19350;
    if (_19343)
    {
        _19350 = 0.0;
    }
    else
    {
        _19350 = precise::min(abs(_19333), abs(_19334));
    }
    float _19353 = precise::max(abs(_19333), abs(_19334));
    float _17932 = spvFMul(_19350, _19350);
    float _19354 = interval_down(_17932, intervalFailed);
    float _17933 = spvFMul(_19353, _19353);
    float _19356 = interval_up(_17933, intervalFailed);
    Interval _17934 = Interval{ 2.0, 2.0 };
    Interval _17935 = Interval{ _19333, _19334 };
    Interval _19358 = imul(_17934, _17935, intervalFailed, optical_product_upper);
    Interval _17936 = _19358;
    Interval _17937 = _19336;
    Interval _19359 = jet_mul_derivative(_17936, _17937, intervalFailed, optical_product_upper);
    Interval _17938 = Interval{ 2.0, 2.0 };
    Interval _17939 = Interval{ _19333, _19334 };
    Interval _19361 = imul(_17938, _17939, intervalFailed, optical_product_upper);
    Interval _17940 = _19361;
    Interval _17941 = _19338;
    Interval _19362 = jet_mul_derivative(_17940, _17941, intervalFailed, optical_product_upper);
    Interval _17926 = Interval{ 1.0, 1.0 };
    Interval _17927 = Interval{ precise::max(0.0, _19354), _19356 };
    Interval _19364 = iadd(_17926, _17927, intervalFailed);
    Interval _17928 = Interval{ 0.0, 0.0 };
    Interval _17929 = _19359;
    Interval _19365 = jet_add_derivative(_17928, _17929, intervalFailed);
    Interval _17930 = Interval{ 0.0, 0.0 };
    Interval _17931 = _19362;
    Interval _19366 = jet_add_derivative(_17930, _17931, intervalFailed);
    Interval _17912 = Interval{ 1.0, 1.0 };
    Interval _17913 = _19364;
    Interval _19368 = idiv(_17912, _17913, intervalFailed, interval_divide_upper);
    bool _19375;
    if (!intervalFailed)
    {
        _19375 = intervalFailed;
    }
    else
    {
        _19375 = false;
    }
    bool _19380;
    if (_19375)
    {
        _19380 = jetFailureSite == 0u;
    }
    else
    {
        _19380 = false;
    }
    if (_19380)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _19364.lo, _19364.hi);
    }
    Interval _17914 = Interval{ 0.0, 0.0 };
    Interval _17915 = _19368;
    Interval _17916 = _19365;
    Interval _19384 = jet_mul_derivative(_17915, _17916, intervalFailed, optical_product_upper);
    Interval _17917 = Interval{ as_type<float>(as_type<uint>(_19384.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19384.lo) ^ 2147483648u) };
    Interval _19394 = jet_add_derivative(_17914, _17917, intervalFailed);
    Interval _17918 = _19394;
    Interval _17919 = _19364;
    Interval _19395 = jet_div_derivative(_17918, _17919, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17920 = Interval{ 0.0, 0.0 };
    Interval _17921 = _19368;
    Interval _17922 = _19366;
    Interval _19396 = jet_mul_derivative(_17921, _17922, intervalFailed, optical_product_upper);
    Interval _17923 = Interval{ as_type<float>(as_type<uint>(_19396.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19396.lo) ^ 2147483648u) };
    Interval _19406 = jet_add_derivative(_17920, _17923, intervalFailed);
    Interval _17924 = _19406;
    Interval _17925 = _19364;
    Interval _19407 = jet_div_derivative(_17924, _17925, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17904 = _18808;
    float _17902 = 3.1415927410125732421875;
    float _19408 = interval_down(_17902, intervalFailed);
    float _17903 = 3.1415927410125732421875;
    float _19409 = interval_up(_17903, intervalFailed);
    Interval _17905 = Interval{ _19408, _19409 };
    Interval _17906 = Interval{ 0.5, 0.5 };
    Interval _19411 = imul(_17905, _17906, intervalFailed, optical_product_upper);
    Interval _17907 = _19411;
    Interval _19412 = iadd(_17904, _17907, intervalFailed);
    float _17900 = _19412.lo;
    float _17901 = _19412.hi;
    float _19415 = sine_bounds(_17900, _17901, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17898 = _18808.lo;
    float _17899 = _18808.hi;
    float _19419 = sine_bounds(_17898, _17899, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17908 = Interval{ _19415, interval_sine_upper };
    Interval _17909 = _18809;
    Interval _19422 = jet_mul_derivative(_17908, _17909, intervalFailed, optical_product_upper);
    Interval _17910 = Interval{ _19415, interval_sine_upper };
    Interval _17911 = _18810;
    Interval _19424 = jet_mul_derivative(_17910, _17911, intervalFailed, optical_product_upper);
    Interval _17884 = Interval{ 9.0, 9.0 };
    Interval _17885 = Interval{ _19419, interval_sine_upper };
    Interval _19426 = imul(_17884, _17885, intervalFailed, optical_product_upper);
    Interval _17886 = Interval{ 0.0, 0.0 };
    Interval _17887 = Interval{ _19419, interval_sine_upper };
    Interval _19428 = jet_mul_derivative(_17886, _17887, intervalFailed, optical_product_upper);
    Interval _17888 = _19428;
    Interval _17889 = Interval{ 9.0, 9.0 };
    Interval _17890 = _19422;
    Interval _19429 = jet_mul_derivative(_17889, _17890, intervalFailed, optical_product_upper);
    Interval _17891 = _19429;
    Interval _19430 = jet_add_derivative(_17888, _17891, intervalFailed);
    Interval _17892 = Interval{ 0.0, 0.0 };
    Interval _17893 = Interval{ _19419, interval_sine_upper };
    Interval _19432 = jet_mul_derivative(_17892, _17893, intervalFailed, optical_product_upper);
    Interval _17894 = _19432;
    Interval _17895 = Interval{ 9.0, 9.0 };
    Interval _17896 = _19424;
    Interval _19433 = jet_mul_derivative(_17895, _17896, intervalFailed, optical_product_upper);
    Interval _17897 = _19433;
    Interval _19434 = jet_add_derivative(_17894, _17897, intervalFailed);
    Interval _17870 = _19426;
    Interval _17871 = _19132;
    Interval _19435 = imul(_17870, _17871, intervalFailed, optical_product_upper);
    Interval _17872 = _19430;
    Interval _17873 = _19132;
    Interval _19436 = jet_mul_derivative(_17872, _17873, intervalFailed, optical_product_upper);
    Interval _17874 = _19436;
    Interval _17875 = _19426;
    Interval _17876 = _19159;
    Interval _19437 = jet_mul_derivative(_17875, _17876, intervalFailed, optical_product_upper);
    Interval _17877 = _19437;
    Interval _19438 = jet_add_derivative(_17874, _17877, intervalFailed);
    Interval _17878 = _19434;
    Interval _17879 = _19132;
    Interval _19439 = jet_mul_derivative(_17878, _17879, intervalFailed, optical_product_upper);
    Interval _17880 = _19439;
    Interval _17881 = _19426;
    Interval _17882 = _19171;
    Interval _19440 = jet_mul_derivative(_17881, _17882, intervalFailed, optical_product_upper);
    Interval _17883 = _19440;
    Interval _19441 = jet_add_derivative(_17880, _17883, intervalFailed);
    Interval _17862 = _18916;
    float _17860 = 3.1415927410125732421875;
    float _19442 = interval_down(_17860, intervalFailed);
    float _17861 = 3.1415927410125732421875;
    float _19443 = interval_up(_17861, intervalFailed);
    Interval _17863 = Interval{ _19442, _19443 };
    Interval _17864 = Interval{ 0.5, 0.5 };
    Interval _19445 = imul(_17863, _17864, intervalFailed, optical_product_upper);
    Interval _17865 = _19445;
    Interval _19446 = iadd(_17862, _17865, intervalFailed);
    float _17858 = _19446.lo;
    float _17859 = _19446.hi;
    float _19449 = sine_bounds(_17858, _17859, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17856 = _18916.lo;
    float _17857 = _18916.hi;
    float _19453 = sine_bounds(_17856, _17857, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17866 = Interval{ _19449, interval_sine_upper };
    Interval _17867 = _18917;
    Interval _19456 = jet_mul_derivative(_17866, _17867, intervalFailed, optical_product_upper);
    Interval _17868 = Interval{ _19449, interval_sine_upper };
    Interval _17869 = _18918;
    Interval _19458 = jet_mul_derivative(_17868, _17869, intervalFailed, optical_product_upper);
    Interval _17842 = Interval{ 2.5, 2.5 };
    Interval _17843 = Interval{ _19453, interval_sine_upper };
    Interval _19460 = imul(_17842, _17843, intervalFailed, optical_product_upper);
    Interval _17844 = Interval{ 0.0, 0.0 };
    Interval _17845 = Interval{ _19453, interval_sine_upper };
    Interval _19462 = jet_mul_derivative(_17844, _17845, intervalFailed, optical_product_upper);
    Interval _17846 = _19462;
    Interval _17847 = Interval{ 2.5, 2.5 };
    Interval _17848 = _19456;
    Interval _19463 = jet_mul_derivative(_17847, _17848, intervalFailed, optical_product_upper);
    Interval _17849 = _19463;
    Interval _19464 = jet_add_derivative(_17846, _17849, intervalFailed);
    Interval _17850 = Interval{ 0.0, 0.0 };
    Interval _17851 = Interval{ _19453, interval_sine_upper };
    Interval _19466 = jet_mul_derivative(_17850, _17851, intervalFailed, optical_product_upper);
    Interval _17852 = _19466;
    Interval _17853 = Interval{ 2.5, 2.5 };
    Interval _17854 = _19458;
    Interval _19467 = jet_mul_derivative(_17853, _17854, intervalFailed, optical_product_upper);
    Interval _17855 = _19467;
    Interval _19468 = jet_add_derivative(_17852, _17855, intervalFailed);
    Interval _17828 = _19460;
    Interval _17829 = _19250;
    Interval _19469 = imul(_17828, _17829, intervalFailed, optical_product_upper);
    Interval _17830 = _19464;
    Interval _17831 = _19250;
    Interval _19470 = jet_mul_derivative(_17830, _17831, intervalFailed, optical_product_upper);
    Interval _17832 = _19470;
    Interval _17833 = _19460;
    Interval _17834 = _19277;
    Interval _19471 = jet_mul_derivative(_17833, _17834, intervalFailed, optical_product_upper);
    Interval _17835 = _19471;
    Interval _19472 = jet_add_derivative(_17832, _17835, intervalFailed);
    Interval _17836 = _19468;
    Interval _17837 = _19250;
    Interval _19473 = jet_mul_derivative(_17836, _17837, intervalFailed, optical_product_upper);
    Interval _17838 = _19473;
    Interval _17839 = _19460;
    Interval _17840 = _19289;
    Interval _19474 = jet_mul_derivative(_17839, _17840, intervalFailed, optical_product_upper);
    Interval _17841 = _19474;
    Interval _19475 = jet_add_derivative(_17838, _17841, intervalFailed);
    Interval _17822 = _19435;
    Interval _17823 = _19469;
    Interval _19476 = iadd(_17822, _17823, intervalFailed);
    Interval _17824 = _19438;
    Interval _17825 = _19472;
    Interval _19477 = jet_add_derivative(_17824, _17825, intervalFailed);
    Interval _17826 = _19441;
    Interval _17827 = _19475;
    Interval _19478 = jet_add_derivative(_17826, _17827, intervalFailed);
    Interval _17814 = _19049;
    float _17812 = 3.1415927410125732421875;
    float _19479 = interval_down(_17812, intervalFailed);
    float _17813 = 3.1415927410125732421875;
    float _19480 = interval_up(_17813, intervalFailed);
    Interval _17815 = Interval{ _19479, _19480 };
    Interval _17816 = Interval{ 0.5, 0.5 };
    Interval _19482 = imul(_17815, _17816, intervalFailed, optical_product_upper);
    Interval _17817 = _19482;
    Interval _19483 = iadd(_17814, _17817, intervalFailed);
    float _17810 = _19483.lo;
    float _17811 = _19483.hi;
    float _19486 = sine_bounds(_17810, _17811, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17808 = _19049.lo;
    float _17809 = _19049.hi;
    float _19490 = sine_bounds(_17808, _17809, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17818 = Interval{ _19486, interval_sine_upper };
    Interval _17819 = _19051;
    Interval _19493 = jet_mul_derivative(_17818, _17819, intervalFailed, optical_product_upper);
    Interval _17820 = Interval{ _19486, interval_sine_upper };
    Interval _17821 = _19053;
    Interval _19495 = jet_mul_derivative(_17820, _17821, intervalFailed, optical_product_upper);
    Interval _17794 = Interval{ 5.0, 5.0 };
    Interval _17795 = Interval{ _19490, interval_sine_upper };
    Interval _19497 = imul(_17794, _17795, intervalFailed, optical_product_upper);
    Interval _17796 = Interval{ 0.0, 0.0 };
    Interval _17797 = Interval{ _19490, interval_sine_upper };
    Interval _19499 = jet_mul_derivative(_17796, _17797, intervalFailed, optical_product_upper);
    Interval _17798 = _19499;
    Interval _17799 = Interval{ 5.0, 5.0 };
    Interval _17800 = _19493;
    Interval _19500 = jet_mul_derivative(_17799, _17800, intervalFailed, optical_product_upper);
    Interval _17801 = _19500;
    Interval _19501 = jet_add_derivative(_17798, _17801, intervalFailed);
    Interval _17802 = Interval{ 0.0, 0.0 };
    Interval _17803 = Interval{ _19490, interval_sine_upper };
    Interval _19503 = jet_mul_derivative(_17802, _17803, intervalFailed, optical_product_upper);
    Interval _17804 = _19503;
    Interval _17805 = Interval{ 5.0, 5.0 };
    Interval _17806 = _19495;
    Interval _19504 = jet_mul_derivative(_17805, _17806, intervalFailed, optical_product_upper);
    Interval _17807 = _19504;
    Interval _19505 = jet_add_derivative(_17804, _17807, intervalFailed);
    Interval _17780 = _19497;
    Interval _17781 = _19368;
    Interval _19506 = imul(_17780, _17781, intervalFailed, optical_product_upper);
    Interval _17782 = _19501;
    Interval _17783 = _19368;
    Interval _19507 = jet_mul_derivative(_17782, _17783, intervalFailed, optical_product_upper);
    Interval _17784 = _19507;
    Interval _17785 = _19497;
    Interval _17786 = _19395;
    Interval _19508 = jet_mul_derivative(_17785, _17786, intervalFailed, optical_product_upper);
    Interval _17787 = _19508;
    Interval _19509 = jet_add_derivative(_17784, _17787, intervalFailed);
    Interval _17788 = _19505;
    Interval _17789 = _19368;
    Interval _19510 = jet_mul_derivative(_17788, _17789, intervalFailed, optical_product_upper);
    Interval _17790 = _19510;
    Interval _17791 = _19497;
    Interval _17792 = _19407;
    Interval _19511 = jet_mul_derivative(_17791, _17792, intervalFailed, optical_product_upper);
    Interval _17793 = _19511;
    Interval _19512 = jet_add_derivative(_17790, _17793, intervalFailed);
    Interval _17774 = _19476;
    Interval _17775 = _19506;
    Interval _19513 = iadd(_17774, _17775, intervalFailed);
    Interval _17776 = _19477;
    Interval _17777 = _19509;
    Interval _19514 = jet_add_derivative(_17776, _17777, intervalFailed);
    Interval _17778 = _19478;
    Interval _17779 = _19512;
    Interval _19515 = jet_add_derivative(_17778, _17779, intervalFailed);
    float _17768 = _18531.lo;
    float _17769 = _18531.hi;
    float _19518 = sine_bounds(_17768, _17769, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19522 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19525 = as_type<float>(as_type<uint>(_19518) ^ 2147483648u);
    Interval _17764 = _18531;
    float _17762 = 3.1415927410125732421875;
    float _19526 = interval_down(_17762, intervalFailed);
    float _17763 = 3.1415927410125732421875;
    float _19527 = interval_up(_17763, intervalFailed);
    Interval _17765 = Interval{ _19526, _19527 };
    Interval _17766 = Interval{ 0.5, 0.5 };
    Interval _19529 = imul(_17765, _17766, intervalFailed, optical_product_upper);
    Interval _17767 = _19529;
    Interval _19530 = iadd(_17764, _17767, intervalFailed);
    float _17760 = _19530.lo;
    float _17761 = _19530.hi;
    float _19533 = sine_bounds(_17760, _17761, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17770 = Interval{ _19522, _19525 };
    Interval _17771 = _18532;
    Interval _19536 = jet_mul_derivative(_17770, _17771, intervalFailed, optical_product_upper);
    Interval _17772 = Interval{ _19522, _19525 };
    Interval _17773 = _18533;
    Interval _19538 = jet_mul_derivative(_17772, _17773, intervalFailed, optical_product_upper);
    Interval _17746 = Interval{ _19533, interval_sine_upper };
    Interval _17747 = _18612;
    Interval _19540 = imul(_17746, _17747, intervalFailed, optical_product_upper);
    Interval _17748 = _19536;
    Interval _17749 = _18612;
    Interval _19541 = jet_mul_derivative(_17748, _17749, intervalFailed, optical_product_upper);
    Interval _17750 = _19541;
    Interval _17751 = Interval{ _19533, interval_sine_upper };
    Interval _17752 = _18639;
    Interval _19543 = jet_mul_derivative(_17751, _17752, intervalFailed, optical_product_upper);
    Interval _17753 = _19543;
    Interval _19544 = jet_add_derivative(_17750, _17753, intervalFailed);
    Interval _17754 = _19538;
    Interval _17755 = _18612;
    Interval _19545 = jet_mul_derivative(_17754, _17755, intervalFailed, optical_product_upper);
    Interval _17756 = _19545;
    Interval _17757 = Interval{ _19533, interval_sine_upper };
    Interval _17758 = _18651;
    Interval _19547 = jet_mul_derivative(_17757, _17758, intervalFailed, optical_product_upper);
    Interval _17759 = _19547;
    Interval _19548 = jet_add_derivative(_17756, _17759, intervalFailed);
    float _17744 = 18.0;
    float _17745 = 1000.0;
    Interval _19550 = iratio(_17744, _17745, intervalFailed, optical_product_upper, interval_divide_upper);
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
        jetFailureSite = 6u;
        jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
    }
    float _17742 = 39.0;
    float _17743 = 10000.0;
    Interval _19564 = iratio(_17742, _17743, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19569;
    if (!intervalFailed)
    {
        _19569 = intervalFailed;
    }
    else
    {
        _19569 = false;
    }
    bool _19574;
    if (_19569)
    {
        _19574 = jetFailureSite == 0u;
    }
    else
    {
        _19574 = false;
    }
    if (_19574)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(39.0, 39.0, 10000.0, 10000.0);
    }
    Interval _17728 = _19564;
    Interval _17729 = _19540;
    Interval _19577 = imul(_17728, _17729, intervalFailed, optical_product_upper);
    Interval _17730 = Interval{ 0.0, 0.0 };
    Interval _17731 = _19540;
    Interval _19578 = jet_mul_derivative(_17730, _17731, intervalFailed, optical_product_upper);
    Interval _17732 = _19578;
    Interval _17733 = _19564;
    Interval _17734 = _19544;
    Interval _19579 = jet_mul_derivative(_17733, _17734, intervalFailed, optical_product_upper);
    Interval _17735 = _19579;
    Interval _19580 = jet_add_derivative(_17732, _17735, intervalFailed);
    Interval _17736 = Interval{ 0.0, 0.0 };
    Interval _17737 = _19540;
    Interval _19581 = jet_mul_derivative(_17736, _17737, intervalFailed, optical_product_upper);
    Interval _17738 = _19581;
    Interval _17739 = _19564;
    Interval _17740 = _19548;
    Interval _19582 = jet_mul_derivative(_17739, _17740, intervalFailed, optical_product_upper);
    Interval _17741 = _19582;
    Interval _19583 = jet_add_derivative(_17738, _17741, intervalFailed);
    Interval _17722 = _19550;
    Interval _17723 = _19577;
    Interval _19584 = iadd(_17722, _17723, intervalFailed);
    Interval _17724 = Interval{ 0.0, 0.0 };
    Interval _17725 = _19580;
    Interval _19585 = jet_add_derivative(_17724, _17725, intervalFailed);
    Interval _17726 = Interval{ 0.0, 0.0 };
    Interval _17727 = _19583;
    Interval _19586 = jet_add_derivative(_17726, _17727, intervalFailed);
    Interval _17708 = Interval{ 9.0, 9.0 };
    Interval _17709 = _19584;
    Interval _19587 = imul(_17708, _17709, intervalFailed, optical_product_upper);
    Interval _17710 = Interval{ 0.0, 0.0 };
    Interval _17711 = _19584;
    Interval _19588 = jet_mul_derivative(_17710, _17711, intervalFailed, optical_product_upper);
    Interval _17712 = _19588;
    Interval _17713 = Interval{ 9.0, 9.0 };
    Interval _17714 = _19585;
    Interval _19589 = jet_mul_derivative(_17713, _17714, intervalFailed, optical_product_upper);
    Interval _17715 = _19589;
    Interval _19590 = jet_add_derivative(_17712, _17715, intervalFailed);
    Interval _17716 = Interval{ 0.0, 0.0 };
    Interval _17717 = _19584;
    Interval _19591 = jet_mul_derivative(_17716, _17717, intervalFailed, optical_product_upper);
    Interval _17718 = _19591;
    Interval _17719 = Interval{ 9.0, 9.0 };
    Interval _17720 = _19586;
    Interval _19592 = jet_mul_derivative(_17719, _17720, intervalFailed, optical_product_upper);
    Interval _17721 = _19592;
    Interval _19593 = jet_add_derivative(_17718, _17721, intervalFailed);
    float _17702 = _18808.lo;
    float _17703 = _18808.hi;
    float _19596 = sine_bounds(_17702, _17703, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19600 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19603 = as_type<float>(as_type<uint>(_19596) ^ 2147483648u);
    Interval _17698 = _18808;
    float _17696 = 3.1415927410125732421875;
    float _19604 = interval_down(_17696, intervalFailed);
    float _17697 = 3.1415927410125732421875;
    float _19605 = interval_up(_17697, intervalFailed);
    Interval _17699 = Interval{ _19604, _19605 };
    Interval _17700 = Interval{ 0.5, 0.5 };
    Interval _19607 = imul(_17699, _17700, intervalFailed, optical_product_upper);
    Interval _17701 = _19607;
    Interval _19608 = iadd(_17698, _17701, intervalFailed);
    float _17694 = _19608.lo;
    float _17695 = _19608.hi;
    float _19611 = sine_bounds(_17694, _17695, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17704 = Interval{ _19600, _19603 };
    Interval _17705 = _18809;
    Interval _19614 = jet_mul_derivative(_17704, _17705, intervalFailed, optical_product_upper);
    Interval _17706 = Interval{ _19600, _19603 };
    Interval _17707 = _18810;
    Interval _19616 = jet_mul_derivative(_17706, _17707, intervalFailed, optical_product_upper);
    Interval _17680 = _19587;
    Interval _17681 = Interval{ _19611, interval_sine_upper };
    Interval _19618 = imul(_17680, _17681, intervalFailed, optical_product_upper);
    Interval _17682 = _19590;
    Interval _17683 = Interval{ _19611, interval_sine_upper };
    Interval _19620 = jet_mul_derivative(_17682, _17683, intervalFailed, optical_product_upper);
    Interval _17684 = _19620;
    Interval _17685 = _19587;
    Interval _17686 = _19614;
    Interval _19621 = jet_mul_derivative(_17685, _17686, intervalFailed, optical_product_upper);
    Interval _17687 = _19621;
    Interval _19622 = jet_add_derivative(_17684, _17687, intervalFailed);
    Interval _17688 = _19593;
    Interval _17689 = Interval{ _19611, interval_sine_upper };
    Interval _19624 = jet_mul_derivative(_17688, _17689, intervalFailed, optical_product_upper);
    Interval _17690 = _19624;
    Interval _17691 = _19587;
    Interval _17692 = _19616;
    Interval _19625 = jet_mul_derivative(_17691, _17692, intervalFailed, optical_product_upper);
    Interval _17693 = _19625;
    Interval _19626 = jet_add_derivative(_17690, _17693, intervalFailed);
    Interval _17666 = _19618;
    Interval _17667 = _19132;
    Interval _19627 = imul(_17666, _17667, intervalFailed, optical_product_upper);
    Interval _17668 = _19622;
    Interval _17669 = _19132;
    Interval _19628 = jet_mul_derivative(_17668, _17669, intervalFailed, optical_product_upper);
    Interval _17670 = _19628;
    Interval _17671 = _19618;
    Interval _17672 = _19159;
    Interval _19629 = jet_mul_derivative(_17671, _17672, intervalFailed, optical_product_upper);
    Interval _17673 = _19629;
    Interval _19630 = jet_add_derivative(_17670, _17673, intervalFailed);
    Interval _17674 = _19626;
    Interval _17675 = _19132;
    Interval _19631 = jet_mul_derivative(_17674, _17675, intervalFailed, optical_product_upper);
    Interval _17676 = _19631;
    Interval _17677 = _19618;
    Interval _17678 = _19171;
    Interval _19632 = jet_mul_derivative(_17677, _17678, intervalFailed, optical_product_upper);
    Interval _17679 = _19632;
    Interval _19633 = jet_add_derivative(_17676, _17679, intervalFailed);
    float _17664 = 1175.0;
    float _17665 = 10000.0;
    Interval _19635 = iratio(_17664, _17665, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19640;
    if (!intervalFailed)
    {
        _19640 = intervalFailed;
    }
    else
    {
        _19640 = false;
    }
    bool _19645;
    if (_19640)
    {
        _19645 = jetFailureSite == 0u;
    }
    else
    {
        _19645 = false;
    }
    if (_19645)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(1175.0, 1175.0, 10000.0, 10000.0);
    }
    float _17658 = _18916.lo;
    float _17659 = _18916.hi;
    float _19650 = sine_bounds(_17658, _17659, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19654 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19657 = as_type<float>(as_type<uint>(_19650) ^ 2147483648u);
    Interval _17654 = _18916;
    float _17652 = 3.1415927410125732421875;
    float _19658 = interval_down(_17652, intervalFailed);
    float _17653 = 3.1415927410125732421875;
    float _19659 = interval_up(_17653, intervalFailed);
    Interval _17655 = Interval{ _19658, _19659 };
    Interval _17656 = Interval{ 0.5, 0.5 };
    Interval _19661 = imul(_17655, _17656, intervalFailed, optical_product_upper);
    Interval _17657 = _19661;
    Interval _19662 = iadd(_17654, _17657, intervalFailed);
    float _17650 = _19662.lo;
    float _17651 = _19662.hi;
    float _19665 = sine_bounds(_17650, _17651, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17660 = Interval{ _19654, _19657 };
    Interval _17661 = _18917;
    Interval _19668 = jet_mul_derivative(_17660, _17661, intervalFailed, optical_product_upper);
    Interval _17662 = Interval{ _19654, _19657 };
    Interval _17663 = _18918;
    Interval _19670 = jet_mul_derivative(_17662, _17663, intervalFailed, optical_product_upper);
    Interval _17636 = _19635;
    Interval _17637 = Interval{ _19665, interval_sine_upper };
    Interval _19672 = imul(_17636, _17637, intervalFailed, optical_product_upper);
    Interval _17638 = Interval{ 0.0, 0.0 };
    Interval _17639 = Interval{ _19665, interval_sine_upper };
    Interval _19674 = jet_mul_derivative(_17638, _17639, intervalFailed, optical_product_upper);
    Interval _17640 = _19674;
    Interval _17641 = _19635;
    Interval _17642 = _19668;
    Interval _19675 = jet_mul_derivative(_17641, _17642, intervalFailed, optical_product_upper);
    Interval _17643 = _19675;
    Interval _19676 = jet_add_derivative(_17640, _17643, intervalFailed);
    Interval _17644 = Interval{ 0.0, 0.0 };
    Interval _17645 = Interval{ _19665, interval_sine_upper };
    Interval _19678 = jet_mul_derivative(_17644, _17645, intervalFailed, optical_product_upper);
    Interval _17646 = _19678;
    Interval _17647 = _19635;
    Interval _17648 = _19670;
    Interval _19679 = jet_mul_derivative(_17647, _17648, intervalFailed, optical_product_upper);
    Interval _17649 = _19679;
    Interval _19680 = jet_add_derivative(_17646, _17649, intervalFailed);
    Interval _17622 = _19672;
    Interval _17623 = _19250;
    Interval _19681 = imul(_17622, _17623, intervalFailed, optical_product_upper);
    Interval _17624 = _19676;
    Interval _17625 = _19250;
    Interval _19682 = jet_mul_derivative(_17624, _17625, intervalFailed, optical_product_upper);
    Interval _17626 = _19682;
    Interval _17627 = _19672;
    Interval _17628 = _19277;
    Interval _19683 = jet_mul_derivative(_17627, _17628, intervalFailed, optical_product_upper);
    Interval _17629 = _19683;
    Interval _19684 = jet_add_derivative(_17626, _17629, intervalFailed);
    Interval _17630 = _19680;
    Interval _17631 = _19250;
    Interval _19685 = jet_mul_derivative(_17630, _17631, intervalFailed, optical_product_upper);
    Interval _17632 = _19685;
    Interval _17633 = _19672;
    Interval _17634 = _19289;
    Interval _19686 = jet_mul_derivative(_17633, _17634, intervalFailed, optical_product_upper);
    Interval _17635 = _19686;
    Interval _19687 = jet_add_derivative(_17632, _17635, intervalFailed);
    Interval _17616 = _19627;
    Interval _17617 = _19681;
    Interval _19688 = iadd(_17616, _17617, intervalFailed);
    Interval _17618 = _19630;
    Interval _17619 = _19684;
    Interval _19689 = jet_add_derivative(_17618, _17619, intervalFailed);
    Interval _17620 = _19633;
    Interval _17621 = _19687;
    Interval _19690 = jet_add_derivative(_17620, _17621, intervalFailed);
    float _17614 = 45.0;
    float _17615 = 1000.0;
    Interval _19692 = iratio(_17614, _17615, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19697;
    if (!intervalFailed)
    {
        _19697 = intervalFailed;
    }
    else
    {
        _19697 = false;
    }
    bool _19702;
    if (_19697)
    {
        _19702 = jetFailureSite == 0u;
    }
    else
    {
        _19702 = false;
    }
    if (_19702)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(45.0, 45.0, 1000.0, 1000.0);
    }
    float _17608 = _19049.lo;
    float _17609 = _19049.hi;
    float _19707 = sine_bounds(_17608, _17609, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19711 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19714 = as_type<float>(as_type<uint>(_19707) ^ 2147483648u);
    Interval _17604 = _19049;
    float _17602 = 3.1415927410125732421875;
    float _19715 = interval_down(_17602, intervalFailed);
    float _17603 = 3.1415927410125732421875;
    float _19716 = interval_up(_17603, intervalFailed);
    Interval _17605 = Interval{ _19715, _19716 };
    Interval _17606 = Interval{ 0.5, 0.5 };
    Interval _19718 = imul(_17605, _17606, intervalFailed, optical_product_upper);
    Interval _17607 = _19718;
    Interval _19719 = iadd(_17604, _17607, intervalFailed);
    float _17600 = _19719.lo;
    float _17601 = _19719.hi;
    float _19722 = sine_bounds(_17600, _17601, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17610 = Interval{ _19711, _19714 };
    Interval _17611 = _19051;
    Interval _19725 = jet_mul_derivative(_17610, _17611, intervalFailed, optical_product_upper);
    Interval _17612 = Interval{ _19711, _19714 };
    Interval _17613 = _19053;
    Interval _19727 = jet_mul_derivative(_17612, _17613, intervalFailed, optical_product_upper);
    Interval _17586 = _19692;
    Interval _17587 = Interval{ _19722, interval_sine_upper };
    Interval _19729 = imul(_17586, _17587, intervalFailed, optical_product_upper);
    Interval _17588 = Interval{ 0.0, 0.0 };
    Interval _17589 = Interval{ _19722, interval_sine_upper };
    Interval _19731 = jet_mul_derivative(_17588, _17589, intervalFailed, optical_product_upper);
    Interval _17590 = _19731;
    Interval _17591 = _19692;
    Interval _17592 = _19725;
    Interval _19732 = jet_mul_derivative(_17591, _17592, intervalFailed, optical_product_upper);
    Interval _17593 = _19732;
    Interval _19733 = jet_add_derivative(_17590, _17593, intervalFailed);
    Interval _17594 = Interval{ 0.0, 0.0 };
    Interval _17595 = Interval{ _19722, interval_sine_upper };
    Interval _19735 = jet_mul_derivative(_17594, _17595, intervalFailed, optical_product_upper);
    Interval _17596 = _19735;
    Interval _17597 = _19692;
    Interval _17598 = _19727;
    Interval _19736 = jet_mul_derivative(_17597, _17598, intervalFailed, optical_product_upper);
    Interval _17599 = _19736;
    Interval _19737 = jet_add_derivative(_17596, _17599, intervalFailed);
    Interval _17572 = _19729;
    Interval _17573 = _19368;
    Interval _19738 = imul(_17572, _17573, intervalFailed, optical_product_upper);
    Interval _17574 = _19733;
    Interval _17575 = _19368;
    Interval _19739 = jet_mul_derivative(_17574, _17575, intervalFailed, optical_product_upper);
    Interval _17576 = _19739;
    Interval _17577 = _19729;
    Interval _17578 = _19395;
    Interval _19740 = jet_mul_derivative(_17577, _17578, intervalFailed, optical_product_upper);
    Interval _17579 = _19740;
    Interval _19741 = jet_add_derivative(_17576, _17579, intervalFailed);
    Interval _17580 = _19737;
    Interval _17581 = _19368;
    Interval _19742 = jet_mul_derivative(_17580, _17581, intervalFailed, optical_product_upper);
    Interval _17582 = _19742;
    Interval _17583 = _19729;
    Interval _17584 = _19407;
    Interval _19743 = jet_mul_derivative(_17583, _17584, intervalFailed, optical_product_upper);
    Interval _17585 = _19743;
    Interval _19744 = jet_add_derivative(_17582, _17585, intervalFailed);
    Interval _17566 = _19688;
    Interval _17567 = Interval{ as_type<float>(as_type<uint>(_19738.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19738.lo) ^ 2147483648u) };
    Interval _19770 = iadd(_17566, _17567, intervalFailed);
    Interval _17568 = _19689;
    Interval _17569 = Interval{ as_type<float>(as_type<uint>(_19741.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19741.lo) ^ 2147483648u) };
    Interval _19772 = jet_add_derivative(_17568, _17569, intervalFailed);
    Interval _17570 = _19690;
    Interval _17571 = Interval{ as_type<float>(as_type<uint>(_19744.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19744.lo) ^ 2147483648u) };
    Interval _19774 = jet_add_derivative(_17570, _17571, intervalFailed);
    float _17564 = 11.0;
    float _17565 = 1000.0;
    Interval _19776 = iratio(_17564, _17565, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19781;
    if (!intervalFailed)
    {
        _19781 = intervalFailed;
    }
    else
    {
        _19781 = false;
    }
    bool _19786;
    if (_19781)
    {
        _19786 = jetFailureSite == 0u;
    }
    else
    {
        _19786 = false;
    }
    if (_19786)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
    }
    float _17562 = 52.0;
    float _17563 = 10000.0;
    Interval _19790 = iratio(_17562, _17563, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19795;
    if (!intervalFailed)
    {
        _19795 = intervalFailed;
    }
    else
    {
        _19795 = false;
    }
    bool _19800;
    if (_19795)
    {
        _19800 = jetFailureSite == 0u;
    }
    else
    {
        _19800 = false;
    }
    if (_19800)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(52.0, 52.0, 10000.0, 10000.0);
    }
    Interval _17548 = _19790;
    Interval _17549 = _19540;
    Interval _19803 = imul(_17548, _17549, intervalFailed, optical_product_upper);
    Interval _17550 = Interval{ 0.0, 0.0 };
    Interval _17551 = _19540;
    Interval _19804 = jet_mul_derivative(_17550, _17551, intervalFailed, optical_product_upper);
    Interval _17552 = _19804;
    Interval _17553 = _19790;
    Interval _17554 = _19544;
    Interval _19805 = jet_mul_derivative(_17553, _17554, intervalFailed, optical_product_upper);
    Interval _17555 = _19805;
    Interval _19806 = jet_add_derivative(_17552, _17555, intervalFailed);
    Interval _17556 = Interval{ 0.0, 0.0 };
    Interval _17557 = _19540;
    Interval _19807 = jet_mul_derivative(_17556, _17557, intervalFailed, optical_product_upper);
    Interval _17558 = _19807;
    Interval _17559 = _19790;
    Interval _17560 = _19548;
    Interval _19808 = jet_mul_derivative(_17559, _17560, intervalFailed, optical_product_upper);
    Interval _17561 = _19808;
    Interval _19809 = jet_add_derivative(_17558, _17561, intervalFailed);
    Interval _17542 = _19776;
    Interval _17543 = Interval{ as_type<float>(as_type<uint>(_19803.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19803.lo) ^ 2147483648u) };
    Interval _19835 = iadd(_17542, _17543, intervalFailed);
    Interval _17544 = Interval{ 0.0, 0.0 };
    Interval _17545 = Interval{ as_type<float>(as_type<uint>(_19806.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19806.lo) ^ 2147483648u) };
    Interval _19837 = jet_add_derivative(_17544, _17545, intervalFailed);
    Interval _17546 = Interval{ 0.0, 0.0 };
    Interval _17547 = Interval{ as_type<float>(as_type<uint>(_19809.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19809.lo) ^ 2147483648u) };
    Interval _19839 = jet_add_derivative(_17546, _17547, intervalFailed);
    Interval _17528 = Interval{ 9.0, 9.0 };
    Interval _17529 = _19835;
    Interval _19840 = imul(_17528, _17529, intervalFailed, optical_product_upper);
    Interval _17530 = Interval{ 0.0, 0.0 };
    Interval _17531 = _19835;
    Interval _19841 = jet_mul_derivative(_17530, _17531, intervalFailed, optical_product_upper);
    Interval _17532 = _19841;
    Interval _17533 = Interval{ 9.0, 9.0 };
    Interval _17534 = _19837;
    Interval _19842 = jet_mul_derivative(_17533, _17534, intervalFailed, optical_product_upper);
    Interval _17535 = _19842;
    Interval _19843 = jet_add_derivative(_17532, _17535, intervalFailed);
    Interval _17536 = Interval{ 0.0, 0.0 };
    Interval _17537 = _19835;
    Interval _19844 = jet_mul_derivative(_17536, _17537, intervalFailed, optical_product_upper);
    Interval _17538 = _19844;
    Interval _17539 = Interval{ 9.0, 9.0 };
    Interval _17540 = _19839;
    Interval _19845 = jet_mul_derivative(_17539, _17540, intervalFailed, optical_product_upper);
    Interval _17541 = _19845;
    Interval _19846 = jet_add_derivative(_17538, _17541, intervalFailed);
    float _17522 = _18808.lo;
    float _17523 = _18808.hi;
    float _19849 = sine_bounds(_17522, _17523, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19853 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19856 = as_type<float>(as_type<uint>(_19849) ^ 2147483648u);
    Interval _17518 = _18808;
    float _17516 = 3.1415927410125732421875;
    float _19857 = interval_down(_17516, intervalFailed);
    float _17517 = 3.1415927410125732421875;
    float _19858 = interval_up(_17517, intervalFailed);
    Interval _17519 = Interval{ _19857, _19858 };
    Interval _17520 = Interval{ 0.5, 0.5 };
    Interval _19860 = imul(_17519, _17520, intervalFailed, optical_product_upper);
    Interval _17521 = _19860;
    Interval _19861 = iadd(_17518, _17521, intervalFailed);
    float _17514 = _19861.lo;
    float _17515 = _19861.hi;
    float _19864 = sine_bounds(_17514, _17515, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17524 = Interval{ _19853, _19856 };
    Interval _17525 = _18809;
    Interval _19867 = jet_mul_derivative(_17524, _17525, intervalFailed, optical_product_upper);
    Interval _17526 = Interval{ _19853, _19856 };
    Interval _17527 = _18810;
    Interval _19869 = jet_mul_derivative(_17526, _17527, intervalFailed, optical_product_upper);
    Interval _17500 = _19840;
    Interval _17501 = Interval{ _19864, interval_sine_upper };
    Interval _19871 = imul(_17500, _17501, intervalFailed, optical_product_upper);
    Interval _17502 = _19843;
    Interval _17503 = Interval{ _19864, interval_sine_upper };
    Interval _19873 = jet_mul_derivative(_17502, _17503, intervalFailed, optical_product_upper);
    Interval _17504 = _19873;
    Interval _17505 = _19840;
    Interval _17506 = _19867;
    Interval _19874 = jet_mul_derivative(_17505, _17506, intervalFailed, optical_product_upper);
    Interval _17507 = _19874;
    Interval _19875 = jet_add_derivative(_17504, _17507, intervalFailed);
    Interval _17508 = _19846;
    Interval _17509 = Interval{ _19864, interval_sine_upper };
    Interval _19877 = jet_mul_derivative(_17508, _17509, intervalFailed, optical_product_upper);
    Interval _17510 = _19877;
    Interval _17511 = _19840;
    Interval _17512 = _19869;
    Interval _19878 = jet_mul_derivative(_17511, _17512, intervalFailed, optical_product_upper);
    Interval _17513 = _19878;
    Interval _19879 = jet_add_derivative(_17510, _17513, intervalFailed);
    Interval _17486 = _19871;
    Interval _17487 = _19132;
    Interval _19880 = imul(_17486, _17487, intervalFailed, optical_product_upper);
    Interval _17488 = _19875;
    Interval _17489 = _19132;
    Interval _19881 = jet_mul_derivative(_17488, _17489, intervalFailed, optical_product_upper);
    Interval _17490 = _19881;
    Interval _17491 = _19871;
    Interval _17492 = _19159;
    Interval _19882 = jet_mul_derivative(_17491, _17492, intervalFailed, optical_product_upper);
    Interval _17493 = _19882;
    Interval _19883 = jet_add_derivative(_17490, _17493, intervalFailed);
    Interval _17494 = _19879;
    Interval _17495 = _19132;
    Interval _19884 = jet_mul_derivative(_17494, _17495, intervalFailed, optical_product_upper);
    Interval _17496 = _19884;
    Interval _17497 = _19871;
    Interval _17498 = _19171;
    Interval _19885 = jet_mul_derivative(_17497, _17498, intervalFailed, optical_product_upper);
    Interval _17499 = _19885;
    Interval _19886 = jet_add_derivative(_17496, _17499, intervalFailed);
    float _17484 = 625.0;
    float _17485 = 10000.0;
    Interval _19888 = iratio(_17484, _17485, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19893;
    if (!intervalFailed)
    {
        _19893 = intervalFailed;
    }
    else
    {
        _19893 = false;
    }
    bool _19898;
    if (_19893)
    {
        _19898 = jetFailureSite == 0u;
    }
    else
    {
        _19898 = false;
    }
    if (_19898)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(625.0, 625.0, 10000.0, 10000.0);
    }
    float _17478 = _18916.lo;
    float _17479 = _18916.hi;
    float _19903 = sine_bounds(_17478, _17479, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19907 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19910 = as_type<float>(as_type<uint>(_19903) ^ 2147483648u);
    Interval _17474 = _18916;
    float _17472 = 3.1415927410125732421875;
    float _19911 = interval_down(_17472, intervalFailed);
    float _17473 = 3.1415927410125732421875;
    float _19912 = interval_up(_17473, intervalFailed);
    Interval _17475 = Interval{ _19911, _19912 };
    Interval _17476 = Interval{ 0.5, 0.5 };
    Interval _19914 = imul(_17475, _17476, intervalFailed, optical_product_upper);
    Interval _17477 = _19914;
    Interval _19915 = iadd(_17474, _17477, intervalFailed);
    float _17470 = _19915.lo;
    float _17471 = _19915.hi;
    float _19918 = sine_bounds(_17470, _17471, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17480 = Interval{ _19907, _19910 };
    Interval _17481 = _18917;
    Interval _19921 = jet_mul_derivative(_17480, _17481, intervalFailed, optical_product_upper);
    Interval _17482 = Interval{ _19907, _19910 };
    Interval _17483 = _18918;
    Interval _19923 = jet_mul_derivative(_17482, _17483, intervalFailed, optical_product_upper);
    Interval _17456 = _19888;
    Interval _17457 = Interval{ _19918, interval_sine_upper };
    Interval _19925 = imul(_17456, _17457, intervalFailed, optical_product_upper);
    Interval _17458 = Interval{ 0.0, 0.0 };
    Interval _17459 = Interval{ _19918, interval_sine_upper };
    Interval _19927 = jet_mul_derivative(_17458, _17459, intervalFailed, optical_product_upper);
    Interval _17460 = _19927;
    Interval _17461 = _19888;
    Interval _17462 = _19921;
    Interval _19928 = jet_mul_derivative(_17461, _17462, intervalFailed, optical_product_upper);
    Interval _17463 = _19928;
    Interval _19929 = jet_add_derivative(_17460, _17463, intervalFailed);
    Interval _17464 = Interval{ 0.0, 0.0 };
    Interval _17465 = Interval{ _19918, interval_sine_upper };
    Interval _19931 = jet_mul_derivative(_17464, _17465, intervalFailed, optical_product_upper);
    Interval _17466 = _19931;
    Interval _17467 = _19888;
    Interval _17468 = _19923;
    Interval _19932 = jet_mul_derivative(_17467, _17468, intervalFailed, optical_product_upper);
    Interval _17469 = _19932;
    Interval _19933 = jet_add_derivative(_17466, _17469, intervalFailed);
    Interval _17442 = _19925;
    Interval _17443 = _19250;
    Interval _19934 = imul(_17442, _17443, intervalFailed, optical_product_upper);
    Interval _17444 = _19929;
    Interval _17445 = _19250;
    Interval _19935 = jet_mul_derivative(_17444, _17445, intervalFailed, optical_product_upper);
    Interval _17446 = _19935;
    Interval _17447 = _19925;
    Interval _17448 = _19277;
    Interval _19936 = jet_mul_derivative(_17447, _17448, intervalFailed, optical_product_upper);
    Interval _17449 = _19936;
    Interval _19937 = jet_add_derivative(_17446, _17449, intervalFailed);
    Interval _17450 = _19933;
    Interval _17451 = _19250;
    Interval _19938 = jet_mul_derivative(_17450, _17451, intervalFailed, optical_product_upper);
    Interval _17452 = _19938;
    Interval _17453 = _19925;
    Interval _17454 = _19289;
    Interval _19939 = jet_mul_derivative(_17453, _17454, intervalFailed, optical_product_upper);
    Interval _17455 = _19939;
    Interval _19940 = jet_add_derivative(_17452, _17455, intervalFailed);
    Interval _17436 = _19880;
    Interval _17437 = Interval{ as_type<float>(as_type<uint>(_19934.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19934.lo) ^ 2147483648u) };
    Interval _19966 = iadd(_17436, _17437, intervalFailed);
    Interval _17438 = _19883;
    Interval _17439 = Interval{ as_type<float>(as_type<uint>(_19937.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19937.lo) ^ 2147483648u) };
    Interval _19968 = jet_add_derivative(_17438, _17439, intervalFailed);
    Interval _17440 = _19886;
    Interval _17441 = Interval{ as_type<float>(as_type<uint>(_19940.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_19940.lo) ^ 2147483648u) };
    Interval _19970 = jet_add_derivative(_17440, _17441, intervalFailed);
    float _17434 = 110.0;
    float _17435 = 1000.0;
    Interval _19972 = iratio(_17434, _17435, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _19977;
    if (!intervalFailed)
    {
        _19977 = intervalFailed;
    }
    else
    {
        _19977 = false;
    }
    bool _19982;
    if (_19977)
    {
        _19982 = jetFailureSite == 0u;
    }
    else
    {
        _19982 = false;
    }
    if (_19982)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(110.0, 110.0, 1000.0, 1000.0);
    }
    float _17428 = _19049.lo;
    float _17429 = _19049.hi;
    float _19987 = sine_bounds(_17428, _17429, intervalFailed, optical_product_upper, interval_sine_upper);
    float _19991 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _19994 = as_type<float>(as_type<uint>(_19987) ^ 2147483648u);
    Interval _17424 = _19049;
    float _17422 = 3.1415927410125732421875;
    float _19995 = interval_down(_17422, intervalFailed);
    float _17423 = 3.1415927410125732421875;
    float _19996 = interval_up(_17423, intervalFailed);
    Interval _17425 = Interval{ _19995, _19996 };
    Interval _17426 = Interval{ 0.5, 0.5 };
    Interval _19998 = imul(_17425, _17426, intervalFailed, optical_product_upper);
    Interval _17427 = _19998;
    Interval _19999 = iadd(_17424, _17427, intervalFailed);
    float _17420 = _19999.lo;
    float _17421 = _19999.hi;
    float _20002 = sine_bounds(_17420, _17421, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17430 = Interval{ _19991, _19994 };
    Interval _17431 = _19051;
    Interval _20005 = jet_mul_derivative(_17430, _17431, intervalFailed, optical_product_upper);
    Interval _17432 = Interval{ _19991, _19994 };
    Interval _17433 = _19053;
    Interval _20007 = jet_mul_derivative(_17432, _17433, intervalFailed, optical_product_upper);
    Interval _17406 = _19972;
    Interval _17407 = Interval{ _20002, interval_sine_upper };
    Interval _20009 = imul(_17406, _17407, intervalFailed, optical_product_upper);
    Interval _17408 = Interval{ 0.0, 0.0 };
    Interval _17409 = Interval{ _20002, interval_sine_upper };
    Interval _20011 = jet_mul_derivative(_17408, _17409, intervalFailed, optical_product_upper);
    Interval _17410 = _20011;
    Interval _17411 = _19972;
    Interval _17412 = _20005;
    Interval _20012 = jet_mul_derivative(_17411, _17412, intervalFailed, optical_product_upper);
    Interval _17413 = _20012;
    Interval _20013 = jet_add_derivative(_17410, _17413, intervalFailed);
    Interval _17414 = Interval{ 0.0, 0.0 };
    Interval _17415 = Interval{ _20002, interval_sine_upper };
    Interval _20015 = jet_mul_derivative(_17414, _17415, intervalFailed, optical_product_upper);
    Interval _17416 = _20015;
    Interval _17417 = _19972;
    Interval _17418 = _20007;
    Interval _20016 = jet_mul_derivative(_17417, _17418, intervalFailed, optical_product_upper);
    Interval _17419 = _20016;
    Interval _20017 = jet_add_derivative(_17416, _17419, intervalFailed);
    Interval _17392 = _20009;
    Interval _17393 = _19368;
    Interval _20018 = imul(_17392, _17393, intervalFailed, optical_product_upper);
    Interval _17394 = _20013;
    Interval _17395 = _19368;
    Interval _20019 = jet_mul_derivative(_17394, _17395, intervalFailed, optical_product_upper);
    Interval _17396 = _20019;
    Interval _17397 = _20009;
    Interval _17398 = _19395;
    Interval _20020 = jet_mul_derivative(_17397, _17398, intervalFailed, optical_product_upper);
    Interval _17399 = _20020;
    Interval _20021 = jet_add_derivative(_17396, _17399, intervalFailed);
    Interval _17400 = _20017;
    Interval _17401 = _19368;
    Interval _20022 = jet_mul_derivative(_17400, _17401, intervalFailed, optical_product_upper);
    Interval _17402 = _20022;
    Interval _17403 = _20009;
    Interval _17404 = _19407;
    Interval _20023 = jet_mul_derivative(_17403, _17404, intervalFailed, optical_product_upper);
    Interval _17405 = _20023;
    Interval _20024 = jet_add_derivative(_17402, _17405, intervalFailed);
    Interval _17386 = _19966;
    Interval _17387 = _20018;
    Interval _20025 = iadd(_17386, _17387, intervalFailed);
    Interval _17388 = _19968;
    Interval _17389 = _20021;
    Interval _20026 = jet_add_derivative(_17388, _17389, intervalFailed);
    Interval _17390 = _19970;
    Interval _17391 = _20024;
    Interval _20027 = jet_add_derivative(_17390, _17391, intervalFailed);
    float _17384 = 173.0;
    float _17385 = 1000.0;
    Interval _20033 = iratio(_17384, _17385, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20038;
    if (!intervalFailed)
    {
        _20038 = intervalFailed;
    }
    else
    {
        _20038 = false;
    }
    bool _20043;
    if (_20038)
    {
        _20043 = jetFailureSite == 0u;
    }
    else
    {
        _20043 = false;
    }
    if (_20043)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(173.0, 173.0, 1000.0, 1000.0);
    }
    Interval _17370 = x.v;
    Interval _17371 = _20033;
    Interval _20046 = imul(_17370, _17371, intervalFailed, optical_product_upper);
    Interval _17372 = x.dx;
    Interval _17373 = _20033;
    Interval _20047 = jet_mul_derivative(_17372, _17373, intervalFailed, optical_product_upper);
    Interval _17374 = _20047;
    Interval _17375 = x.v;
    Interval _17376 = Interval{ 0.0, 0.0 };
    Interval _20048 = jet_mul_derivative(_17375, _17376, intervalFailed, optical_product_upper);
    Interval _17377 = _20048;
    Interval _20049 = jet_add_derivative(_17374, _17377, intervalFailed);
    Interval _17378 = x.dy;
    Interval _17379 = _20033;
    Interval _20050 = jet_mul_derivative(_17378, _17379, intervalFailed, optical_product_upper);
    Interval _17380 = _20050;
    Interval _17381 = x.v;
    Interval _17382 = Interval{ 0.0, 0.0 };
    Interval _20051 = jet_mul_derivative(_17381, _17382, intervalFailed, optical_product_upper);
    Interval _17383 = _20051;
    Interval _20052 = jet_add_derivative(_17380, _17383, intervalFailed);
    float _17368 = 129.0;
    float _17369 = 1000.0;
    Interval _20058 = iratio(_17368, _17369, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20063;
    if (!intervalFailed)
    {
        _20063 = intervalFailed;
    }
    else
    {
        _20063 = false;
    }
    bool _20068;
    if (_20063)
    {
        _20068 = jetFailureSite == 0u;
    }
    else
    {
        _20068 = false;
    }
    if (_20068)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(129.0, 129.0, 1000.0, 1000.0);
    }
    Interval _17354 = z.v;
    Interval _17355 = _20058;
    Interval _20071 = imul(_17354, _17355, intervalFailed, optical_product_upper);
    Interval _17356 = z.dx;
    Interval _17357 = _20058;
    Interval _20072 = jet_mul_derivative(_17356, _17357, intervalFailed, optical_product_upper);
    Interval _17358 = _20072;
    Interval _17359 = z.v;
    Interval _17360 = Interval{ 0.0, 0.0 };
    Interval _20073 = jet_mul_derivative(_17359, _17360, intervalFailed, optical_product_upper);
    Interval _17361 = _20073;
    Interval _20074 = jet_add_derivative(_17358, _17361, intervalFailed);
    Interval _17362 = z.dy;
    Interval _17363 = _20058;
    Interval _20075 = jet_mul_derivative(_17362, _17363, intervalFailed, optical_product_upper);
    Interval _17364 = _20075;
    Interval _17365 = z.v;
    Interval _17366 = Interval{ 0.0, 0.0 };
    Interval _20076 = jet_mul_derivative(_17365, _17366, intervalFailed, optical_product_upper);
    Interval _17367 = _20076;
    Interval _20077 = jet_add_derivative(_17364, _17367, intervalFailed);
    Interval _17348 = _20046;
    Interval _17349 = _20071;
    Interval _20078 = iadd(_17348, _17349, intervalFailed);
    Interval _17350 = _20049;
    Interval _17351 = _20074;
    Interval _20079 = jet_add_derivative(_17350, _17351, intervalFailed);
    Interval _17352 = _20052;
    Interval _17353 = _20077;
    Interval _20080 = jet_add_derivative(_17352, _17353, intervalFailed);
    float _17346 = 73.0;
    float _17347 = 100.0;
    Interval _20086 = iratio(_17346, _17347, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20091;
    if (!intervalFailed)
    {
        _20091 = intervalFailed;
    }
    else
    {
        _20091 = false;
    }
    bool _20096;
    if (_20091)
    {
        _20096 = jetFailureSite == 0u;
    }
    else
    {
        _20096 = false;
    }
    if (_20096)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(73.0, 73.0, 100.0, 100.0);
    }
    Interval _17332 = t.v;
    Interval _17333 = _20086;
    Interval _20099 = imul(_17332, _17333, intervalFailed, optical_product_upper);
    Interval _17334 = t.dx;
    Interval _17335 = _20086;
    Interval _20100 = jet_mul_derivative(_17334, _17335, intervalFailed, optical_product_upper);
    Interval _17336 = _20100;
    Interval _17337 = t.v;
    Interval _17338 = Interval{ 0.0, 0.0 };
    Interval _20101 = jet_mul_derivative(_17337, _17338, intervalFailed, optical_product_upper);
    Interval _17339 = _20101;
    Interval _20102 = jet_add_derivative(_17336, _17339, intervalFailed);
    Interval _17340 = t.dy;
    Interval _17341 = _20086;
    Interval _20103 = jet_mul_derivative(_17340, _17341, intervalFailed, optical_product_upper);
    Interval _17342 = _20103;
    Interval _17343 = t.v;
    Interval _17344 = Interval{ 0.0, 0.0 };
    Interval _20104 = jet_mul_derivative(_17343, _17344, intervalFailed, optical_product_upper);
    Interval _17345 = _20104;
    Interval _20105 = jet_add_derivative(_17342, _17345, intervalFailed);
    Interval _17326 = _20078;
    Interval _17327 = Interval{ as_type<float>(as_type<uint>(_20099.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20099.lo) ^ 2147483648u) };
    Interval _20131 = iadd(_17326, _17327, intervalFailed);
    Interval _17328 = _20079;
    Interval _17329 = Interval{ as_type<float>(as_type<uint>(_20102.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20102.lo) ^ 2147483648u) };
    Interval _20133 = jet_add_derivative(_17328, _17329, intervalFailed);
    Interval _17330 = _20080;
    Interval _17331 = Interval{ as_type<float>(as_type<uint>(_20105.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20105.lo) ^ 2147483648u) };
    Interval _20135 = jet_add_derivative(_17330, _17331, intervalFailed);
    float _17324 = 216.0;
    float _17325 = 1000.0;
    Interval _20138 = iratio(_17324, _17325, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20143;
    if (!intervalFailed)
    {
        _20143 = intervalFailed;
    }
    else
    {
        _20143 = false;
    }
    bool _20148;
    if (_20143)
    {
        _20148 = jetFailureSite == 0u;
    }
    else
    {
        _20148 = false;
    }
    if (_20148)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(216.0, 216.0, 1000.0, 1000.0);
    }
    Interval _17310 = footprint.v;
    Interval _17311 = _20138;
    Interval _20154 = imul(_17310, _17311, intervalFailed, optical_product_upper);
    Interval _17312 = footprint.dx;
    Interval _17313 = _20138;
    Interval _20155 = jet_mul_derivative(_17312, _17313, intervalFailed, optical_product_upper);
    Interval _17314 = _20155;
    Interval _17315 = footprint.v;
    Interval _17316 = Interval{ 0.0, 0.0 };
    Interval _20156 = jet_mul_derivative(_17315, _17316, intervalFailed, optical_product_upper);
    Interval _17317 = _20156;
    Interval _20157 = jet_add_derivative(_17314, _17317, intervalFailed);
    Interval _17318 = footprint.dy;
    Interval _17319 = _20138;
    Interval _20158 = jet_mul_derivative(_17318, _17319, intervalFailed, optical_product_upper);
    Interval _17320 = _20158;
    Interval _17321 = footprint.v;
    Interval _17322 = Interval{ 0.0, 0.0 };
    Interval _20159 = jet_mul_derivative(_17321, _17322, intervalFailed, optical_product_upper);
    Interval _17323 = _20159;
    Interval _20160 = jet_add_derivative(_17320, _17323, intervalFailed);
    bool _20167;
    if (_20154.lo <= 0.0)
    {
        _20167 = _20154.hi >= 0.0;
    }
    else
    {
        _20167 = false;
    }
    float _20174;
    if (_20167)
    {
        _20174 = 0.0;
    }
    else
    {
        _20174 = precise::min(abs(_20154.lo), abs(_20154.hi));
    }
    float _20177 = precise::max(abs(_20154.lo), abs(_20154.hi));
    float _17300 = spvFMul(_20174, _20174);
    float _20178 = interval_down(_17300, intervalFailed);
    float _20179 = precise::max(0.0, _20178);
    float _17301 = spvFMul(_20177, _20177);
    float _20180 = interval_up(_17301, intervalFailed);
    Interval _17302 = Interval{ 2.0, 2.0 };
    Interval _17303 = _20154;
    Interval _20181 = imul(_17302, _17303, intervalFailed, optical_product_upper);
    Interval _17304 = _20181;
    Interval _17305 = _20157;
    Interval _20182 = jet_mul_derivative(_17304, _17305, intervalFailed, optical_product_upper);
    Interval _17306 = Interval{ 2.0, 2.0 };
    Interval _17307 = _20154;
    Interval _20183 = imul(_17306, _17307, intervalFailed, optical_product_upper);
    Interval _17308 = _20183;
    Interval _17309 = _20160;
    Interval _20184 = jet_mul_derivative(_17308, _17309, intervalFailed, optical_product_upper);
    bool _20189;
    if (_20179 <= 0.0)
    {
        _20189 = _20180 >= 0.0;
    }
    else
    {
        _20189 = false;
    }
    float _20196;
    if (_20189)
    {
        _20196 = 0.0;
    }
    else
    {
        _20196 = precise::min(abs(_20179), abs(_20180));
    }
    float _20199 = precise::max(abs(_20179), abs(_20180));
    float _17290 = spvFMul(_20196, _20196);
    float _20200 = interval_down(_17290, intervalFailed);
    float _17291 = spvFMul(_20199, _20199);
    float _20202 = interval_up(_17291, intervalFailed);
    Interval _17292 = Interval{ 2.0, 2.0 };
    Interval _17293 = Interval{ _20179, _20180 };
    Interval _20204 = imul(_17292, _17293, intervalFailed, optical_product_upper);
    Interval _17294 = _20204;
    Interval _17295 = _20182;
    Interval _20205 = jet_mul_derivative(_17294, _17295, intervalFailed, optical_product_upper);
    Interval _17296 = Interval{ 2.0, 2.0 };
    Interval _17297 = Interval{ _20179, _20180 };
    Interval _20207 = imul(_17296, _17297, intervalFailed, optical_product_upper);
    Interval _17298 = _20207;
    Interval _17299 = _20184;
    Interval _20208 = jet_mul_derivative(_17298, _17299, intervalFailed, optical_product_upper);
    Interval _17284 = Interval{ 1.0, 1.0 };
    Interval _17285 = Interval{ precise::max(0.0, _20200), _20202 };
    Interval _20210 = iadd(_17284, _17285, intervalFailed);
    Interval _17286 = Interval{ 0.0, 0.0 };
    Interval _17287 = _20205;
    Interval _20211 = jet_add_derivative(_17286, _17287, intervalFailed);
    Interval _17288 = Interval{ 0.0, 0.0 };
    Interval _17289 = _20208;
    Interval _20212 = jet_add_derivative(_17288, _17289, intervalFailed);
    Interval _17270 = Interval{ 1.0, 1.0 };
    Interval _17271 = _20210;
    Interval _20214 = idiv(_17270, _17271, intervalFailed, interval_divide_upper);
    bool _20221;
    if (!intervalFailed)
    {
        _20221 = intervalFailed;
    }
    else
    {
        _20221 = false;
    }
    bool _20226;
    if (_20221)
    {
        _20226 = jetFailureSite == 0u;
    }
    else
    {
        _20226 = false;
    }
    if (_20226)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _20210.lo, _20210.hi);
    }
    Interval _17272 = Interval{ 0.0, 0.0 };
    Interval _17273 = _20214;
    Interval _17274 = _20211;
    Interval _20230 = jet_mul_derivative(_17273, _17274, intervalFailed, optical_product_upper);
    Interval _17275 = Interval{ as_type<float>(as_type<uint>(_20230.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20230.lo) ^ 2147483648u) };
    Interval _20240 = jet_add_derivative(_17272, _17275, intervalFailed);
    Interval _17276 = _20240;
    Interval _17277 = _20210;
    Interval _20241 = jet_div_derivative(_17276, _17277, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _17278 = Interval{ 0.0, 0.0 };
    Interval _17279 = _20214;
    Interval _17280 = _20212;
    Interval _20242 = jet_mul_derivative(_17279, _17280, intervalFailed, optical_product_upper);
    Interval _17281 = Interval{ as_type<float>(as_type<uint>(_20242.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20242.lo) ^ 2147483648u) };
    Interval _20252 = jet_add_derivative(_17278, _17281, intervalFailed);
    Interval _17282 = _20252;
    Interval _17283 = _20210;
    Interval _20253 = jet_div_derivative(_17282, _17283, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    float _17268 = 38.0;
    float _17269 = 100.0;
    Interval _20255 = iratio(_17268, _17269, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20260;
    if (!intervalFailed)
    {
        _20260 = intervalFailed;
    }
    else
    {
        _20260 = false;
    }
    bool _20265;
    if (_20260)
    {
        _20265 = jetFailureSite == 0u;
    }
    else
    {
        _20265 = false;
    }
    if (_20265)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(38.0, 38.0, 100.0, 100.0);
    }
    Interval _17260 = _20131;
    float _17258 = 3.1415927410125732421875;
    float _20268 = interval_down(_17258, intervalFailed);
    float _17259 = 3.1415927410125732421875;
    float _20269 = interval_up(_17259, intervalFailed);
    Interval _17261 = Interval{ _20268, _20269 };
    Interval _17262 = Interval{ 0.5, 0.5 };
    Interval _20271 = imul(_17261, _17262, intervalFailed, optical_product_upper);
    Interval _17263 = _20271;
    Interval _20272 = iadd(_17260, _17263, intervalFailed);
    float _17256 = _20272.lo;
    float _17257 = _20272.hi;
    float _20275 = sine_bounds(_17256, _17257, intervalFailed, optical_product_upper, interval_sine_upper);
    float _17254 = _20131.lo;
    float _17255 = _20131.hi;
    float _20279 = sine_bounds(_17254, _17255, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17264 = Interval{ _20275, interval_sine_upper };
    Interval _17265 = _20133;
    Interval _20282 = jet_mul_derivative(_17264, _17265, intervalFailed, optical_product_upper);
    Interval _17266 = Interval{ _20275, interval_sine_upper };
    Interval _17267 = _20135;
    Interval _20284 = jet_mul_derivative(_17266, _17267, intervalFailed, optical_product_upper);
    Interval _17240 = _20255;
    Interval _17241 = Interval{ _20279, interval_sine_upper };
    Interval _20286 = imul(_17240, _17241, intervalFailed, optical_product_upper);
    Interval _17242 = Interval{ 0.0, 0.0 };
    Interval _17243 = Interval{ _20279, interval_sine_upper };
    Interval _20288 = jet_mul_derivative(_17242, _17243, intervalFailed, optical_product_upper);
    Interval _17244 = _20288;
    Interval _17245 = _20255;
    Interval _17246 = _20282;
    Interval _20289 = jet_mul_derivative(_17245, _17246, intervalFailed, optical_product_upper);
    Interval _17247 = _20289;
    Interval _20290 = jet_add_derivative(_17244, _17247, intervalFailed);
    Interval _17248 = Interval{ 0.0, 0.0 };
    Interval _17249 = Interval{ _20279, interval_sine_upper };
    Interval _20292 = jet_mul_derivative(_17248, _17249, intervalFailed, optical_product_upper);
    Interval _17250 = _20292;
    Interval _17251 = _20255;
    Interval _17252 = _20284;
    Interval _20293 = jet_mul_derivative(_17251, _17252, intervalFailed, optical_product_upper);
    Interval _17253 = _20293;
    Interval _20294 = jet_add_derivative(_17250, _17253, intervalFailed);
    Interval _17226 = _20286;
    Interval _17227 = _20214;
    Interval _20295 = imul(_17226, _17227, intervalFailed, optical_product_upper);
    Interval _17228 = _20290;
    Interval _17229 = _20214;
    Interval _20296 = jet_mul_derivative(_17228, _17229, intervalFailed, optical_product_upper);
    Interval _17230 = _20296;
    Interval _17231 = _20286;
    Interval _17232 = _20241;
    Interval _20297 = jet_mul_derivative(_17231, _17232, intervalFailed, optical_product_upper);
    Interval _17233 = _20297;
    Interval _20298 = jet_add_derivative(_17230, _17233, intervalFailed);
    Interval _17234 = _20294;
    Interval _17235 = _20214;
    Interval _20299 = jet_mul_derivative(_17234, _17235, intervalFailed, optical_product_upper);
    Interval _17236 = _20299;
    Interval _17237 = _20286;
    Interval _17238 = _20253;
    Interval _20300 = jet_mul_derivative(_17237, _17238, intervalFailed, optical_product_upper);
    Interval _17239 = _20300;
    Interval _20301 = jet_add_derivative(_17236, _17239, intervalFailed);
    Interval _17220 = _19513;
    Interval _17221 = _20295;
    Interval _20302 = iadd(_17220, _17221, intervalFailed);
    Interval _17222 = _19514;
    Interval _17223 = _20298;
    Interval _20303 = jet_add_derivative(_17222, _17223, intervalFailed);
    Interval _17224 = _19515;
    Interval _17225 = _20301;
    Interval _20304 = jet_add_derivative(_17224, _17225, intervalFailed);
    float _17218 = 6574.0;
    float _17219 = 100000.0;
    Interval _20312 = iratio(_17218, _17219, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20317;
    if (!intervalFailed)
    {
        _20317 = intervalFailed;
    }
    else
    {
        _20317 = false;
    }
    bool _20322;
    if (_20317)
    {
        _20322 = jetFailureSite == 0u;
    }
    else
    {
        _20322 = false;
    }
    if (_20322)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(6574.0, 6574.0, 100000.0, 100000.0);
    }
    float _17212 = _20131.lo;
    float _17213 = _20131.hi;
    float _20327 = sine_bounds(_17212, _17213, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20331 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20334 = as_type<float>(as_type<uint>(_20327) ^ 2147483648u);
    Interval _17208 = _20131;
    float _17206 = 3.1415927410125732421875;
    float _20335 = interval_down(_17206, intervalFailed);
    float _17207 = 3.1415927410125732421875;
    float _20336 = interval_up(_17207, intervalFailed);
    Interval _17209 = Interval{ _20335, _20336 };
    Interval _17210 = Interval{ 0.5, 0.5 };
    Interval _20338 = imul(_17209, _17210, intervalFailed, optical_product_upper);
    Interval _17211 = _20338;
    Interval _20339 = iadd(_17208, _17211, intervalFailed);
    float _17204 = _20339.lo;
    float _17205 = _20339.hi;
    float _20342 = sine_bounds(_17204, _17205, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17214 = Interval{ _20331, _20334 };
    Interval _17215 = _20133;
    Interval _20345 = jet_mul_derivative(_17214, _17215, intervalFailed, optical_product_upper);
    Interval _17216 = Interval{ _20331, _20334 };
    Interval _17217 = _20135;
    Interval _20347 = jet_mul_derivative(_17216, _17217, intervalFailed, optical_product_upper);
    Interval _17190 = _20312;
    Interval _17191 = Interval{ _20342, interval_sine_upper };
    Interval _20349 = imul(_17190, _17191, intervalFailed, optical_product_upper);
    Interval _17192 = Interval{ 0.0, 0.0 };
    Interval _17193 = Interval{ _20342, interval_sine_upper };
    Interval _20351 = jet_mul_derivative(_17192, _17193, intervalFailed, optical_product_upper);
    Interval _17194 = _20351;
    Interval _17195 = _20312;
    Interval _17196 = _20345;
    Interval _20352 = jet_mul_derivative(_17195, _17196, intervalFailed, optical_product_upper);
    Interval _17197 = _20352;
    Interval _20353 = jet_add_derivative(_17194, _17197, intervalFailed);
    Interval _17198 = Interval{ 0.0, 0.0 };
    Interval _17199 = Interval{ _20342, interval_sine_upper };
    Interval _20355 = jet_mul_derivative(_17198, _17199, intervalFailed, optical_product_upper);
    Interval _17200 = _20355;
    Interval _17201 = _20312;
    Interval _17202 = _20347;
    Interval _20356 = jet_mul_derivative(_17201, _17202, intervalFailed, optical_product_upper);
    Interval _17203 = _20356;
    Interval _20357 = jet_add_derivative(_17200, _17203, intervalFailed);
    Interval _17176 = _20349;
    Interval _17177 = _20214;
    Interval _20358 = imul(_17176, _17177, intervalFailed, optical_product_upper);
    Interval _17178 = _20353;
    Interval _17179 = _20214;
    Interval _20359 = jet_mul_derivative(_17178, _17179, intervalFailed, optical_product_upper);
    Interval _17180 = _20359;
    Interval _17181 = _20349;
    Interval _17182 = _20241;
    Interval _20360 = jet_mul_derivative(_17181, _17182, intervalFailed, optical_product_upper);
    Interval _17183 = _20360;
    Interval _20361 = jet_add_derivative(_17180, _17183, intervalFailed);
    Interval _17184 = _20357;
    Interval _17185 = _20214;
    Interval _20362 = jet_mul_derivative(_17184, _17185, intervalFailed, optical_product_upper);
    Interval _17186 = _20362;
    Interval _17187 = _20349;
    Interval _17188 = _20253;
    Interval _20363 = jet_mul_derivative(_17187, _17188, intervalFailed, optical_product_upper);
    Interval _17189 = _20363;
    Interval _20364 = jet_add_derivative(_17186, _17189, intervalFailed);
    Interval _17170 = _19770;
    Interval _17171 = _20358;
    Interval _20365 = iadd(_17170, _17171, intervalFailed);
    Interval _17172 = _19772;
    Interval _17173 = _20361;
    Interval _20366 = jet_add_derivative(_17172, _17173, intervalFailed);
    Interval _17174 = _19774;
    Interval _17175 = _20364;
    Interval _20367 = jet_add_derivative(_17174, _17175, intervalFailed);
    float _17168 = 4902.0;
    float _17169 = 100000.0;
    Interval _20375 = iratio(_17168, _17169, intervalFailed, optical_product_upper, interval_divide_upper);
    bool _20380;
    if (!intervalFailed)
    {
        _20380 = intervalFailed;
    }
    else
    {
        _20380 = false;
    }
    bool _20385;
    if (_20380)
    {
        _20385 = jetFailureSite == 0u;
    }
    else
    {
        _20385 = false;
    }
    if (_20385)
    {
        jetFailureSite = 6u;
        jetFailureArguments = float4(4902.0, 4902.0, 100000.0, 100000.0);
    }
    float _17162 = _20131.lo;
    float _17163 = _20131.hi;
    float _20390 = sine_bounds(_17162, _17163, intervalFailed, optical_product_upper, interval_sine_upper);
    float _20394 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
    float _20397 = as_type<float>(as_type<uint>(_20390) ^ 2147483648u);
    Interval _17158 = _20131;
    float _17156 = 3.1415927410125732421875;
    float _20398 = interval_down(_17156, intervalFailed);
    float _17157 = 3.1415927410125732421875;
    float _20399 = interval_up(_17157, intervalFailed);
    Interval _17159 = Interval{ _20398, _20399 };
    Interval _17160 = Interval{ 0.5, 0.5 };
    Interval _20401 = imul(_17159, _17160, intervalFailed, optical_product_upper);
    Interval _17161 = _20401;
    Interval _20402 = iadd(_17158, _17161, intervalFailed);
    float _17154 = _20402.lo;
    float _17155 = _20402.hi;
    float _20405 = sine_bounds(_17154, _17155, intervalFailed, optical_product_upper, interval_sine_upper);
    Interval _17164 = Interval{ _20394, _20397 };
    Interval _17165 = _20133;
    Interval _20408 = jet_mul_derivative(_17164, _17165, intervalFailed, optical_product_upper);
    Interval _17166 = Interval{ _20394, _20397 };
    Interval _17167 = _20135;
    Interval _20410 = jet_mul_derivative(_17166, _17167, intervalFailed, optical_product_upper);
    Interval _17140 = _20375;
    Interval _17141 = Interval{ _20405, interval_sine_upper };
    Interval _20412 = imul(_17140, _17141, intervalFailed, optical_product_upper);
    Interval _17142 = Interval{ 0.0, 0.0 };
    Interval _17143 = Interval{ _20405, interval_sine_upper };
    Interval _20414 = jet_mul_derivative(_17142, _17143, intervalFailed, optical_product_upper);
    Interval _17144 = _20414;
    Interval _17145 = _20375;
    Interval _17146 = _20408;
    Interval _20415 = jet_mul_derivative(_17145, _17146, intervalFailed, optical_product_upper);
    Interval _17147 = _20415;
    Interval _20416 = jet_add_derivative(_17144, _17147, intervalFailed);
    Interval _17148 = Interval{ 0.0, 0.0 };
    Interval _17149 = Interval{ _20405, interval_sine_upper };
    Interval _20418 = jet_mul_derivative(_17148, _17149, intervalFailed, optical_product_upper);
    Interval _17150 = _20418;
    Interval _17151 = _20375;
    Interval _17152 = _20410;
    Interval _20419 = jet_mul_derivative(_17151, _17152, intervalFailed, optical_product_upper);
    Interval _17153 = _20419;
    Interval _20420 = jet_add_derivative(_17150, _17153, intervalFailed);
    Interval _17126 = _20412;
    Interval _17127 = _20214;
    Interval _20421 = imul(_17126, _17127, intervalFailed, optical_product_upper);
    Interval _17128 = _20416;
    Interval _17129 = _20214;
    Interval _20422 = jet_mul_derivative(_17128, _17129, intervalFailed, optical_product_upper);
    Interval _17130 = _20422;
    Interval _17131 = _20412;
    Interval _17132 = _20241;
    Interval _20423 = jet_mul_derivative(_17131, _17132, intervalFailed, optical_product_upper);
    Interval _17133 = _20423;
    Interval _20424 = jet_add_derivative(_17130, _17133, intervalFailed);
    Interval _17134 = _20420;
    Interval _17135 = _20214;
    Interval _20425 = jet_mul_derivative(_17134, _17135, intervalFailed, optical_product_upper);
    Interval _17136 = _20425;
    Interval _17137 = _20412;
    Interval _17138 = _20253;
    Interval _20426 = jet_mul_derivative(_17137, _17138, intervalFailed, optical_product_upper);
    Interval _17139 = _20426;
    Interval _20427 = jet_add_derivative(_17136, _17139, intervalFailed);
    Interval _17120 = _20025;
    Interval _17121 = _20421;
    Interval _20428 = iadd(_17120, _17121, intervalFailed);
    Interval _17122 = _20026;
    Interval _17123 = _20424;
    Interval _20429 = jet_add_derivative(_17122, _17123, intervalFailed);
    Interval _17124 = _20027;
    Interval _17125 = _20427;
    Interval _20430 = jet_add_derivative(_17124, _17125, intervalFailed);
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
    float _21448;
    float _21449;
    float _21450;
    float _21451;
    float _21452;
    float _21453;
    if (footprint.v.lo < 24.0)
    {
        if (footprint.v.hi >= 24.0)
        {
            jetBranchKnown = false;
            return OpticalJet3{ OpticalJet{ _20302, _20303, _20304 }, OpticalJet{ _20365, _20366, _20367 }, OpticalJet{ _20428, _20429, _20430 } };
        }
        Interval _17106 = x.v;
        Interval _17107 = Interval{ 128.0, 128.0 };
        Interval _20452 = idiv(_17106, _17107, intervalFailed, interval_divide_upper);
        bool _20459;
        if (!intervalFailed)
        {
            _20459 = intervalFailed;
        }
        else
        {
            _20459 = false;
        }
        bool _20464;
        if (_20459)
        {
            _20464 = jetFailureSite == 0u;
        }
        else
        {
            _20464 = false;
        }
        if (_20464)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(x.v.lo, x.v.hi, 128.0, 128.0);
        }
        Interval _17108 = x.dx;
        Interval _17109 = _20452;
        Interval _17110 = Interval{ 0.0, 0.0 };
        Interval _20468 = jet_mul_derivative(_17109, _17110, intervalFailed, optical_product_upper);
        Interval _17111 = Interval{ as_type<float>(as_type<uint>(_20468.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20468.lo) ^ 2147483648u) };
        Interval _20478 = jet_add_derivative(_17108, _17111, intervalFailed);
        Interval _17112 = _20478;
        Interval _17113 = Interval{ 128.0, 128.0 };
        Interval _20479 = jet_div_derivative(_17112, _17113, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17114 = x.dy;
        Interval _17115 = _20452;
        Interval _17116 = Interval{ 0.0, 0.0 };
        Interval _20480 = jet_mul_derivative(_17115, _17116, intervalFailed, optical_product_upper);
        Interval _17117 = Interval{ as_type<float>(as_type<uint>(_20480.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20480.lo) ^ 2147483648u) };
        Interval _20490 = jet_add_derivative(_17114, _17117, intervalFailed);
        Interval _17118 = _20490;
        Interval _17119 = Interval{ 128.0, 128.0 };
        Interval _20491 = jet_div_derivative(_17118, _17119, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17092 = z.v;
        Interval _17093 = Interval{ 128.0, 128.0 };
        Interval _20499 = idiv(_17092, _17093, intervalFailed, interval_divide_upper);
        bool _20506;
        if (!intervalFailed)
        {
            _20506 = intervalFailed;
        }
        else
        {
            _20506 = false;
        }
        bool _20511;
        if (_20506)
        {
            _20511 = jetFailureSite == 0u;
        }
        else
        {
            _20511 = false;
        }
        if (_20511)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(z.v.lo, z.v.hi, 128.0, 128.0);
        }
        Interval _17094 = z.dx;
        Interval _17095 = _20499;
        Interval _17096 = Interval{ 0.0, 0.0 };
        Interval _20515 = jet_mul_derivative(_17095, _17096, intervalFailed, optical_product_upper);
        Interval _17097 = Interval{ as_type<float>(as_type<uint>(_20515.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20515.lo) ^ 2147483648u) };
        Interval _20525 = jet_add_derivative(_17094, _17097, intervalFailed);
        Interval _17098 = _20525;
        Interval _17099 = Interval{ 128.0, 128.0 };
        Interval _20526 = jet_div_derivative(_17098, _17099, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _17100 = z.dy;
        Interval _17101 = _20499;
        Interval _17102 = Interval{ 0.0, 0.0 };
        Interval _20527 = jet_mul_derivative(_17101, _17102, intervalFailed, optical_product_upper);
        Interval _17103 = Interval{ as_type<float>(as_type<uint>(_20527.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20527.lo) ^ 2147483648u) };
        Interval _20537 = jet_add_derivative(_17100, _17103, intervalFailed);
        Interval _17104 = _20537;
        Interval _17105 = Interval{ 128.0, 128.0 };
        Interval _20538 = jet_div_derivative(_17104, _17105, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        float _20541 = floor(_20452.lo);
        float _20542 = floor(_20452.hi);
        bool _20547;
        if ((isunordered(_20541, _20542) || _20541 == _20542))
        {
            _20547 = floor(_20499.lo) != floor(_20499.hi);
        }
        else
        {
            _20547 = true;
        }
        float _21418;
        float _21419;
        float _21420;
        float _21421;
        float _21422;
        float _21423;
        float _21424;
        float _21425;
        float _21426;
        float _21427;
        float _21428;
        float _21429;
        float _21430;
        float _21431;
        float _21432;
        float _21433;
        float _21434;
        float _21435;
        if (_20547)
        {
            jetBranchKnown = false;
            return OpticalJet3{ OpticalJet{ _20302, _20303, _20304 }, OpticalJet{ _20365, _20366, _20367 }, OpticalJet{ _20428, _20429, _20430 } };
        }
        else
        {
            int _20553 = int(floor(_20452.lo));
            int _20555 = int(floor(_20499.lo));
            uint _20559 = (uint(_20553) * 1597334677u) ^ (uint(_20555) * 3812015801u);
            uint _1917 = (_20559 ^ (_20559 >> 16u)) * 2246822519u;
            float _17090 = float((_1917 ^ (_1917 >> 13u)) & 65535u);
            float _17091 = 65535.0;
            Interval _20566 = iratio(_17090, _17091, intervalFailed, optical_product_upper, interval_divide_upper);
            float _20567 = float(_20553);
            float _20568 = float(_20555);
            bool _20573;
            if (!intervalFailed)
            {
                _20573 = intervalFailed;
            }
            else
            {
                _20573 = false;
            }
            bool _20578;
            if (_20573)
            {
                _20578 = jetFailureSite == 0u;
            }
            else
            {
                _20578 = false;
            }
            if (_20578)
            {
                jetFailureSite = 7u;
                jetFailureArguments = float4(_20567, _20567, _20568, _20568);
            }
            float param_var_n = 64.0;
            float param_var_d = 100.0;
            Interval _20584 = iratio(param_var_n, param_var_d, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _20589;
            if (_20566.lo <= _20584.hi)
            {
                _20589 = _20566.hi > _20584.lo;
            }
            else
            {
                _20589 = false;
            }
            if (_20589)
            {
                jetBranchKnown = false;
                return OpticalJet3{ OpticalJet{ _20302, _20303, _20304 }, OpticalJet{ _20365, _20366, _20367 }, OpticalJet{ _20428, _20429, _20430 } };
            }
            if (_20566.lo > _20584.hi)
            {
                float _20595 = float(_20553);
                Interval _17084 = _20452;
                Interval _17085 = Interval{ as_type<float>(as_type<uint>(_20595) ^ 2147483648u), as_type<float>(as_type<uint>(_20595) ^ 2147483648u) };
                Interval _20607 = iadd(_17084, _17085, intervalFailed);
                Interval _17086 = _20479;
                Interval _17087 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20609 = jet_add_derivative(_17086, _17087, intervalFailed);
                Interval _17088 = _20491;
                Interval _17089 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20611 = jet_add_derivative(_17088, _17089, intervalFailed);
                float _17082 = 28.0;
                float _17083 = 100.0;
                Interval _20613 = iratio(_17082, _17083, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20618;
                if (!intervalFailed)
                {
                    _20618 = intervalFailed;
                }
                else
                {
                    _20618 = false;
                }
                bool _20623;
                if (_20618)
                {
                    _20623 = jetFailureSite == 0u;
                }
                else
                {
                    _20623 = false;
                }
                if (_20623)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(28.0, 28.0, 100.0, 100.0);
                }
                float _17080 = 44.0;
                float _17081 = 100.0;
                Interval _20627 = iratio(_17080, _17081, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20632;
                if (!intervalFailed)
                {
                    _20632 = intervalFailed;
                }
                else
                {
                    _20632 = false;
                }
                bool _20637;
                if (_20632)
                {
                    _20637 = jetFailureSite == 0u;
                }
                else
                {
                    _20637 = false;
                }
                if (_20637)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(44.0, 44.0, 100.0, 100.0);
                }
                int _1728 = _20553 + 19;
                uint _20643 = (uint(_1728) * 1597334677u) ^ (uint(_20555) * 3812015801u);
                uint _1920 = (_20643 ^ (_20643 >> 16u)) * 2246822519u;
                float _17078 = float((_1920 ^ (_1920 >> 13u)) & 65535u);
                float _17079 = 65535.0;
                Interval _20650 = iratio(_17078, _17079, intervalFailed, optical_product_upper, interval_divide_upper);
                float _20651 = float(_1728);
                float _20652 = float(_20555);
                bool _20657;
                if (!intervalFailed)
                {
                    _20657 = intervalFailed;
                }
                else
                {
                    _20657 = false;
                }
                bool _20662;
                if (_20657)
                {
                    _20662 = jetFailureSite == 0u;
                }
                else
                {
                    _20662 = false;
                }
                if (_20662)
                {
                    jetFailureSite = 7u;
                    jetFailureArguments = float4(_20651, _20651, _20652, _20652);
                }
                Interval _17064 = _20627;
                Interval _17065 = _20650;
                Interval _20666 = imul(_17064, _17065, intervalFailed, optical_product_upper);
                Interval _17066 = Interval{ 0.0, 0.0 };
                Interval _17067 = _20650;
                Interval _20667 = jet_mul_derivative(_17066, _17067, intervalFailed, optical_product_upper);
                Interval _17068 = _20667;
                Interval _17069 = _20627;
                Interval _17070 = Interval{ 0.0, 0.0 };
                Interval _20668 = jet_mul_derivative(_17069, _17070, intervalFailed, optical_product_upper);
                Interval _17071 = _20668;
                Interval _20669 = jet_add_derivative(_17068, _17071, intervalFailed);
                Interval _17072 = Interval{ 0.0, 0.0 };
                Interval _17073 = _20650;
                Interval _20670 = jet_mul_derivative(_17072, _17073, intervalFailed, optical_product_upper);
                Interval _17074 = _20670;
                Interval _17075 = _20627;
                Interval _17076 = Interval{ 0.0, 0.0 };
                Interval _20671 = jet_mul_derivative(_17075, _17076, intervalFailed, optical_product_upper);
                Interval _17077 = _20671;
                Interval _20672 = jet_add_derivative(_17074, _17077, intervalFailed);
                Interval _17058 = _20613;
                Interval _17059 = _20666;
                Interval _20673 = iadd(_17058, _17059, intervalFailed);
                Interval _17060 = Interval{ 0.0, 0.0 };
                Interval _17061 = _20669;
                Interval _20674 = jet_add_derivative(_17060, _17061, intervalFailed);
                Interval _17062 = Interval{ 0.0, 0.0 };
                Interval _17063 = _20672;
                Interval _20675 = jet_add_derivative(_17062, _17063, intervalFailed);
                Interval _17052 = _20607;
                Interval _17053 = Interval{ as_type<float>(as_type<uint>(_20673.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20673.lo) ^ 2147483648u) };
                Interval _20701 = iadd(_17052, _17053, intervalFailed);
                Interval _17054 = _20609;
                Interval _17055 = Interval{ as_type<float>(as_type<uint>(_20674.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20674.lo) ^ 2147483648u) };
                Interval _20703 = jet_add_derivative(_17054, _17055, intervalFailed);
                Interval _17056 = _20611;
                Interval _17057 = Interval{ as_type<float>(as_type<uint>(_20675.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20675.lo) ^ 2147483648u) };
                Interval _20705 = jet_add_derivative(_17056, _17057, intervalFailed);
                float _20706 = float(_20555);
                Interval _17046 = _20499;
                Interval _17047 = Interval{ as_type<float>(as_type<uint>(_20706) ^ 2147483648u), as_type<float>(as_type<uint>(_20706) ^ 2147483648u) };
                Interval _20718 = iadd(_17046, _17047, intervalFailed);
                Interval _17048 = _20526;
                Interval _17049 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20720 = jet_add_derivative(_17048, _17049, intervalFailed);
                Interval _17050 = _20538;
                Interval _17051 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                Interval _20722 = jet_add_derivative(_17050, _17051, intervalFailed);
                float _17044 = 28.0;
                float _17045 = 100.0;
                Interval _20724 = iratio(_17044, _17045, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20729;
                if (!intervalFailed)
                {
                    _20729 = intervalFailed;
                }
                else
                {
                    _20729 = false;
                }
                bool _20734;
                if (_20729)
                {
                    _20734 = jetFailureSite == 0u;
                }
                else
                {
                    _20734 = false;
                }
                if (_20734)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(28.0, 28.0, 100.0, 100.0);
                }
                float _17042 = 44.0;
                float _17043 = 100.0;
                Interval _20738 = iratio(_17042, _17043, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20743;
                if (!intervalFailed)
                {
                    _20743 = intervalFailed;
                }
                else
                {
                    _20743 = false;
                }
                bool _20748;
                if (_20743)
                {
                    _20748 = jetFailureSite == 0u;
                }
                else
                {
                    _20748 = false;
                }
                if (_20748)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(44.0, 44.0, 100.0, 100.0);
                }
                int _1729 = _20555 + 29;
                uint _20754 = (uint(_20553) * 1597334677u) ^ (uint(_1729) * 3812015801u);
                uint _1923 = (_20754 ^ (_20754 >> 16u)) * 2246822519u;
                float _17040 = float((_1923 ^ (_1923 >> 13u)) & 65535u);
                float _17041 = 65535.0;
                Interval _20761 = iratio(_17040, _17041, intervalFailed, optical_product_upper, interval_divide_upper);
                float _20762 = float(_20553);
                float _20763 = float(_1729);
                bool _20768;
                if (!intervalFailed)
                {
                    _20768 = intervalFailed;
                }
                else
                {
                    _20768 = false;
                }
                bool _20773;
                if (_20768)
                {
                    _20773 = jetFailureSite == 0u;
                }
                else
                {
                    _20773 = false;
                }
                if (_20773)
                {
                    jetFailureSite = 7u;
                    jetFailureArguments = float4(_20762, _20762, _20763, _20763);
                }
                Interval _17026 = _20738;
                Interval _17027 = _20761;
                Interval _20777 = imul(_17026, _17027, intervalFailed, optical_product_upper);
                Interval _17028 = Interval{ 0.0, 0.0 };
                Interval _17029 = _20761;
                Interval _20778 = jet_mul_derivative(_17028, _17029, intervalFailed, optical_product_upper);
                Interval _17030 = _20778;
                Interval _17031 = _20738;
                Interval _17032 = Interval{ 0.0, 0.0 };
                Interval _20779 = jet_mul_derivative(_17031, _17032, intervalFailed, optical_product_upper);
                Interval _17033 = _20779;
                Interval _20780 = jet_add_derivative(_17030, _17033, intervalFailed);
                Interval _17034 = Interval{ 0.0, 0.0 };
                Interval _17035 = _20761;
                Interval _20781 = jet_mul_derivative(_17034, _17035, intervalFailed, optical_product_upper);
                Interval _17036 = _20781;
                Interval _17037 = _20738;
                Interval _17038 = Interval{ 0.0, 0.0 };
                Interval _20782 = jet_mul_derivative(_17037, _17038, intervalFailed, optical_product_upper);
                Interval _17039 = _20782;
                Interval _20783 = jet_add_derivative(_17036, _17039, intervalFailed);
                Interval _17020 = _20724;
                Interval _17021 = _20777;
                Interval _20784 = iadd(_17020, _17021, intervalFailed);
                Interval _17022 = Interval{ 0.0, 0.0 };
                Interval _17023 = _20780;
                Interval _20785 = jet_add_derivative(_17022, _17023, intervalFailed);
                Interval _17024 = Interval{ 0.0, 0.0 };
                Interval _17025 = _20783;
                Interval _20786 = jet_add_derivative(_17024, _17025, intervalFailed);
                Interval _17014 = _20718;
                Interval _17015 = Interval{ as_type<float>(as_type<uint>(_20784.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20784.lo) ^ 2147483648u) };
                Interval _20812 = iadd(_17014, _17015, intervalFailed);
                Interval _17016 = _20720;
                Interval _17017 = Interval{ as_type<float>(as_type<uint>(_20785.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20785.lo) ^ 2147483648u) };
                Interval _20814 = jet_add_derivative(_17016, _17017, intervalFailed);
                Interval _17018 = _20722;
                Interval _17019 = Interval{ as_type<float>(as_type<uint>(_20786.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_20786.lo) ^ 2147483648u) };
                Interval _20816 = jet_add_derivative(_17018, _17019, intervalFailed);
                float _17012 = 14.0;
                float _17013 = 100.0;
                Interval _20822 = iratio(_17012, _17013, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20827;
                if (!intervalFailed)
                {
                    _20827 = intervalFailed;
                }
                else
                {
                    _20827 = false;
                }
                bool _20832;
                if (_20827)
                {
                    _20832 = jetFailureSite == 0u;
                }
                else
                {
                    _20832 = false;
                }
                if (_20832)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(14.0, 14.0, 100.0, 100.0);
                }
                Interval _20869;
                Interval _20871;
                Interval _20873;
                Interval _16998 = t.v;
                Interval _16999 = _20822;
                Interval _20835 = imul(_16998, _16999, intervalFailed, optical_product_upper);
                Interval _17000 = t.dx;
                Interval _17001 = _20822;
                Interval _20836 = jet_mul_derivative(_17000, _17001, intervalFailed, optical_product_upper);
                Interval _17002 = _20836;
                Interval _17003 = t.v;
                Interval _17004 = Interval{ 0.0, 0.0 };
                Interval _20837 = jet_mul_derivative(_17003, _17004, intervalFailed, optical_product_upper);
                Interval _17005 = _20837;
                Interval _20838 = jet_add_derivative(_17002, _17005, intervalFailed);
                Interval _17006 = t.dy;
                Interval _17007 = _20822;
                Interval _20839 = jet_mul_derivative(_17006, _17007, intervalFailed, optical_product_upper);
                Interval _17008 = _20839;
                Interval _17009 = t.v;
                Interval _17010 = Interval{ 0.0, 0.0 };
                Interval _20840 = jet_mul_derivative(_17009, _17010, intervalFailed, optical_product_upper);
                Interval _17011 = _20840;
                Interval _20841 = jet_add_derivative(_17008, _17011, intervalFailed);
                Interval _16984 = _20566;
                Interval _16985 = Interval{ 7.0, 7.0 };
                Interval _20842 = imul(_16984, _16985, intervalFailed, optical_product_upper);
                Interval _16986 = Interval{ 0.0, 0.0 };
                Interval _16987 = Interval{ 7.0, 7.0 };
                Interval _20843 = jet_mul_derivative(_16986, _16987, intervalFailed, optical_product_upper);
                Interval _16988 = _20843;
                Interval _16989 = _20566;
                Interval _16990 = Interval{ 0.0, 0.0 };
                Interval _20844 = jet_mul_derivative(_16989, _16990, intervalFailed, optical_product_upper);
                Interval _16991 = _20844;
                Interval _20845 = jet_add_derivative(_16988, _16991, intervalFailed);
                Interval _16992 = Interval{ 0.0, 0.0 };
                Interval _16993 = Interval{ 7.0, 7.0 };
                Interval _20846 = jet_mul_derivative(_16992, _16993, intervalFailed, optical_product_upper);
                Interval _16994 = _20846;
                Interval _16995 = _20566;
                Interval _16996 = Interval{ 0.0, 0.0 };
                Interval _20847 = jet_mul_derivative(_16995, _16996, intervalFailed, optical_product_upper);
                Interval _16997 = _20847;
                Interval _20848 = jet_add_derivative(_16994, _16997, intervalFailed);
                Interval _16978 = _20835;
                Interval _16979 = _20842;
                Interval _20849 = iadd(_16978, _16979, intervalFailed);
                Interval _16980 = _20838;
                Interval _16981 = _20845;
                Interval _20850 = jet_add_derivative(_16980, _16981, intervalFailed);
                Interval _16982 = _20841;
                Interval _16983 = _20848;
                Interval _20851 = jet_add_derivative(_16982, _16983, intervalFailed);
                if (floor(_20849.lo) == floor(_20849.hi))
                {
                    float _20857 = floor(_20849.lo);
                    Interval _16972 = _20849;
                    Interval _16973 = Interval{ as_type<float>(as_type<uint>(_20857) ^ 2147483648u), as_type<float>(as_type<uint>(_20857) ^ 2147483648u) };
                    _20869 = iadd(_16972, _16973, intervalFailed);
                    Interval _16974 = _20850;
                    Interval _16975 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                    _20871 = jet_add_derivative(_16974, _16975, intervalFailed);
                    Interval _16976 = _20851;
                    Interval _16977 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
                    _20873 = jet_add_derivative(_16976, _16977, intervalFailed);
                }
                else
                {
                    jetBranchKnown = false;
                    return OpticalJet3{ OpticalJet{ _20302, _20303, _20304 }, OpticalJet{ _20365, _20366, _20367 }, OpticalJet{ _20428, _20429, _20430 } };
                }
                float _16970 = 3.1415927410125732421875;
                float _20878 = interval_down(_16970, intervalFailed);
                float _16971 = 3.1415927410125732421875;
                float _20879 = interval_up(_16971, intervalFailed);
                Interval _16956 = _20869;
                Interval _16957 = Interval{ _20878, _20879 };
                Interval _20881 = imul(_16956, _16957, intervalFailed, optical_product_upper);
                Interval _16958 = _20871;
                Interval _16959 = Interval{ _20878, _20879 };
                Interval _20883 = jet_mul_derivative(_16958, _16959, intervalFailed, optical_product_upper);
                Interval _16960 = _20883;
                Interval _16961 = _20869;
                Interval _16962 = Interval{ 0.0, 0.0 };
                Interval _20884 = jet_mul_derivative(_16961, _16962, intervalFailed, optical_product_upper);
                Interval _16963 = _20884;
                Interval _20885 = jet_add_derivative(_16960, _16963, intervalFailed);
                Interval _16964 = _20873;
                Interval _16965 = Interval{ _20878, _20879 };
                Interval _20887 = jet_mul_derivative(_16964, _16965, intervalFailed, optical_product_upper);
                Interval _16966 = _20887;
                Interval _16967 = _20869;
                Interval _16968 = Interval{ 0.0, 0.0 };
                Interval _20888 = jet_mul_derivative(_16967, _16968, intervalFailed, optical_product_upper);
                Interval _16969 = _20888;
                Interval _20889 = jet_add_derivative(_16966, _16969, intervalFailed);
                Interval _16948 = _20881;
                float _16946 = 3.1415927410125732421875;
                float _20890 = interval_down(_16946, intervalFailed);
                float _16947 = 3.1415927410125732421875;
                float _20891 = interval_up(_16947, intervalFailed);
                Interval _16949 = Interval{ _20890, _20891 };
                Interval _16950 = Interval{ 0.5, 0.5 };
                Interval _20893 = imul(_16949, _16950, intervalFailed, optical_product_upper);
                Interval _16951 = _20893;
                Interval _20894 = iadd(_16948, _16951, intervalFailed);
                float _16944 = _20894.lo;
                float _16945 = _20894.hi;
                float _20897 = sine_bounds(_16944, _16945, intervalFailed, optical_product_upper, interval_sine_upper);
                float _16942 = _20881.lo;
                float _16943 = _20881.hi;
                float _20901 = sine_bounds(_16942, _16943, intervalFailed, optical_product_upper, interval_sine_upper);
                Interval _16952 = Interval{ _20897, interval_sine_upper };
                Interval _16953 = _20885;
                Interval _20904 = jet_mul_derivative(_16952, _16953, intervalFailed, optical_product_upper);
                Interval _16954 = Interval{ _20897, interval_sine_upper };
                Interval _16955 = _20889;
                Interval _20906 = jet_mul_derivative(_16954, _16955, intervalFailed, optical_product_upper);
                bool _20911;
                if (_20901 <= 0.0)
                {
                    _20911 = interval_sine_upper >= 0.0;
                }
                else
                {
                    _20911 = false;
                }
                float _20918;
                if (_20911)
                {
                    _20918 = 0.0;
                }
                else
                {
                    _20918 = precise::min(abs(_20901), abs(interval_sine_upper));
                }
                float _20921 = precise::max(abs(_20901), abs(interval_sine_upper));
                float _16932 = spvFMul(_20918, _20918);
                float _20922 = interval_down(_16932, intervalFailed);
                float _20923 = precise::max(0.0, _20922);
                float _16933 = spvFMul(_20921, _20921);
                float _20924 = interval_up(_16933, intervalFailed);
                Interval _16934 = Interval{ 2.0, 2.0 };
                Interval _16935 = Interval{ _20901, interval_sine_upper };
                Interval _20926 = imul(_16934, _16935, intervalFailed, optical_product_upper);
                Interval _16936 = _20926;
                Interval _16937 = _20904;
                Interval _20927 = jet_mul_derivative(_16936, _16937, intervalFailed, optical_product_upper);
                Interval _16938 = Interval{ 2.0, 2.0 };
                Interval _16939 = Interval{ _20901, interval_sine_upper };
                Interval _20929 = imul(_16938, _16939, intervalFailed, optical_product_upper);
                Interval _16940 = _20929;
                Interval _16941 = _20906;
                Interval _20930 = jet_mul_derivative(_16940, _16941, intervalFailed, optical_product_upper);
                float _16930 = 55.0;
                float _16931 = 1000.0;
                Interval _20932 = iratio(_16930, _16931, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20937;
                if (!intervalFailed)
                {
                    _20937 = intervalFailed;
                }
                else
                {
                    _20937 = false;
                }
                bool _20942;
                if (_20937)
                {
                    _20942 = jetFailureSite == 0u;
                }
                else
                {
                    _20942 = false;
                }
                if (_20942)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(55.0, 55.0, 1000.0, 1000.0);
                }
                float _16928 = 14.0;
                float _16929 = 100.0;
                Interval _20946 = iratio(_16928, _16929, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _20951;
                if (!intervalFailed)
                {
                    _20951 = intervalFailed;
                }
                else
                {
                    _20951 = false;
                }
                bool _20956;
                if (_20951)
                {
                    _20956 = jetFailureSite == 0u;
                }
                else
                {
                    _20956 = false;
                }
                if (_20956)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(14.0, 14.0, 100.0, 100.0);
                }
                Interval _16914 = _20946;
                Interval _16915 = _20869;
                Interval _20959 = imul(_16914, _16915, intervalFailed, optical_product_upper);
                Interval _16916 = Interval{ 0.0, 0.0 };
                Interval _16917 = _20869;
                Interval _20960 = jet_mul_derivative(_16916, _16917, intervalFailed, optical_product_upper);
                Interval _16918 = _20960;
                Interval _16919 = _20946;
                Interval _16920 = _20871;
                Interval _20961 = jet_mul_derivative(_16919, _16920, intervalFailed, optical_product_upper);
                Interval _16921 = _20961;
                Interval _20962 = jet_add_derivative(_16918, _16921, intervalFailed);
                Interval _16922 = Interval{ 0.0, 0.0 };
                Interval _16923 = _20869;
                Interval _20963 = jet_mul_derivative(_16922, _16923, intervalFailed, optical_product_upper);
                Interval _16924 = _20963;
                Interval _16925 = _20946;
                Interval _16926 = _20873;
                Interval _20964 = jet_mul_derivative(_16925, _16926, intervalFailed, optical_product_upper);
                Interval _16927 = _20964;
                Interval _20965 = jet_add_derivative(_16924, _16927, intervalFailed);
                Interval _16908 = _20932;
                Interval _16909 = _20959;
                Interval _20966 = iadd(_16908, _16909, intervalFailed);
                Interval _16910 = Interval{ 0.0, 0.0 };
                Interval _16911 = _20962;
                Interval _20967 = jet_add_derivative(_16910, _16911, intervalFailed);
                Interval _16912 = Interval{ 0.0, 0.0 };
                Interval _16913 = _20965;
                Interval _20968 = jet_add_derivative(_16912, _16913, intervalFailed);
                bool _20975;
                if (_20966.lo <= 0.0)
                {
                    _20975 = _20966.hi >= 0.0;
                }
                else
                {
                    _20975 = false;
                }
                float _20982;
                if (_20975)
                {
                    _20982 = 0.0;
                }
                else
                {
                    _20982 = precise::min(abs(_20966.lo), abs(_20966.hi));
                }
                float _20985 = precise::max(abs(_20966.lo), abs(_20966.hi));
                float _16898 = spvFMul(_20982, _20982);
                float _20986 = interval_down(_16898, intervalFailed);
                float _20987 = precise::max(0.0, _20986);
                float _16899 = spvFMul(_20985, _20985);
                float _20988 = interval_up(_16899, intervalFailed);
                Interval _16900 = Interval{ 2.0, 2.0 };
                Interval _16901 = _20966;
                Interval _20989 = imul(_16900, _16901, intervalFailed, optical_product_upper);
                Interval _16902 = _20989;
                Interval _16903 = _20967;
                Interval _20990 = jet_mul_derivative(_16902, _16903, intervalFailed, optical_product_upper);
                Interval _16904 = Interval{ 2.0, 2.0 };
                Interval _16905 = _20966;
                Interval _20991 = imul(_16904, _16905, intervalFailed, optical_product_upper);
                Interval _16906 = _20991;
                Interval _16907 = _20968;
                Interval _20992 = jet_mul_derivative(_16906, _16907, intervalFailed, optical_product_upper);
                bool _20999;
                if (_20701.lo <= 0.0)
                {
                    _20999 = _20701.hi >= 0.0;
                }
                else
                {
                    _20999 = false;
                }
                float _21006;
                if (_20999)
                {
                    _21006 = 0.0;
                }
                else
                {
                    _21006 = precise::min(abs(_20701.lo), abs(_20701.hi));
                }
                float _21009 = precise::max(abs(_20701.lo), abs(_20701.hi));
                float _16888 = spvFMul(_21006, _21006);
                float _21010 = interval_down(_16888, intervalFailed);
                float _16889 = spvFMul(_21009, _21009);
                float _21012 = interval_up(_16889, intervalFailed);
                Interval _16890 = Interval{ 2.0, 2.0 };
                Interval _16891 = _20701;
                Interval _21013 = imul(_16890, _16891, intervalFailed, optical_product_upper);
                Interval _16892 = _21013;
                Interval _16893 = _20703;
                Interval _21014 = jet_mul_derivative(_16892, _16893, intervalFailed, optical_product_upper);
                Interval _16894 = Interval{ 2.0, 2.0 };
                Interval _16895 = _20701;
                Interval _21015 = imul(_16894, _16895, intervalFailed, optical_product_upper);
                Interval _16896 = _21015;
                Interval _16897 = _20705;
                Interval _21016 = jet_mul_derivative(_16896, _16897, intervalFailed, optical_product_upper);
                bool _21023;
                if (_20812.lo <= 0.0)
                {
                    _21023 = _20812.hi >= 0.0;
                }
                else
                {
                    _21023 = false;
                }
                float _21030;
                if (_21023)
                {
                    _21030 = 0.0;
                }
                else
                {
                    _21030 = precise::min(abs(_20812.lo), abs(_20812.hi));
                }
                float _21033 = precise::max(abs(_20812.lo), abs(_20812.hi));
                float _16878 = spvFMul(_21030, _21030);
                float _21034 = interval_down(_16878, intervalFailed);
                float _16879 = spvFMul(_21033, _21033);
                float _21036 = interval_up(_16879, intervalFailed);
                Interval _16880 = Interval{ 2.0, 2.0 };
                Interval _16881 = _20812;
                Interval _21037 = imul(_16880, _16881, intervalFailed, optical_product_upper);
                Interval _16882 = _21037;
                Interval _16883 = _20814;
                Interval _21038 = jet_mul_derivative(_16882, _16883, intervalFailed, optical_product_upper);
                Interval _16884 = Interval{ 2.0, 2.0 };
                Interval _16885 = _20812;
                Interval _21039 = imul(_16884, _16885, intervalFailed, optical_product_upper);
                Interval _16886 = _21039;
                Interval _16887 = _20816;
                Interval _21040 = jet_mul_derivative(_16886, _16887, intervalFailed, optical_product_upper);
                Interval _16872 = Interval{ precise::max(0.0, _21010), _21012 };
                Interval _16873 = Interval{ precise::max(0.0, _21034), _21036 };
                Interval _21043 = iadd(_16872, _16873, intervalFailed);
                Interval _16874 = _21014;
                Interval _16875 = _21038;
                Interval _21044 = jet_add_derivative(_16874, _16875, intervalFailed);
                Interval _16876 = _21016;
                Interval _16877 = _21040;
                Interval _21045 = jet_add_derivative(_16876, _16877, intervalFailed);
                float _21048 = precise::max(0.0, _21043.lo);
                Interval _16858 = Interval{ _21048, _21043.hi };
                Interval _16859 = Interval{ _20987, _20988 };
                Interval _21052 = idiv(_16858, _16859, intervalFailed, interval_divide_upper);
                bool _21057;
                if (!intervalFailed)
                {
                    _21057 = intervalFailed;
                }
                else
                {
                    _21057 = false;
                }
                bool _21062;
                if (_21057)
                {
                    _21062 = jetFailureSite == 0u;
                }
                else
                {
                    _21062 = false;
                }
                if (_21062)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_21048, _21043.hi, _20987, _20988);
                }
                Interval _16860 = _21044;
                Interval _16861 = _21052;
                Interval _16862 = _20990;
                Interval _21066 = jet_mul_derivative(_16861, _16862, intervalFailed, optical_product_upper);
                Interval _16863 = Interval{ as_type<float>(as_type<uint>(_21066.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21066.lo) ^ 2147483648u) };
                Interval _21076 = jet_add_derivative(_16860, _16863, intervalFailed);
                Interval _16864 = _21076;
                Interval _16865 = Interval{ _20987, _20988 };
                Interval _21078 = jet_div_derivative(_16864, _16865, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16866 = _21045;
                Interval _16867 = _21052;
                Interval _16868 = _20992;
                Interval _21079 = jet_mul_derivative(_16867, _16868, intervalFailed, optical_product_upper);
                Interval _16869 = Interval{ as_type<float>(as_type<uint>(_21079.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21079.lo) ^ 2147483648u) };
                Interval _21089 = jet_add_derivative(_16866, _16869, intervalFailed);
                Interval _16870 = _21089;
                Interval _16871 = Interval{ _20987, _20988 };
                Interval _21091 = jet_div_derivative(_16870, _16871, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16852 = Interval{ 1.0, 1.0 };
                Interval _16853 = Interval{ as_type<float>(as_type<uint>(_21052.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21052.lo) ^ 2147483648u) };
                Interval _21117 = iadd(_16852, _16853, intervalFailed);
                Interval _16854 = Interval{ 0.0, 0.0 };
                Interval _16855 = Interval{ as_type<float>(as_type<uint>(_21078.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21078.lo) ^ 2147483648u) };
                Interval _21119 = jet_add_derivative(_16854, _16855, intervalFailed);
                Interval _16856 = Interval{ 0.0, 0.0 };
                Interval _16857 = Interval{ as_type<float>(as_type<uint>(_21091.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21091.lo) ^ 2147483648u) };
                Interval _21121 = jet_add_derivative(_16856, _16857, intervalFailed);
                OpticalJet _16848 = OpticalJet{ _21117, _21119, _21121 };
                OpticalJet _16849 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _16850 = jmax(_16848, _16849);
                OpticalJet _16851 = OpticalJet{ Interval{ 1.0, 1.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _21124 = jmin(_16850, _16851);
                Interval _16834 = Interval{ 10.0, 10.0 };
                Interval _16835 = Interval{ _20923, _20924 };
                Interval _21126 = imul(_16834, _16835, intervalFailed, optical_product_upper);
                Interval _16836 = Interval{ 0.0, 0.0 };
                Interval _16837 = Interval{ _20923, _20924 };
                Interval _21128 = jet_mul_derivative(_16836, _16837, intervalFailed, optical_product_upper);
                Interval _16838 = _21128;
                Interval _16839 = Interval{ 10.0, 10.0 };
                Interval _16840 = _20927;
                Interval _21129 = jet_mul_derivative(_16839, _16840, intervalFailed, optical_product_upper);
                Interval _16841 = _21129;
                Interval _21130 = jet_add_derivative(_16838, _16841, intervalFailed);
                Interval _16842 = Interval{ 0.0, 0.0 };
                Interval _16843 = Interval{ _20923, _20924 };
                Interval _21132 = jet_mul_derivative(_16842, _16843, intervalFailed, optical_product_upper);
                Interval _16844 = _21132;
                Interval _16845 = Interval{ 10.0, 10.0 };
                Interval _16846 = _20930;
                Interval _21133 = jet_mul_derivative(_16845, _16846, intervalFailed, optical_product_upper);
                Interval _16847 = _21133;
                Interval _21134 = jet_add_derivative(_16844, _16847, intervalFailed);
                float _16832 = 18.0;
                float _16833 = 100.0;
                Interval _21137 = iratio(_16832, _16833, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _21142;
                if (!intervalFailed)
                {
                    _21142 = intervalFailed;
                }
                else
                {
                    _21142 = false;
                }
                bool _21147;
                if (_21142)
                {
                    _21147 = jetFailureSite == 0u;
                }
                else
                {
                    _21147 = false;
                }
                if (_21147)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(18.0, 18.0, 100.0, 100.0);
                }
                Interval _16818 = footprint.v;
                Interval _16819 = _21137;
                Interval _21153 = imul(_16818, _16819, intervalFailed, optical_product_upper);
                Interval _16820 = footprint.dx;
                Interval _16821 = _21137;
                Interval _21154 = jet_mul_derivative(_16820, _16821, intervalFailed, optical_product_upper);
                Interval _16822 = _21154;
                Interval _16823 = footprint.v;
                Interval _16824 = Interval{ 0.0, 0.0 };
                Interval _21155 = jet_mul_derivative(_16823, _16824, intervalFailed, optical_product_upper);
                Interval _16825 = _21155;
                Interval _21156 = jet_add_derivative(_16822, _16825, intervalFailed);
                Interval _16826 = footprint.dy;
                Interval _16827 = _21137;
                Interval _21157 = jet_mul_derivative(_16826, _16827, intervalFailed, optical_product_upper);
                Interval _16828 = _21157;
                Interval _16829 = footprint.v;
                Interval _16830 = Interval{ 0.0, 0.0 };
                Interval _21158 = jet_mul_derivative(_16829, _16830, intervalFailed, optical_product_upper);
                Interval _16831 = _21158;
                Interval _21159 = jet_add_derivative(_16828, _16831, intervalFailed);
                bool _21166;
                if (_21153.lo <= 0.0)
                {
                    _21166 = _21153.hi >= 0.0;
                }
                else
                {
                    _21166 = false;
                }
                float _21173;
                if (_21166)
                {
                    _21173 = 0.0;
                }
                else
                {
                    _21173 = precise::min(abs(_21153.lo), abs(_21153.hi));
                }
                float _21176 = precise::max(abs(_21153.lo), abs(_21153.hi));
                float _16808 = spvFMul(_21173, _21173);
                float _21177 = interval_down(_16808, intervalFailed);
                float _21178 = precise::max(0.0, _21177);
                float _16809 = spvFMul(_21176, _21176);
                float _21179 = interval_up(_16809, intervalFailed);
                Interval _16810 = Interval{ 2.0, 2.0 };
                Interval _16811 = _21153;
                Interval _21180 = imul(_16810, _16811, intervalFailed, optical_product_upper);
                Interval _16812 = _21180;
                Interval _16813 = _21156;
                Interval _21181 = jet_mul_derivative(_16812, _16813, intervalFailed, optical_product_upper);
                Interval _16814 = Interval{ 2.0, 2.0 };
                Interval _16815 = _21153;
                Interval _21182 = imul(_16814, _16815, intervalFailed, optical_product_upper);
                Interval _16816 = _21182;
                Interval _16817 = _21159;
                Interval _21183 = jet_mul_derivative(_16816, _16817, intervalFailed, optical_product_upper);
                bool _21188;
                if (_21178 <= 0.0)
                {
                    _21188 = _21179 >= 0.0;
                }
                else
                {
                    _21188 = false;
                }
                float _21195;
                if (_21188)
                {
                    _21195 = 0.0;
                }
                else
                {
                    _21195 = precise::min(abs(_21178), abs(_21179));
                }
                float _21198 = precise::max(abs(_21178), abs(_21179));
                float _16798 = spvFMul(_21195, _21195);
                float _21199 = interval_down(_16798, intervalFailed);
                float _16799 = spvFMul(_21198, _21198);
                float _21201 = interval_up(_16799, intervalFailed);
                Interval _16800 = Interval{ 2.0, 2.0 };
                Interval _16801 = Interval{ _21178, _21179 };
                Interval _21203 = imul(_16800, _16801, intervalFailed, optical_product_upper);
                Interval _16802 = _21203;
                Interval _16803 = _21181;
                Interval _21204 = jet_mul_derivative(_16802, _16803, intervalFailed, optical_product_upper);
                Interval _16804 = Interval{ 2.0, 2.0 };
                Interval _16805 = Interval{ _21178, _21179 };
                Interval _21206 = imul(_16804, _16805, intervalFailed, optical_product_upper);
                Interval _16806 = _21206;
                Interval _16807 = _21183;
                Interval _21207 = jet_mul_derivative(_16806, _16807, intervalFailed, optical_product_upper);
                Interval _16792 = Interval{ 1.0, 1.0 };
                Interval _16793 = Interval{ precise::max(0.0, _21199), _21201 };
                Interval _21209 = iadd(_16792, _16793, intervalFailed);
                Interval _16794 = Interval{ 0.0, 0.0 };
                Interval _16795 = _21204;
                Interval _21210 = jet_add_derivative(_16794, _16795, intervalFailed);
                Interval _16796 = Interval{ 0.0, 0.0 };
                Interval _16797 = _21207;
                Interval _21211 = jet_add_derivative(_16796, _16797, intervalFailed);
                Interval _16778 = Interval{ 1.0, 1.0 };
                Interval _16779 = _21209;
                Interval _21213 = idiv(_16778, _16779, intervalFailed, interval_divide_upper);
                bool _21220;
                if (!intervalFailed)
                {
                    _21220 = intervalFailed;
                }
                else
                {
                    _21220 = false;
                }
                bool _21225;
                if (_21220)
                {
                    _21225 = jetFailureSite == 0u;
                }
                else
                {
                    _21225 = false;
                }
                if (_21225)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(1.0, 1.0, _21209.lo, _21209.hi);
                }
                Interval _16780 = Interval{ 0.0, 0.0 };
                Interval _16781 = _21213;
                Interval _16782 = _21210;
                Interval _21229 = jet_mul_derivative(_16781, _16782, intervalFailed, optical_product_upper);
                Interval _16783 = Interval{ as_type<float>(as_type<uint>(_21229.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21229.lo) ^ 2147483648u) };
                Interval _21239 = jet_add_derivative(_16780, _16783, intervalFailed);
                Interval _16784 = _21239;
                Interval _16785 = _21209;
                Interval _21240 = jet_div_derivative(_16784, _16785, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16786 = Interval{ 0.0, 0.0 };
                Interval _16787 = _21213;
                Interval _16788 = _21211;
                Interval _21241 = jet_mul_derivative(_16787, _16788, intervalFailed, optical_product_upper);
                Interval _16789 = Interval{ as_type<float>(as_type<uint>(_21241.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21241.lo) ^ 2147483648u) };
                Interval _21251 = jet_add_derivative(_16786, _16789, intervalFailed);
                Interval _16790 = _21251;
                Interval _16791 = _21209;
                Interval _21252 = jet_div_derivative(_16790, _16791, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16764 = _21126;
                Interval _16765 = _21213;
                Interval _21253 = imul(_16764, _16765, intervalFailed, optical_product_upper);
                Interval _16766 = _21130;
                Interval _16767 = _21213;
                Interval _21254 = jet_mul_derivative(_16766, _16767, intervalFailed, optical_product_upper);
                Interval _16768 = _21254;
                Interval _16769 = _21126;
                Interval _16770 = _21240;
                Interval _21255 = jet_mul_derivative(_16769, _16770, intervalFailed, optical_product_upper);
                Interval _16771 = _21255;
                Interval _21256 = jet_add_derivative(_16768, _16771, intervalFailed);
                Interval _16772 = _21134;
                Interval _16773 = _21213;
                Interval _21257 = jet_mul_derivative(_16772, _16773, intervalFailed, optical_product_upper);
                Interval _16774 = _21257;
                Interval _16775 = _21126;
                Interval _16776 = _21252;
                Interval _21258 = jet_mul_derivative(_16775, _16776, intervalFailed, optical_product_upper);
                Interval _16777 = _21258;
                Interval _21259 = jet_add_derivative(_16774, _16777, intervalFailed);
                Interval _21260 = _21124.v;
                float _21263 = _21260.lo;
                float _21264 = _21260.hi;
                bool _21269;
                if (_21263 <= 0.0)
                {
                    _21269 = _21264 >= 0.0;
                }
                else
                {
                    _21269 = false;
                }
                float _21276;
                if (_21269)
                {
                    _21276 = 0.0;
                }
                else
                {
                    _21276 = precise::min(abs(_21263), abs(_21264));
                }
                float _21279 = precise::max(abs(_21263), abs(_21264));
                float _16754 = spvFMul(_21276, _21276);
                float _21280 = interval_down(_16754, intervalFailed);
                float _21281 = precise::max(0.0, _21280);
                float _16755 = spvFMul(_21279, _21279);
                float _21282 = interval_up(_16755, intervalFailed);
                Interval _16756 = Interval{ 2.0, 2.0 };
                Interval _16757 = _21260;
                Interval _21283 = imul(_16756, _16757, intervalFailed, optical_product_upper);
                Interval _16758 = _21283;
                Interval _16759 = _21124.dx;
                Interval _21284 = jet_mul_derivative(_16758, _16759, intervalFailed, optical_product_upper);
                Interval _16760 = Interval{ 2.0, 2.0 };
                Interval _16761 = _21260;
                Interval _21285 = imul(_16760, _16761, intervalFailed, optical_product_upper);
                Interval _16762 = _21285;
                Interval _16763 = _21124.dy;
                Interval _21286 = jet_mul_derivative(_16762, _16763, intervalFailed, optical_product_upper);
                Interval _16740 = _21253;
                Interval _16741 = Interval{ _21281, _21282 };
                Interval _21288 = imul(_16740, _16741, intervalFailed, optical_product_upper);
                Interval _16742 = _21256;
                Interval _16743 = Interval{ _21281, _21282 };
                Interval _21290 = jet_mul_derivative(_16742, _16743, intervalFailed, optical_product_upper);
                Interval _16744 = _21290;
                Interval _16745 = _21253;
                Interval _16746 = _21284;
                Interval _21291 = jet_mul_derivative(_16745, _16746, intervalFailed, optical_product_upper);
                Interval _16747 = _21291;
                Interval _21292 = jet_add_derivative(_16744, _16747, intervalFailed);
                Interval _16748 = _21259;
                Interval _16749 = Interval{ _21281, _21282 };
                Interval _21294 = jet_mul_derivative(_16748, _16749, intervalFailed, optical_product_upper);
                Interval _16750 = _21294;
                Interval _16751 = _21253;
                Interval _16752 = _21286;
                Interval _21295 = jet_mul_derivative(_16751, _16752, intervalFailed, optical_product_upper);
                Interval _16753 = _21295;
                Interval _21296 = jet_add_derivative(_16750, _16753, intervalFailed);
                Interval _21297 = _21124.v;
                Interval _16726 = _21288;
                Interval _16727 = _21297;
                Interval _21300 = imul(_16726, _16727, intervalFailed, optical_product_upper);
                Interval _16728 = _21292;
                Interval _16729 = _21297;
                Interval _21301 = jet_mul_derivative(_16728, _16729, intervalFailed, optical_product_upper);
                Interval _16730 = _21301;
                Interval _16731 = _21288;
                Interval _16732 = _21124.dx;
                Interval _21302 = jet_mul_derivative(_16731, _16732, intervalFailed, optical_product_upper);
                Interval _16733 = _21302;
                Interval _21303 = jet_add_derivative(_16730, _16733, intervalFailed);
                Interval _16734 = _21296;
                Interval _16735 = _21297;
                Interval _21304 = jet_mul_derivative(_16734, _16735, intervalFailed, optical_product_upper);
                Interval _16736 = _21304;
                Interval _16737 = _21288;
                Interval _16738 = _21124.dy;
                Interval _21305 = jet_mul_derivative(_16737, _16738, intervalFailed, optical_product_upper);
                Interval _16739 = _21305;
                Interval _21306 = jet_add_derivative(_16736, _16739, intervalFailed);
                Interval _16712 = Interval{ -6.0, -6.0 };
                Interval _16713 = _21253;
                Interval _21307 = imul(_16712, _16713, intervalFailed, optical_product_upper);
                Interval _16714 = Interval{ 0.0, 0.0 };
                Interval _16715 = _21253;
                Interval _21308 = jet_mul_derivative(_16714, _16715, intervalFailed, optical_product_upper);
                Interval _16716 = _21308;
                Interval _16717 = Interval{ -6.0, -6.0 };
                Interval _16718 = _21256;
                Interval _21309 = jet_mul_derivative(_16717, _16718, intervalFailed, optical_product_upper);
                Interval _16719 = _21309;
                Interval _21310 = jet_add_derivative(_16716, _16719, intervalFailed);
                Interval _16720 = Interval{ 0.0, 0.0 };
                Interval _16721 = _21253;
                Interval _21311 = jet_mul_derivative(_16720, _16721, intervalFailed, optical_product_upper);
                Interval _16722 = _21311;
                Interval _16723 = Interval{ -6.0, -6.0 };
                Interval _16724 = _21259;
                Interval _21312 = jet_mul_derivative(_16723, _16724, intervalFailed, optical_product_upper);
                Interval _16725 = _21312;
                Interval _21313 = jet_add_derivative(_16722, _16725, intervalFailed);
                Interval _16698 = _21307;
                Interval _16699 = Interval{ _21281, _21282 };
                Interval _21315 = imul(_16698, _16699, intervalFailed, optical_product_upper);
                Interval _16700 = _21310;
                Interval _16701 = Interval{ _21281, _21282 };
                Interval _21317 = jet_mul_derivative(_16700, _16701, intervalFailed, optical_product_upper);
                Interval _16702 = _21317;
                Interval _16703 = _21307;
                Interval _16704 = _21284;
                Interval _21318 = jet_mul_derivative(_16703, _16704, intervalFailed, optical_product_upper);
                Interval _16705 = _21318;
                Interval _21319 = jet_add_derivative(_16702, _16705, intervalFailed);
                Interval _16706 = _21313;
                Interval _16707 = Interval{ _21281, _21282 };
                Interval _21321 = jet_mul_derivative(_16706, _16707, intervalFailed, optical_product_upper);
                Interval _16708 = _21321;
                Interval _16709 = _21307;
                Interval _16710 = _21286;
                Interval _21322 = jet_mul_derivative(_16709, _16710, intervalFailed, optical_product_upper);
                Interval _16711 = _21322;
                Interval _21323 = jet_add_derivative(_16708, _16711, intervalFailed);
                Interval _16684 = Interval{ 128.0, 128.0 };
                Interval _16685 = Interval{ _20987, _20988 };
                Interval _21325 = imul(_16684, _16685, intervalFailed, optical_product_upper);
                Interval _16686 = Interval{ 0.0, 0.0 };
                Interval _16687 = Interval{ _20987, _20988 };
                Interval _21327 = jet_mul_derivative(_16686, _16687, intervalFailed, optical_product_upper);
                Interval _16688 = _21327;
                Interval _16689 = Interval{ 128.0, 128.0 };
                Interval _16690 = _20990;
                Interval _21328 = jet_mul_derivative(_16689, _16690, intervalFailed, optical_product_upper);
                Interval _16691 = _21328;
                Interval _21329 = jet_add_derivative(_16688, _16691, intervalFailed);
                Interval _16692 = Interval{ 0.0, 0.0 };
                Interval _16693 = Interval{ _20987, _20988 };
                Interval _21331 = jet_mul_derivative(_16692, _16693, intervalFailed, optical_product_upper);
                Interval _16694 = _21331;
                Interval _16695 = Interval{ 128.0, 128.0 };
                Interval _16696 = _20992;
                Interval _21332 = jet_mul_derivative(_16695, _16696, intervalFailed, optical_product_upper);
                Interval _16697 = _21332;
                Interval _21333 = jet_add_derivative(_16694, _16697, intervalFailed);
                Interval _16670 = _21315;
                Interval _16671 = _21325;
                Interval _21335 = idiv(_16670, _16671, intervalFailed, interval_divide_upper);
                bool _21344;
                if (!intervalFailed)
                {
                    _21344 = intervalFailed;
                }
                else
                {
                    _21344 = false;
                }
                bool _21349;
                if (_21344)
                {
                    _21349 = jetFailureSite == 0u;
                }
                else
                {
                    _21349 = false;
                }
                if (_21349)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_21315.lo, _21315.hi, _21325.lo, _21325.hi);
                }
                Interval _16672 = _21319;
                Interval _16673 = _21335;
                Interval _16674 = _21329;
                Interval _21353 = jet_mul_derivative(_16673, _16674, intervalFailed, optical_product_upper);
                Interval _16675 = Interval{ as_type<float>(as_type<uint>(_21353.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21353.lo) ^ 2147483648u) };
                Interval _21363 = jet_add_derivative(_16672, _16675, intervalFailed);
                Interval _16676 = _21363;
                Interval _16677 = _21325;
                Interval _21364 = jet_div_derivative(_16676, _16677, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16678 = _21323;
                Interval _16679 = _21335;
                Interval _16680 = _21333;
                Interval _21365 = jet_mul_derivative(_16679, _16680, intervalFailed, optical_product_upper);
                Interval _16681 = Interval{ as_type<float>(as_type<uint>(_21365.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_21365.lo) ^ 2147483648u) };
                Interval _21375 = jet_add_derivative(_16678, _16681, intervalFailed);
                Interval _16682 = _21375;
                Interval _16683 = _21325;
                Interval _21376 = jet_div_derivative(_16682, _16683, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _16656 = _21335;
                Interval _16657 = _20701;
                Interval _21377 = imul(_16656, _16657, intervalFailed, optical_product_upper);
                Interval _16658 = _21364;
                Interval _16659 = _20701;
                Interval _21378 = jet_mul_derivative(_16658, _16659, intervalFailed, optical_product_upper);
                Interval _16660 = _21378;
                Interval _16661 = _21335;
                Interval _16662 = _20703;
                Interval _21379 = jet_mul_derivative(_16661, _16662, intervalFailed, optical_product_upper);
                Interval _16663 = _21379;
                Interval _21380 = jet_add_derivative(_16660, _16663, intervalFailed);
                Interval _16664 = _21376;
                Interval _16665 = _20701;
                Interval _21381 = jet_mul_derivative(_16664, _16665, intervalFailed, optical_product_upper);
                Interval _16666 = _21381;
                Interval _16667 = _21335;
                Interval _16668 = _20705;
                Interval _21382 = jet_mul_derivative(_16667, _16668, intervalFailed, optical_product_upper);
                Interval _16669 = _21382;
                Interval _21383 = jet_add_derivative(_16666, _16669, intervalFailed);
                Interval _16642 = _21335;
                Interval _16643 = _20812;
                Interval _21384 = imul(_16642, _16643, intervalFailed, optical_product_upper);
                Interval _16644 = _21364;
                Interval _16645 = _20812;
                Interval _21385 = jet_mul_derivative(_16644, _16645, intervalFailed, optical_product_upper);
                Interval _16646 = _21385;
                Interval _16647 = _21335;
                Interval _16648 = _20814;
                Interval _21386 = jet_mul_derivative(_16647, _16648, intervalFailed, optical_product_upper);
                Interval _16649 = _21386;
                Interval _21387 = jet_add_derivative(_16646, _16649, intervalFailed);
                Interval _16650 = _21376;
                Interval _16651 = _20812;
                Interval _21388 = jet_mul_derivative(_16650, _16651, intervalFailed, optical_product_upper);
                Interval _16652 = _21388;
                Interval _16653 = _21335;
                Interval _16654 = _20816;
                Interval _21389 = jet_mul_derivative(_16653, _16654, intervalFailed, optical_product_upper);
                Interval _16655 = _21389;
                Interval _21390 = jet_add_derivative(_16652, _16655, intervalFailed);
                Interval _16636 = _20302;
                Interval _16637 = _21300;
                Interval _21391 = iadd(_16636, _16637, intervalFailed);
                Interval _16638 = _20303;
                Interval _16639 = _21303;
                Interval _21392 = jet_add_derivative(_16638, _16639, intervalFailed);
                Interval _16640 = _20304;
                Interval _16641 = _21306;
                Interval _21393 = jet_add_derivative(_16640, _16641, intervalFailed);
                Interval _16630 = _20365;
                Interval _16631 = _21377;
                Interval _21400 = iadd(_16630, _16631, intervalFailed);
                Interval _16632 = _20366;
                Interval _16633 = _21380;
                Interval _21401 = jet_add_derivative(_16632, _16633, intervalFailed);
                Interval _16634 = _20367;
                Interval _16635 = _21383;
                Interval _21402 = jet_add_derivative(_16634, _16635, intervalFailed);
                Interval _16624 = _20428;
                Interval _16625 = _21384;
                Interval _21409 = iadd(_16624, _16625, intervalFailed);
                Interval _16626 = _20429;
                Interval _16627 = _21387;
                Interval _21410 = jet_add_derivative(_16626, _16627, intervalFailed);
                Interval _16628 = _20430;
                Interval _16629 = _21390;
                Interval _21411 = jet_add_derivative(_16628, _16629, intervalFailed);
                _21418 = _21409.lo;
                _21419 = _21409.hi;
                _21420 = _21410.lo;
                _21421 = _21410.hi;
                _21422 = _21411.lo;
                _21423 = _21411.hi;
                _21424 = _21400.lo;
                _21425 = _21400.hi;
                _21426 = _21401.lo;
                _21427 = _21401.hi;
                _21428 = _21402.lo;
                _21429 = _21402.hi;
                _21430 = _21391.lo;
                _21431 = _21391.hi;
                _21432 = _21392.lo;
                _21433 = _21392.hi;
                _21434 = _21393.lo;
                _21435 = _21393.hi;
            }
            else
            {
                _21418 = _20428.lo;
                _21419 = _20428.hi;
                _21420 = _20429.lo;
                _21421 = _20429.hi;
                _21422 = _20430.lo;
                _21423 = _20430.hi;
                _21424 = _20365.lo;
                _21425 = _20365.hi;
                _21426 = _20366.lo;
                _21427 = _20366.hi;
                _21428 = _20367.lo;
                _21429 = _20367.hi;
                _21430 = _20302.lo;
                _21431 = _20302.hi;
                _21432 = _20303.lo;
                _21433 = _20303.hi;
                _21434 = _20304.lo;
                _21435 = _20304.hi;
            }
        }
        _21436 = _21418;
        _21437 = _21419;
        _21438 = _21420;
        _21439 = _21421;
        _21440 = _21422;
        _21441 = _21423;
        _21442 = _21424;
        _21443 = _21425;
        _21444 = _21426;
        _21445 = _21427;
        _21446 = _21428;
        _21447 = _21429;
        _21448 = _21430;
        _21449 = _21431;
        _21450 = _21432;
        _21451 = _21433;
        _21452 = _21434;
        _21453 = _21435;
    }
    else
    {
        _21436 = _20428.lo;
        _21437 = _20428.hi;
        _21438 = _20429.lo;
        _21439 = _20429.hi;
        _21440 = _20430.lo;
        _21441 = _20430.hi;
        _21442 = _20365.lo;
        _21443 = _20365.hi;
        _21444 = _20366.lo;
        _21445 = _20366.hi;
        _21446 = _20367.lo;
        _21447 = _20367.hi;
        _21448 = _20302.lo;
        _21449 = _20302.hi;
        _21450 = _20303.lo;
        _21451 = _20303.hi;
        _21452 = _20304.lo;
        _21453 = _20304.hi;
    }
    return OpticalJet3{ OpticalJet{ Interval{ _21448, _21449 }, Interval{ _21450, _21451 }, Interval{ _21452, _21453 } }, OpticalJet{ Interval{ _21442, _21443 }, Interval{ _21444, _21445 }, Interval{ _21446, _21447 } }, OpticalJet{ Interval{ _21436, _21437 }, Interval{ _21438, _21439 }, Interval{ _21440, _21441 } } };
}

static inline __attribute__((always_inline))
OpticalJet3 jet_liquid_normal(thread const ReflectionLiquidFrame& f, thread const OpticalJet3& direction, thread const OpticalJet& _distance, thread const OpticalJet& footprint, thread OpticalJet3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    Interval _13445 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13446 = hit.x.v;
    Interval _13474 = imul(_13445, _13446, intervalFailed, optical_product_upper);
    Interval _13447 = Interval{ 0.0, 0.0 };
    Interval _13448 = hit.x.v;
    Interval _13475 = jet_mul_derivative(_13447, _13448, intervalFailed, optical_product_upper);
    Interval _13449 = _13475;
    Interval _13450 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13451 = hit.x.dx;
    Interval _13477 = jet_mul_derivative(_13450, _13451, intervalFailed, optical_product_upper);
    Interval _13452 = _13477;
    Interval _13478 = jet_add_derivative(_13449, _13452, intervalFailed);
    Interval _13453 = Interval{ 0.0, 0.0 };
    Interval _13454 = hit.x.v;
    Interval _13479 = jet_mul_derivative(_13453, _13454, intervalFailed, optical_product_upper);
    Interval _13455 = _13479;
    Interval _13456 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _13457 = hit.x.dy;
    Interval _13481 = jet_mul_derivative(_13456, _13457, intervalFailed, optical_product_upper);
    Interval _13458 = _13481;
    Interval _13482 = jet_add_derivative(_13455, _13458, intervalFailed);
    Interval _13431 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13432 = hit.x.v;
    Interval _13487 = imul(_13431, _13432, intervalFailed, optical_product_upper);
    Interval _13433 = Interval{ 0.0, 0.0 };
    Interval _13434 = hit.x.v;
    Interval _13488 = jet_mul_derivative(_13433, _13434, intervalFailed, optical_product_upper);
    Interval _13435 = _13488;
    Interval _13436 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13437 = hit.x.dx;
    Interval _13490 = jet_mul_derivative(_13436, _13437, intervalFailed, optical_product_upper);
    Interval _13438 = _13490;
    Interval _13491 = jet_add_derivative(_13435, _13438, intervalFailed);
    Interval _13439 = Interval{ 0.0, 0.0 };
    Interval _13440 = hit.x.v;
    Interval _13492 = jet_mul_derivative(_13439, _13440, intervalFailed, optical_product_upper);
    Interval _13441 = _13492;
    Interval _13442 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _13443 = hit.x.dy;
    Interval _13494 = jet_mul_derivative(_13442, _13443, intervalFailed, optical_product_upper);
    Interval _13444 = _13494;
    Interval _13495 = jet_add_derivative(_13441, _13444, intervalFailed);
    Interval _13417 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13418 = hit.x.v;
    Interval _13500 = imul(_13417, _13418, intervalFailed, optical_product_upper);
    Interval _13419 = Interval{ 0.0, 0.0 };
    Interval _13420 = hit.x.v;
    Interval _13501 = jet_mul_derivative(_13419, _13420, intervalFailed, optical_product_upper);
    Interval _13421 = _13501;
    Interval _13422 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13423 = hit.x.dx;
    Interval _13503 = jet_mul_derivative(_13422, _13423, intervalFailed, optical_product_upper);
    Interval _13424 = _13503;
    Interval _13504 = jet_add_derivative(_13421, _13424, intervalFailed);
    Interval _13425 = Interval{ 0.0, 0.0 };
    Interval _13426 = hit.x.v;
    Interval _13505 = jet_mul_derivative(_13425, _13426, intervalFailed, optical_product_upper);
    Interval _13427 = _13505;
    Interval _13428 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _13429 = hit.x.dy;
    Interval _13507 = jet_mul_derivative(_13428, _13429, intervalFailed, optical_product_upper);
    Interval _13430 = _13507;
    Interval _13508 = jet_add_derivative(_13427, _13430, intervalFailed);
    Interval _13403 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13404 = hit.y.v;
    Interval _13516 = imul(_13403, _13404, intervalFailed, optical_product_upper);
    Interval _13405 = Interval{ 0.0, 0.0 };
    Interval _13406 = hit.y.v;
    Interval _13517 = jet_mul_derivative(_13405, _13406, intervalFailed, optical_product_upper);
    Interval _13407 = _13517;
    Interval _13408 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13409 = hit.y.dx;
    Interval _13519 = jet_mul_derivative(_13408, _13409, intervalFailed, optical_product_upper);
    Interval _13410 = _13519;
    Interval _13520 = jet_add_derivative(_13407, _13410, intervalFailed);
    Interval _13411 = Interval{ 0.0, 0.0 };
    Interval _13412 = hit.y.v;
    Interval _13521 = jet_mul_derivative(_13411, _13412, intervalFailed, optical_product_upper);
    Interval _13413 = _13521;
    Interval _13414 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _13415 = hit.y.dy;
    Interval _13523 = jet_mul_derivative(_13414, _13415, intervalFailed, optical_product_upper);
    Interval _13416 = _13523;
    Interval _13524 = jet_add_derivative(_13413, _13416, intervalFailed);
    Interval _13389 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13390 = hit.y.v;
    Interval _13529 = imul(_13389, _13390, intervalFailed, optical_product_upper);
    Interval _13391 = Interval{ 0.0, 0.0 };
    Interval _13392 = hit.y.v;
    Interval _13530 = jet_mul_derivative(_13391, _13392, intervalFailed, optical_product_upper);
    Interval _13393 = _13530;
    Interval _13394 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13395 = hit.y.dx;
    Interval _13532 = jet_mul_derivative(_13394, _13395, intervalFailed, optical_product_upper);
    Interval _13396 = _13532;
    Interval _13533 = jet_add_derivative(_13393, _13396, intervalFailed);
    Interval _13397 = Interval{ 0.0, 0.0 };
    Interval _13398 = hit.y.v;
    Interval _13534 = jet_mul_derivative(_13397, _13398, intervalFailed, optical_product_upper);
    Interval _13399 = _13534;
    Interval _13400 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _13401 = hit.y.dy;
    Interval _13536 = jet_mul_derivative(_13400, _13401, intervalFailed, optical_product_upper);
    Interval _13402 = _13536;
    Interval _13537 = jet_add_derivative(_13399, _13402, intervalFailed);
    Interval _13375 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13376 = hit.y.v;
    Interval _13542 = imul(_13375, _13376, intervalFailed, optical_product_upper);
    Interval _13377 = Interval{ 0.0, 0.0 };
    Interval _13378 = hit.y.v;
    Interval _13543 = jet_mul_derivative(_13377, _13378, intervalFailed, optical_product_upper);
    Interval _13379 = _13543;
    Interval _13380 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13381 = hit.y.dx;
    Interval _13545 = jet_mul_derivative(_13380, _13381, intervalFailed, optical_product_upper);
    Interval _13382 = _13545;
    Interval _13546 = jet_add_derivative(_13379, _13382, intervalFailed);
    Interval _13383 = Interval{ 0.0, 0.0 };
    Interval _13384 = hit.y.v;
    Interval _13547 = jet_mul_derivative(_13383, _13384, intervalFailed, optical_product_upper);
    Interval _13385 = _13547;
    Interval _13386 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _13387 = hit.y.dy;
    Interval _13549 = jet_mul_derivative(_13386, _13387, intervalFailed, optical_product_upper);
    Interval _13388 = _13549;
    Interval _13550 = jet_add_derivative(_13385, _13388, intervalFailed);
    Interval _13369 = _13474;
    Interval _13370 = _13516;
    Interval _13551 = iadd(_13369, _13370, intervalFailed);
    Interval _13371 = _13478;
    Interval _13372 = _13520;
    Interval _13552 = jet_add_derivative(_13371, _13372, intervalFailed);
    Interval _13373 = _13482;
    Interval _13374 = _13524;
    Interval _13553 = jet_add_derivative(_13373, _13374, intervalFailed);
    Interval _13363 = _13487;
    Interval _13364 = _13529;
    Interval _13554 = iadd(_13363, _13364, intervalFailed);
    Interval _13365 = _13491;
    Interval _13366 = _13533;
    Interval _13555 = jet_add_derivative(_13365, _13366, intervalFailed);
    Interval _13367 = _13495;
    Interval _13368 = _13537;
    Interval _13556 = jet_add_derivative(_13367, _13368, intervalFailed);
    Interval _13357 = _13500;
    Interval _13358 = _13542;
    Interval _13557 = iadd(_13357, _13358, intervalFailed);
    Interval _13359 = _13504;
    Interval _13360 = _13546;
    Interval _13558 = jet_add_derivative(_13359, _13360, intervalFailed);
    Interval _13361 = _13508;
    Interval _13362 = _13550;
    Interval _13559 = jet_add_derivative(_13361, _13362, intervalFailed);
    Interval _13343 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13344 = hit.z.v;
    Interval _13567 = imul(_13343, _13344, intervalFailed, optical_product_upper);
    Interval _13345 = Interval{ 0.0, 0.0 };
    Interval _13346 = hit.z.v;
    Interval _13568 = jet_mul_derivative(_13345, _13346, intervalFailed, optical_product_upper);
    Interval _13347 = _13568;
    Interval _13348 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13349 = hit.z.dx;
    Interval _13570 = jet_mul_derivative(_13348, _13349, intervalFailed, optical_product_upper);
    Interval _13350 = _13570;
    Interval _13571 = jet_add_derivative(_13347, _13350, intervalFailed);
    Interval _13351 = Interval{ 0.0, 0.0 };
    Interval _13352 = hit.z.v;
    Interval _13572 = jet_mul_derivative(_13351, _13352, intervalFailed, optical_product_upper);
    Interval _13353 = _13572;
    Interval _13354 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _13355 = hit.z.dy;
    Interval _13574 = jet_mul_derivative(_13354, _13355, intervalFailed, optical_product_upper);
    Interval _13356 = _13574;
    Interval _13575 = jet_add_derivative(_13353, _13356, intervalFailed);
    Interval _13329 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13330 = hit.z.v;
    Interval _13580 = imul(_13329, _13330, intervalFailed, optical_product_upper);
    Interval _13331 = Interval{ 0.0, 0.0 };
    Interval _13332 = hit.z.v;
    Interval _13581 = jet_mul_derivative(_13331, _13332, intervalFailed, optical_product_upper);
    Interval _13333 = _13581;
    Interval _13334 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13335 = hit.z.dx;
    Interval _13583 = jet_mul_derivative(_13334, _13335, intervalFailed, optical_product_upper);
    Interval _13336 = _13583;
    Interval _13584 = jet_add_derivative(_13333, _13336, intervalFailed);
    Interval _13337 = Interval{ 0.0, 0.0 };
    Interval _13338 = hit.z.v;
    Interval _13585 = jet_mul_derivative(_13337, _13338, intervalFailed, optical_product_upper);
    Interval _13339 = _13585;
    Interval _13340 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _13341 = hit.z.dy;
    Interval _13587 = jet_mul_derivative(_13340, _13341, intervalFailed, optical_product_upper);
    Interval _13342 = _13587;
    Interval _13588 = jet_add_derivative(_13339, _13342, intervalFailed);
    Interval _13315 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13316 = hit.z.v;
    Interval _13593 = imul(_13315, _13316, intervalFailed, optical_product_upper);
    Interval _13317 = Interval{ 0.0, 0.0 };
    Interval _13318 = hit.z.v;
    Interval _13594 = jet_mul_derivative(_13317, _13318, intervalFailed, optical_product_upper);
    Interval _13319 = _13594;
    Interval _13320 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13321 = hit.z.dx;
    Interval _13596 = jet_mul_derivative(_13320, _13321, intervalFailed, optical_product_upper);
    Interval _13322 = _13596;
    Interval _13597 = jet_add_derivative(_13319, _13322, intervalFailed);
    Interval _13323 = Interval{ 0.0, 0.0 };
    Interval _13324 = hit.z.v;
    Interval _13598 = jet_mul_derivative(_13323, _13324, intervalFailed, optical_product_upper);
    Interval _13325 = _13598;
    Interval _13326 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _13327 = hit.z.dy;
    Interval _13600 = jet_mul_derivative(_13326, _13327, intervalFailed, optical_product_upper);
    Interval _13328 = _13600;
    Interval _13601 = jet_add_derivative(_13325, _13328, intervalFailed);
    Interval _13309 = _13551;
    Interval _13310 = _13567;
    Interval _13602 = iadd(_13309, _13310, intervalFailed);
    Interval _13311 = _13552;
    Interval _13312 = _13571;
    Interval _13603 = jet_add_derivative(_13311, _13312, intervalFailed);
    Interval _13313 = _13553;
    Interval _13314 = _13575;
    Interval _13604 = jet_add_derivative(_13313, _13314, intervalFailed);
    Interval _13303 = _13554;
    Interval _13304 = _13580;
    Interval _13605 = iadd(_13303, _13304, intervalFailed);
    Interval _13305 = _13555;
    Interval _13306 = _13584;
    Interval _13606 = jet_add_derivative(_13305, _13306, intervalFailed);
    Interval _13307 = _13556;
    Interval _13308 = _13588;
    Interval _13607 = jet_add_derivative(_13307, _13308, intervalFailed);
    Interval _13297 = _13557;
    Interval _13298 = _13593;
    Interval _13608 = iadd(_13297, _13298, intervalFailed);
    Interval _13299 = _13558;
    Interval _13300 = _13597;
    Interval _13609 = jet_add_derivative(_13299, _13300, intervalFailed);
    Interval _13301 = _13559;
    Interval _13302 = _13601;
    Interval _13610 = jet_add_derivative(_13301, _13302, intervalFailed);
    Interval _13291 = _13602;
    Interval _13292 = Interval{ f.rotation0.w, f.rotation0.w };
    Interval _13621 = iadd(_13291, _13292, intervalFailed);
    Interval _13293 = _13603;
    Interval _13294 = Interval{ 0.0, 0.0 };
    Interval _13622 = jet_add_derivative(_13293, _13294, intervalFailed);
    Interval _13295 = _13604;
    Interval _13296 = Interval{ 0.0, 0.0 };
    Interval _13623 = jet_add_derivative(_13295, _13296, intervalFailed);
    Interval _13285 = _13605;
    Interval _13286 = Interval{ f.rotation1.w, f.rotation1.w };
    Interval _13625 = iadd(_13285, _13286, intervalFailed);
    Interval _13287 = _13606;
    Interval _13288 = Interval{ 0.0, 0.0 };
    Interval _13626 = jet_add_derivative(_13287, _13288, intervalFailed);
    Interval _13289 = _13607;
    Interval _13290 = Interval{ 0.0, 0.0 };
    Interval _13627 = jet_add_derivative(_13289, _13290, intervalFailed);
    Interval _13279 = _13608;
    Interval _13280 = Interval{ f.rotation2.w, f.rotation2.w };
    Interval _13629 = iadd(_13279, _13280, intervalFailed);
    Interval _13281 = _13609;
    Interval _13282 = Interval{ 0.0, 0.0 };
    Interval _13630 = jet_add_derivative(_13281, _13282, intervalFailed);
    Interval _13283 = _13610;
    Interval _13284 = Interval{ 0.0, 0.0 };
    Interval _13631 = jet_add_derivative(_13283, _13284, intervalFailed);
    float _15245;
    float _15246;
    float _15247;
    float _15248;
    float _15249;
    float _15250;
    float _15251;
    float _15252;
    float _15253;
    float _15254;
    float _15255;
    float _15256;
    if (f.settings.y == 3.0)
    {
        Interval _13265 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13266 = direction.x.v;
        Interval _13672 = imul(_13265, _13266, intervalFailed, optical_product_upper);
        Interval _13267 = Interval{ 0.0, 0.0 };
        Interval _13268 = direction.x.v;
        Interval _13673 = jet_mul_derivative(_13267, _13268, intervalFailed, optical_product_upper);
        Interval _13269 = _13673;
        Interval _13270 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13271 = direction.x.dx;
        Interval _13675 = jet_mul_derivative(_13270, _13271, intervalFailed, optical_product_upper);
        Interval _13272 = _13675;
        Interval _13676 = jet_add_derivative(_13269, _13272, intervalFailed);
        Interval _13273 = Interval{ 0.0, 0.0 };
        Interval _13274 = direction.x.v;
        Interval _13677 = jet_mul_derivative(_13273, _13274, intervalFailed, optical_product_upper);
        Interval _13275 = _13677;
        Interval _13276 = Interval{ f.rotation0.x, f.rotation0.x };
        Interval _13277 = direction.x.dy;
        Interval _13679 = jet_mul_derivative(_13276, _13277, intervalFailed, optical_product_upper);
        Interval _13278 = _13679;
        Interval _13680 = jet_add_derivative(_13275, _13278, intervalFailed);
        Interval _13251 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13252 = direction.x.v;
        Interval _13685 = imul(_13251, _13252, intervalFailed, optical_product_upper);
        Interval _13253 = Interval{ 0.0, 0.0 };
        Interval _13254 = direction.x.v;
        Interval _13686 = jet_mul_derivative(_13253, _13254, intervalFailed, optical_product_upper);
        Interval _13255 = _13686;
        Interval _13256 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13257 = direction.x.dx;
        Interval _13688 = jet_mul_derivative(_13256, _13257, intervalFailed, optical_product_upper);
        Interval _13258 = _13688;
        Interval _13689 = jet_add_derivative(_13255, _13258, intervalFailed);
        Interval _13259 = Interval{ 0.0, 0.0 };
        Interval _13260 = direction.x.v;
        Interval _13690 = jet_mul_derivative(_13259, _13260, intervalFailed, optical_product_upper);
        Interval _13261 = _13690;
        Interval _13262 = Interval{ f.rotation0.y, f.rotation0.y };
        Interval _13263 = direction.x.dy;
        Interval _13692 = jet_mul_derivative(_13262, _13263, intervalFailed, optical_product_upper);
        Interval _13264 = _13692;
        Interval _13693 = jet_add_derivative(_13261, _13264, intervalFailed);
        Interval _13237 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13238 = direction.x.v;
        Interval _13698 = imul(_13237, _13238, intervalFailed, optical_product_upper);
        Interval _13239 = Interval{ 0.0, 0.0 };
        Interval _13240 = direction.x.v;
        Interval _13699 = jet_mul_derivative(_13239, _13240, intervalFailed, optical_product_upper);
        Interval _13241 = _13699;
        Interval _13242 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13243 = direction.x.dx;
        Interval _13701 = jet_mul_derivative(_13242, _13243, intervalFailed, optical_product_upper);
        Interval _13244 = _13701;
        Interval _13702 = jet_add_derivative(_13241, _13244, intervalFailed);
        Interval _13245 = Interval{ 0.0, 0.0 };
        Interval _13246 = direction.x.v;
        Interval _13703 = jet_mul_derivative(_13245, _13246, intervalFailed, optical_product_upper);
        Interval _13247 = _13703;
        Interval _13248 = Interval{ f.rotation0.z, f.rotation0.z };
        Interval _13249 = direction.x.dy;
        Interval _13705 = jet_mul_derivative(_13248, _13249, intervalFailed, optical_product_upper);
        Interval _13250 = _13705;
        Interval _13706 = jet_add_derivative(_13247, _13250, intervalFailed);
        Interval _13223 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13224 = direction.y.v;
        Interval _13714 = imul(_13223, _13224, intervalFailed, optical_product_upper);
        Interval _13225 = Interval{ 0.0, 0.0 };
        Interval _13226 = direction.y.v;
        Interval _13715 = jet_mul_derivative(_13225, _13226, intervalFailed, optical_product_upper);
        Interval _13227 = _13715;
        Interval _13228 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13229 = direction.y.dx;
        Interval _13717 = jet_mul_derivative(_13228, _13229, intervalFailed, optical_product_upper);
        Interval _13230 = _13717;
        Interval _13718 = jet_add_derivative(_13227, _13230, intervalFailed);
        Interval _13231 = Interval{ 0.0, 0.0 };
        Interval _13232 = direction.y.v;
        Interval _13719 = jet_mul_derivative(_13231, _13232, intervalFailed, optical_product_upper);
        Interval _13233 = _13719;
        Interval _13234 = Interval{ f.rotation1.x, f.rotation1.x };
        Interval _13235 = direction.y.dy;
        Interval _13721 = jet_mul_derivative(_13234, _13235, intervalFailed, optical_product_upper);
        Interval _13236 = _13721;
        Interval _13722 = jet_add_derivative(_13233, _13236, intervalFailed);
        Interval _13209 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13210 = direction.y.v;
        Interval _13727 = imul(_13209, _13210, intervalFailed, optical_product_upper);
        Interval _13211 = Interval{ 0.0, 0.0 };
        Interval _13212 = direction.y.v;
        Interval _13728 = jet_mul_derivative(_13211, _13212, intervalFailed, optical_product_upper);
        Interval _13213 = _13728;
        Interval _13214 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13215 = direction.y.dx;
        Interval _13730 = jet_mul_derivative(_13214, _13215, intervalFailed, optical_product_upper);
        Interval _13216 = _13730;
        Interval _13731 = jet_add_derivative(_13213, _13216, intervalFailed);
        Interval _13217 = Interval{ 0.0, 0.0 };
        Interval _13218 = direction.y.v;
        Interval _13732 = jet_mul_derivative(_13217, _13218, intervalFailed, optical_product_upper);
        Interval _13219 = _13732;
        Interval _13220 = Interval{ f.rotation1.y, f.rotation1.y };
        Interval _13221 = direction.y.dy;
        Interval _13734 = jet_mul_derivative(_13220, _13221, intervalFailed, optical_product_upper);
        Interval _13222 = _13734;
        Interval _13735 = jet_add_derivative(_13219, _13222, intervalFailed);
        Interval _13195 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13196 = direction.y.v;
        Interval _13740 = imul(_13195, _13196, intervalFailed, optical_product_upper);
        Interval _13197 = Interval{ 0.0, 0.0 };
        Interval _13198 = direction.y.v;
        Interval _13741 = jet_mul_derivative(_13197, _13198, intervalFailed, optical_product_upper);
        Interval _13199 = _13741;
        Interval _13200 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13201 = direction.y.dx;
        Interval _13743 = jet_mul_derivative(_13200, _13201, intervalFailed, optical_product_upper);
        Interval _13202 = _13743;
        Interval _13744 = jet_add_derivative(_13199, _13202, intervalFailed);
        Interval _13203 = Interval{ 0.0, 0.0 };
        Interval _13204 = direction.y.v;
        Interval _13745 = jet_mul_derivative(_13203, _13204, intervalFailed, optical_product_upper);
        Interval _13205 = _13745;
        Interval _13206 = Interval{ f.rotation1.z, f.rotation1.z };
        Interval _13207 = direction.y.dy;
        Interval _13747 = jet_mul_derivative(_13206, _13207, intervalFailed, optical_product_upper);
        Interval _13208 = _13747;
        Interval _13748 = jet_add_derivative(_13205, _13208, intervalFailed);
        Interval _13189 = _13672;
        Interval _13190 = _13714;
        Interval _13749 = iadd(_13189, _13190, intervalFailed);
        Interval _13191 = _13676;
        Interval _13192 = _13718;
        Interval _13750 = jet_add_derivative(_13191, _13192, intervalFailed);
        Interval _13193 = _13680;
        Interval _13194 = _13722;
        Interval _13751 = jet_add_derivative(_13193, _13194, intervalFailed);
        Interval _13183 = _13685;
        Interval _13184 = _13727;
        Interval _13752 = iadd(_13183, _13184, intervalFailed);
        Interval _13185 = _13689;
        Interval _13186 = _13731;
        Interval _13753 = jet_add_derivative(_13185, _13186, intervalFailed);
        Interval _13187 = _13693;
        Interval _13188 = _13735;
        Interval _13754 = jet_add_derivative(_13187, _13188, intervalFailed);
        Interval _13177 = _13698;
        Interval _13178 = _13740;
        Interval _13755 = iadd(_13177, _13178, intervalFailed);
        Interval _13179 = _13702;
        Interval _13180 = _13744;
        Interval _13756 = jet_add_derivative(_13179, _13180, intervalFailed);
        Interval _13181 = _13706;
        Interval _13182 = _13748;
        Interval _13757 = jet_add_derivative(_13181, _13182, intervalFailed);
        Interval _13163 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13164 = direction.z.v;
        Interval _13765 = imul(_13163, _13164, intervalFailed, optical_product_upper);
        Interval _13165 = Interval{ 0.0, 0.0 };
        Interval _13166 = direction.z.v;
        Interval _13766 = jet_mul_derivative(_13165, _13166, intervalFailed, optical_product_upper);
        Interval _13167 = _13766;
        Interval _13168 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13169 = direction.z.dx;
        Interval _13768 = jet_mul_derivative(_13168, _13169, intervalFailed, optical_product_upper);
        Interval _13170 = _13768;
        Interval _13769 = jet_add_derivative(_13167, _13170, intervalFailed);
        Interval _13171 = Interval{ 0.0, 0.0 };
        Interval _13172 = direction.z.v;
        Interval _13770 = jet_mul_derivative(_13171, _13172, intervalFailed, optical_product_upper);
        Interval _13173 = _13770;
        Interval _13174 = Interval{ f.rotation2.x, f.rotation2.x };
        Interval _13175 = direction.z.dy;
        Interval _13772 = jet_mul_derivative(_13174, _13175, intervalFailed, optical_product_upper);
        Interval _13176 = _13772;
        Interval _13773 = jet_add_derivative(_13173, _13176, intervalFailed);
        Interval _13149 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13150 = direction.z.v;
        Interval _13778 = imul(_13149, _13150, intervalFailed, optical_product_upper);
        Interval _13151 = Interval{ 0.0, 0.0 };
        Interval _13152 = direction.z.v;
        Interval _13779 = jet_mul_derivative(_13151, _13152, intervalFailed, optical_product_upper);
        Interval _13153 = _13779;
        Interval _13154 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13155 = direction.z.dx;
        Interval _13781 = jet_mul_derivative(_13154, _13155, intervalFailed, optical_product_upper);
        Interval _13156 = _13781;
        Interval _13782 = jet_add_derivative(_13153, _13156, intervalFailed);
        Interval _13157 = Interval{ 0.0, 0.0 };
        Interval _13158 = direction.z.v;
        Interval _13783 = jet_mul_derivative(_13157, _13158, intervalFailed, optical_product_upper);
        Interval _13159 = _13783;
        Interval _13160 = Interval{ f.rotation2.y, f.rotation2.y };
        Interval _13161 = direction.z.dy;
        Interval _13785 = jet_mul_derivative(_13160, _13161, intervalFailed, optical_product_upper);
        Interval _13162 = _13785;
        Interval _13786 = jet_add_derivative(_13159, _13162, intervalFailed);
        Interval _13135 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13136 = direction.z.v;
        Interval _13791 = imul(_13135, _13136, intervalFailed, optical_product_upper);
        Interval _13137 = Interval{ 0.0, 0.0 };
        Interval _13138 = direction.z.v;
        Interval _13792 = jet_mul_derivative(_13137, _13138, intervalFailed, optical_product_upper);
        Interval _13139 = _13792;
        Interval _13140 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13141 = direction.z.dx;
        Interval _13794 = jet_mul_derivative(_13140, _13141, intervalFailed, optical_product_upper);
        Interval _13142 = _13794;
        Interval _13795 = jet_add_derivative(_13139, _13142, intervalFailed);
        Interval _13143 = Interval{ 0.0, 0.0 };
        Interval _13144 = direction.z.v;
        Interval _13796 = jet_mul_derivative(_13143, _13144, intervalFailed, optical_product_upper);
        Interval _13145 = _13796;
        Interval _13146 = Interval{ f.rotation2.z, f.rotation2.z };
        Interval _13147 = direction.z.dy;
        Interval _13798 = jet_mul_derivative(_13146, _13147, intervalFailed, optical_product_upper);
        Interval _13148 = _13798;
        Interval _13799 = jet_add_derivative(_13145, _13148, intervalFailed);
        Interval _13129 = _13749;
        Interval _13130 = _13765;
        Interval _13800 = iadd(_13129, _13130, intervalFailed);
        Interval _13131 = _13750;
        Interval _13132 = _13769;
        Interval _13801 = jet_add_derivative(_13131, _13132, intervalFailed);
        Interval _13133 = _13751;
        Interval _13134 = _13773;
        Interval _13802 = jet_add_derivative(_13133, _13134, intervalFailed);
        Interval _13123 = _13752;
        Interval _13124 = _13778;
        Interval _13803 = iadd(_13123, _13124, intervalFailed);
        Interval _13125 = _13753;
        Interval _13126 = _13782;
        Interval _13804 = jet_add_derivative(_13125, _13126, intervalFailed);
        Interval _13127 = _13754;
        Interval _13128 = _13786;
        Interval _13805 = jet_add_derivative(_13127, _13128, intervalFailed);
        Interval _13117 = _13755;
        Interval _13118 = _13791;
        Interval _13806 = iadd(_13117, _13118, intervalFailed);
        Interval _13119 = _13756;
        Interval _13120 = _13795;
        Interval _13807 = jet_add_derivative(_13119, _13120, intervalFailed);
        Interval _13121 = _13757;
        Interval _13122 = _13799;
        Interval _13808 = jet_add_derivative(_13121, _13122, intervalFailed);
        float _13845;
        float _13847;
        float _13849;
        float _13851;
        float _13853;
        float _13855;
        float _13857;
        float _13859;
        float _13861;
        float _13863;
        float _13865;
        float _13867;
        _13845 = 0.0;
        _13847 = 0.0;
        _13849 = 0.0;
        _13851 = 0.0;
        _13853 = 0.0;
        _13855 = 0.0;
        _13857 = 0.0;
        _13859 = 0.0;
        _13861 = 0.0;
        _13863 = 0.0;
        _13865 = 0.0;
        _13867 = 0.0;
        float _13810;
        float _13812;
        float _13814;
        float _13816;
        float _13818;
        float _13820;
        float _13822;
        float _13824;
        float _13826;
        float _13828;
        float _13830;
        float _13832;
        float _13834;
        float _13836;
        float _13838;
        float _13840;
        float _13842;
        float _13844;
        float _13846;
        float _13848;
        float _13850;
        float _13852;
        float _13854;
        float _13856;
        float _13858;
        float _13860;
        float _13862;
        float _13864;
        float _13866;
        float _13868;
        float _13809 = _13625.lo;
        float _13811 = _13625.hi;
        float _13813 = _13626.lo;
        float _13815 = _13626.hi;
        float _13817 = _13627.lo;
        float _13819 = _13627.hi;
        float _13821 = _13629.lo;
        float _13823 = _13629.hi;
        float _13825 = _13630.lo;
        float _13827 = _13630.hi;
        float _13829 = _13631.lo;
        float _13831 = _13631.hi;
        float _13833 = _13621.lo;
        float _13835 = _13621.hi;
        float _13837 = _13622.lo;
        float _13839 = _13622.hi;
        float _13841 = _13623.lo;
        float _13843 = _13623.hi;
        uint _13869 = 0u;
        for (; _13869 < 2u; _13809 = _13810, _13811 = _13812, _13813 = _13814, _13815 = _13816, _13817 = _13818, _13819 = _13820, _13821 = _13822, _13823 = _13824, _13825 = _13826, _13827 = _13828, _13829 = _13830, _13831 = _13832, _13833 = _13834, _13835 = _13836, _13837 = _13838, _13839 = _13840, _13841 = _13842, _13843 = _13844, _13845 = _13846, _13847 = _13848, _13849 = _13850, _13851 = _13852, _13853 = _13854, _13855 = _13856, _13857 = _13858, _13859 = _13860, _13861 = _13862, _13863 = _13864, _13865 = _13866, _13867 = _13868, _13869++)
        {
            OpticalJet param_var_x = OpticalJet{ Interval{ _13833, _13835 }, Interval{ _13837, _13839 }, Interval{ _13841, _13843 } };
            OpticalJet param_var_z = OpticalJet{ Interval{ _13821, _13823 }, Interval{ _13825, _13827 }, Interval{ _13829, _13831 } };
            OpticalJet param_var_t = OpticalJet{ Interval{ f.settings.x, f.settings.x }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
            OpticalJet param_var_footprint = footprint;
            OpticalJet3 _13882 = jlava(param_var_x, param_var_z, param_var_t, param_var_footprint, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (_13869 == 0u)
            {
                float _13115 = 2.0;
                float _13116 = 10.0;
                Interval _13892 = iratio(_13115, _13116, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _13897;
                if (!intervalFailed)
                {
                    _13897 = intervalFailed;
                }
                else
                {
                    _13897 = false;
                }
                bool _13902;
                if (_13897)
                {
                    _13902 = jetFailureSite == 0u;
                }
                else
                {
                    _13902 = false;
                }
                if (_13902)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(2.0, 2.0, 10.0, 10.0);
                }
                Interval _13101 = _distance.v;
                Interval _13102 = _13892;
                Interval _13905 = imul(_13101, _13102, intervalFailed, optical_product_upper);
                Interval _13103 = _distance.dx;
                Interval _13104 = _13892;
                Interval _13906 = jet_mul_derivative(_13103, _13104, intervalFailed, optical_product_upper);
                Interval _13105 = _13906;
                Interval _13106 = _distance.v;
                Interval _13107 = Interval{ 0.0, 0.0 };
                Interval _13907 = jet_mul_derivative(_13106, _13107, intervalFailed, optical_product_upper);
                Interval _13108 = _13907;
                Interval _13908 = jet_add_derivative(_13105, _13108, intervalFailed);
                Interval _13109 = _distance.dy;
                Interval _13110 = _13892;
                Interval _13909 = jet_mul_derivative(_13109, _13110, intervalFailed, optical_product_upper);
                Interval _13111 = _13909;
                Interval _13112 = _distance.v;
                Interval _13113 = Interval{ 0.0, 0.0 };
                Interval _13910 = jet_mul_derivative(_13112, _13113, intervalFailed, optical_product_upper);
                Interval _13114 = _13910;
                Interval _13911 = jet_add_derivative(_13111, _13114, intervalFailed);
                float _13919 = as_type<float>(as_type<uint>(_13882.x.v.hi) ^ 2147483648u);
                float _13922 = as_type<float>(as_type<uint>(_13882.x.v.lo) ^ 2147483648u);
                OpticalJet param_var_a = OpticalJet{ _13803, _13804, _13805 };
                float _13099 = 12.0;
                float _13100 = 100.0;
                Interval _13941 = iratio(_13099, _13100, intervalFailed, optical_product_upper, interval_divide_upper);
                bool _13946;
                if (!intervalFailed)
                {
                    _13946 = intervalFailed;
                }
                else
                {
                    _13946 = false;
                }
                bool _13951;
                if (_13946)
                {
                    _13951 = jetFailureSite == 0u;
                }
                else
                {
                    _13951 = false;
                }
                if (_13951)
                {
                    jetFailureSite = 6u;
                    jetFailureArguments = float4(12.0, 12.0, 100.0, 100.0);
                }
                OpticalJet param_var_b = OpticalJet{ _13941, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
                OpticalJet _13955 = jmax(param_var_a, param_var_b);
                Interval _13956 = _13955.v;
                Interval _13085 = Interval{ _13919, _13922 };
                Interval _13086 = _13956;
                Interval _13961 = idiv(_13085, _13086, intervalFailed, interval_divide_upper);
                bool _13968;
                if (!intervalFailed)
                {
                    _13968 = intervalFailed;
                }
                else
                {
                    _13968 = false;
                }
                bool _13973;
                if (_13968)
                {
                    _13973 = jetFailureSite == 0u;
                }
                else
                {
                    _13973 = false;
                }
                if (_13973)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(_13919, _13922, _13956.lo, _13956.hi);
                }
                Interval _13087 = Interval{ as_type<float>(as_type<uint>(_13882.x.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13882.x.dx.lo) ^ 2147483648u) };
                Interval _13088 = _13961;
                Interval _13089 = _13955.dx;
                Interval _13978 = jet_mul_derivative(_13088, _13089, intervalFailed, optical_product_upper);
                Interval _13090 = Interval{ as_type<float>(as_type<uint>(_13978.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13978.lo) ^ 2147483648u) };
                Interval _13988 = jet_add_derivative(_13087, _13090, intervalFailed);
                Interval _13091 = _13988;
                Interval _13092 = _13956;
                Interval _13989 = jet_div_derivative(_13091, _13092, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _13093 = Interval{ as_type<float>(as_type<uint>(_13882.x.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13882.x.dy.lo) ^ 2147483648u) };
                Interval _13094 = _13961;
                Interval _13095 = _13955.dy;
                Interval _13991 = jet_mul_derivative(_13094, _13095, intervalFailed, optical_product_upper);
                Interval _13096 = Interval{ as_type<float>(as_type<uint>(_13991.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13991.lo) ^ 2147483648u) };
                Interval _14001 = jet_add_derivative(_13093, _13096, intervalFailed);
                Interval _13097 = _14001;
                Interval _13098 = _13956;
                Interval _14002 = jet_div_derivative(_13097, _13098, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                OpticalJet _13081 = OpticalJet{ _13961, _13989, _14002 };
                OpticalJet _13082 = OpticalJet{ Interval{ as_type<float>(as_type<uint>(_13905.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13905.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(_13908.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13908.lo) ^ 2147483648u) }, Interval{ as_type<float>(as_type<uint>(_13911.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_13911.lo) ^ 2147483648u) } };
                OpticalJet _13083 = jmax(_13081, _13082);
                OpticalJet _13084 = OpticalJet{ _13905, _13908, _13911 };
                OpticalJet _14034 = jmin(_13083, _13084);
                Interval _14035 = _14034.v;
                Interval _13067 = _13800;
                Interval _13068 = _14035;
                Interval _14038 = imul(_13067, _13068, intervalFailed, optical_product_upper);
                Interval _13069 = _13801;
                Interval _13070 = _14035;
                Interval _14039 = jet_mul_derivative(_13069, _13070, intervalFailed, optical_product_upper);
                Interval _13071 = _14039;
                Interval _13072 = _13800;
                Interval _13073 = _14034.dx;
                Interval _14040 = jet_mul_derivative(_13072, _13073, intervalFailed, optical_product_upper);
                Interval _13074 = _14040;
                Interval _14041 = jet_add_derivative(_13071, _13074, intervalFailed);
                Interval _13075 = _13802;
                Interval _13076 = _14035;
                Interval _14042 = jet_mul_derivative(_13075, _13076, intervalFailed, optical_product_upper);
                Interval _13077 = _14042;
                Interval _13078 = _13800;
                Interval _13079 = _14034.dy;
                Interval _14043 = jet_mul_derivative(_13078, _13079, intervalFailed, optical_product_upper);
                Interval _13080 = _14043;
                Interval _14044 = jet_add_derivative(_13077, _13080, intervalFailed);
                Interval _14045 = _14034.v;
                Interval _13053 = _13803;
                Interval _13054 = _14045;
                Interval _14048 = imul(_13053, _13054, intervalFailed, optical_product_upper);
                Interval _13055 = _13804;
                Interval _13056 = _14045;
                Interval _14049 = jet_mul_derivative(_13055, _13056, intervalFailed, optical_product_upper);
                Interval _13057 = _14049;
                Interval _13058 = _13803;
                Interval _13059 = _14034.dx;
                Interval _14050 = jet_mul_derivative(_13058, _13059, intervalFailed, optical_product_upper);
                Interval _13060 = _14050;
                Interval _14051 = jet_add_derivative(_13057, _13060, intervalFailed);
                Interval _13061 = _13805;
                Interval _13062 = _14045;
                Interval _14052 = jet_mul_derivative(_13061, _13062, intervalFailed, optical_product_upper);
                Interval _13063 = _14052;
                Interval _13064 = _13803;
                Interval _13065 = _14034.dy;
                Interval _14053 = jet_mul_derivative(_13064, _13065, intervalFailed, optical_product_upper);
                Interval _13066 = _14053;
                Interval _14054 = jet_add_derivative(_13063, _13066, intervalFailed);
                Interval _14055 = _14034.v;
                Interval _13039 = _13806;
                Interval _13040 = _14055;
                Interval _14058 = imul(_13039, _13040, intervalFailed, optical_product_upper);
                Interval _13041 = _13807;
                Interval _13042 = _14055;
                Interval _14059 = jet_mul_derivative(_13041, _13042, intervalFailed, optical_product_upper);
                Interval _13043 = _14059;
                Interval _13044 = _13806;
                Interval _13045 = _14034.dx;
                Interval _14060 = jet_mul_derivative(_13044, _13045, intervalFailed, optical_product_upper);
                Interval _13046 = _14060;
                Interval _14061 = jet_add_derivative(_13043, _13046, intervalFailed);
                Interval _13047 = _13808;
                Interval _13048 = _14055;
                Interval _14062 = jet_mul_derivative(_13047, _13048, intervalFailed, optical_product_upper);
                Interval _13049 = _14062;
                Interval _13050 = _13806;
                Interval _13051 = _14034.dy;
                Interval _14063 = jet_mul_derivative(_13050, _13051, intervalFailed, optical_product_upper);
                Interval _13052 = _14063;
                Interval _14064 = jet_add_derivative(_13049, _13052, intervalFailed);
                Interval _13033 = Interval{ _13833, _13835 };
                Interval _13034 = _14038;
                Interval _14066 = iadd(_13033, _13034, intervalFailed);
                Interval _13035 = Interval{ _13837, _13839 };
                Interval _13036 = _14041;
                Interval _14068 = jet_add_derivative(_13035, _13036, intervalFailed);
                Interval _13037 = Interval{ _13841, _13843 };
                Interval _13038 = _14044;
                Interval _14070 = jet_add_derivative(_13037, _13038, intervalFailed);
                Interval _13027 = Interval{ _13809, _13811 };
                Interval _13028 = _14048;
                Interval _14072 = iadd(_13027, _13028, intervalFailed);
                Interval _13029 = Interval{ _13813, _13815 };
                Interval _13030 = _14051;
                Interval _14074 = jet_add_derivative(_13029, _13030, intervalFailed);
                Interval _13031 = Interval{ _13817, _13819 };
                Interval _13032 = _14054;
                Interval _14076 = jet_add_derivative(_13031, _13032, intervalFailed);
                Interval _13021 = Interval{ _13821, _13823 };
                Interval _13022 = _14058;
                Interval _14078 = iadd(_13021, _13022, intervalFailed);
                Interval _13023 = Interval{ _13825, _13827 };
                Interval _13024 = _14061;
                Interval _14080 = jet_add_derivative(_13023, _13024, intervalFailed);
                Interval _13025 = Interval{ _13829, _13831 };
                Interval _13026 = _14064;
                Interval _14082 = jet_add_derivative(_13025, _13026, intervalFailed);
                Interval _14112 = _14034.v;
                Interval _13007 = direction.x.v;
                Interval _13008 = _14112;
                Interval _14115 = imul(_13007, _13008, intervalFailed, optical_product_upper);
                Interval _13009 = direction.x.dx;
                Interval _13010 = _14112;
                Interval _14116 = jet_mul_derivative(_13009, _13010, intervalFailed, optical_product_upper);
                Interval _13011 = _14116;
                Interval _13012 = direction.x.v;
                Interval _13013 = _14034.dx;
                Interval _14117 = jet_mul_derivative(_13012, _13013, intervalFailed, optical_product_upper);
                Interval _13014 = _14117;
                Interval _14118 = jet_add_derivative(_13011, _13014, intervalFailed);
                Interval _13015 = direction.x.dy;
                Interval _13016 = _14112;
                Interval _14119 = jet_mul_derivative(_13015, _13016, intervalFailed, optical_product_upper);
                Interval _13017 = _14119;
                Interval _13018 = direction.x.v;
                Interval _13019 = _14034.dy;
                Interval _14120 = jet_mul_derivative(_13018, _13019, intervalFailed, optical_product_upper);
                Interval _13020 = _14120;
                Interval _14121 = jet_add_derivative(_13017, _13020, intervalFailed);
                Interval _14125 = _14034.v;
                Interval _12993 = direction.y.v;
                Interval _12994 = _14125;
                Interval _14128 = imul(_12993, _12994, intervalFailed, optical_product_upper);
                Interval _12995 = direction.y.dx;
                Interval _12996 = _14125;
                Interval _14129 = jet_mul_derivative(_12995, _12996, intervalFailed, optical_product_upper);
                Interval _12997 = _14129;
                Interval _12998 = direction.y.v;
                Interval _12999 = _14034.dx;
                Interval _14130 = jet_mul_derivative(_12998, _12999, intervalFailed, optical_product_upper);
                Interval _13000 = _14130;
                Interval _14131 = jet_add_derivative(_12997, _13000, intervalFailed);
                Interval _13001 = direction.y.dy;
                Interval _13002 = _14125;
                Interval _14132 = jet_mul_derivative(_13001, _13002, intervalFailed, optical_product_upper);
                Interval _13003 = _14132;
                Interval _13004 = direction.y.v;
                Interval _13005 = _14034.dy;
                Interval _14133 = jet_mul_derivative(_13004, _13005, intervalFailed, optical_product_upper);
                Interval _13006 = _14133;
                Interval _14134 = jet_add_derivative(_13003, _13006, intervalFailed);
                Interval _14138 = _14034.v;
                Interval _12979 = direction.z.v;
                Interval _12980 = _14138;
                Interval _14141 = imul(_12979, _12980, intervalFailed, optical_product_upper);
                Interval _12981 = direction.z.dx;
                Interval _12982 = _14138;
                Interval _14142 = jet_mul_derivative(_12981, _12982, intervalFailed, optical_product_upper);
                Interval _12983 = _14142;
                Interval _12984 = direction.z.v;
                Interval _12985 = _14034.dx;
                Interval _14143 = jet_mul_derivative(_12984, _12985, intervalFailed, optical_product_upper);
                Interval _12986 = _14143;
                Interval _14144 = jet_add_derivative(_12983, _12986, intervalFailed);
                Interval _12987 = direction.z.dy;
                Interval _12988 = _14138;
                Interval _14145 = jet_mul_derivative(_12987, _12988, intervalFailed, optical_product_upper);
                Interval _12989 = _14145;
                Interval _12990 = direction.z.v;
                Interval _12991 = _14034.dy;
                Interval _14146 = jet_mul_derivative(_12990, _12991, intervalFailed, optical_product_upper);
                Interval _12992 = _14146;
                Interval _14147 = jet_add_derivative(_12989, _12992, intervalFailed);
                Interval _12973 = hit.x.v;
                Interval _12974 = _14115;
                Interval _14151 = iadd(_12973, _12974, intervalFailed);
                Interval _12975 = hit.x.dx;
                Interval _12976 = _14118;
                Interval _14152 = jet_add_derivative(_12975, _12976, intervalFailed);
                Interval _12977 = hit.x.dy;
                Interval _12978 = _14121;
                Interval _14153 = jet_add_derivative(_12977, _12978, intervalFailed);
                Interval _12967 = hit.y.v;
                Interval _12968 = _14128;
                Interval _14157 = iadd(_12967, _12968, intervalFailed);
                Interval _12969 = hit.y.dx;
                Interval _12970 = _14131;
                Interval _14158 = jet_add_derivative(_12969, _12970, intervalFailed);
                Interval _12971 = hit.y.dy;
                Interval _12972 = _14134;
                Interval _14159 = jet_add_derivative(_12971, _12972, intervalFailed);
                Interval _12961 = hit.z.v;
                Interval _12962 = _14141;
                Interval _14163 = iadd(_12961, _12962, intervalFailed);
                Interval _12963 = hit.z.dx;
                Interval _12964 = _14144;
                Interval _14164 = jet_add_derivative(_12963, _12964, intervalFailed);
                Interval _12965 = hit.z.dy;
                Interval _12966 = _14147;
                Interval _14165 = jet_add_derivative(_12965, _12966, intervalFailed);
                hit = OpticalJet3{ OpticalJet{ _14151, _14152, _14153 }, OpticalJet{ _14157, _14158, _14159 }, OpticalJet{ _14163, _14164, _14165 } };
                _13810 = _14072.lo;
                _13812 = _14072.hi;
                _13814 = _14074.lo;
                _13816 = _14074.hi;
                _13818 = _14076.lo;
                _13820 = _14076.hi;
                _13822 = _14078.lo;
                _13824 = _14078.hi;
                _13826 = _14080.lo;
                _13828 = _14080.hi;
                _13830 = _14082.lo;
                _13832 = _14082.hi;
                _13834 = _14066.lo;
                _13836 = _14066.hi;
                _13838 = _14068.lo;
                _13840 = _14068.hi;
                _13842 = _14070.lo;
                _13844 = _14070.hi;
                _13846 = _13845;
                _13848 = _13847;
                _13850 = _13849;
                _13852 = _13851;
                _13854 = _13853;
                _13856 = _13855;
                _13858 = _13857;
                _13860 = _13859;
                _13862 = _13861;
                _13864 = _13863;
                _13866 = _13865;
                _13868 = _13867;
            }
            else
            {
                _13810 = _13809;
                _13812 = _13811;
                _13814 = _13813;
                _13816 = _13815;
                _13818 = _13817;
                _13820 = _13819;
                _13822 = _13821;
                _13824 = _13823;
                _13826 = _13825;
                _13828 = _13827;
                _13830 = _13829;
                _13832 = _13831;
                _13834 = _13833;
                _13836 = _13835;
                _13838 = _13837;
                _13840 = _13839;
                _13842 = _13841;
                _13844 = _13843;
                _13846 = _13882.z.v.lo;
                _13848 = _13882.z.v.hi;
                _13850 = _13882.z.dx.lo;
                _13852 = _13882.z.dx.hi;
                _13854 = _13882.z.dy.lo;
                _13856 = _13882.z.dy.hi;
                _13858 = _13882.y.v.lo;
                _13860 = _13882.y.v.hi;
                _13862 = _13882.y.dx.lo;
                _13864 = _13882.y.dx.hi;
                _13866 = _13882.y.dy.lo;
                _13868 = _13882.y.dy.hi;
            }
        }
        _15245 = _13845;
        _15246 = _13847;
        _15247 = _13849;
        _15248 = _13851;
        _15249 = _13853;
        _15250 = _13855;
        _15251 = _13857;
        _15252 = _13859;
        _15253 = _13861;
        _15254 = _13863;
        _15255 = _13865;
        _15256 = _13867;
    }
    else
    {
        float _12959 = 18.0;
        float _12960 = 1000.0;
        Interval _14189 = iratio(_12959, _12960, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14194;
        if (!intervalFailed)
        {
            _14194 = intervalFailed;
        }
        else
        {
            _14194 = false;
        }
        bool _14199;
        if (_14194)
        {
            _14199 = jetFailureSite == 0u;
        }
        else
        {
            _14199 = false;
        }
        if (_14199)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(18.0, 18.0, 1000.0, 1000.0);
        }
        Interval _12945 = _13621;
        Interval _12946 = _14189;
        Interval _14202 = imul(_12945, _12946, intervalFailed, optical_product_upper);
        Interval _12947 = _13622;
        Interval _12948 = _14189;
        Interval _14203 = jet_mul_derivative(_12947, _12948, intervalFailed, optical_product_upper);
        Interval _12949 = _14203;
        Interval _12950 = _13621;
        Interval _12951 = Interval{ 0.0, 0.0 };
        Interval _14204 = jet_mul_derivative(_12950, _12951, intervalFailed, optical_product_upper);
        Interval _12952 = _14204;
        Interval _14205 = jet_add_derivative(_12949, _12952, intervalFailed);
        Interval _12953 = _13623;
        Interval _12954 = _14189;
        Interval _14206 = jet_mul_derivative(_12953, _12954, intervalFailed, optical_product_upper);
        Interval _12955 = _14206;
        Interval _12956 = _13621;
        Interval _12957 = Interval{ 0.0, 0.0 };
        Interval _14207 = jet_mul_derivative(_12956, _12957, intervalFailed, optical_product_upper);
        Interval _12958 = _14207;
        Interval _14208 = jet_add_derivative(_12955, _12958, intervalFailed);
        float _12943 = 11.0;
        float _12944 = 1000.0;
        Interval _14210 = iratio(_12943, _12944, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14215;
        if (!intervalFailed)
        {
            _14215 = intervalFailed;
        }
        else
        {
            _14215 = false;
        }
        bool _14220;
        if (_14215)
        {
            _14220 = jetFailureSite == 0u;
        }
        else
        {
            _14220 = false;
        }
        if (_14220)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(11.0, 11.0, 1000.0, 1000.0);
        }
        Interval _12929 = _13629;
        Interval _12930 = _14210;
        Interval _14223 = imul(_12929, _12930, intervalFailed, optical_product_upper);
        Interval _12931 = _13630;
        Interval _12932 = _14210;
        Interval _14224 = jet_mul_derivative(_12931, _12932, intervalFailed, optical_product_upper);
        Interval _12933 = _14224;
        Interval _12934 = _13629;
        Interval _12935 = Interval{ 0.0, 0.0 };
        Interval _14225 = jet_mul_derivative(_12934, _12935, intervalFailed, optical_product_upper);
        Interval _12936 = _14225;
        Interval _14226 = jet_add_derivative(_12933, _12936, intervalFailed);
        Interval _12937 = _13631;
        Interval _12938 = _14210;
        Interval _14227 = jet_mul_derivative(_12937, _12938, intervalFailed, optical_product_upper);
        Interval _12939 = _14227;
        Interval _12940 = _13629;
        Interval _12941 = Interval{ 0.0, 0.0 };
        Interval _14228 = jet_mul_derivative(_12940, _12941, intervalFailed, optical_product_upper);
        Interval _12942 = _14228;
        Interval _14229 = jet_add_derivative(_12939, _12942, intervalFailed);
        Interval _12923 = _14202;
        Interval _12924 = _14223;
        Interval _14230 = iadd(_12923, _12924, intervalFailed);
        Interval _12925 = _14205;
        Interval _12926 = _14226;
        Interval _14231 = jet_add_derivative(_12925, _12926, intervalFailed);
        Interval _12927 = _14208;
        Interval _12928 = _14229;
        Interval _14232 = jet_add_derivative(_12927, _12928, intervalFailed);
        float _12921 = 8.0;
        float _12922 = 10.0;
        Interval _14234 = iratio(_12921, _12922, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14239;
        if (!intervalFailed)
        {
            _14239 = intervalFailed;
        }
        else
        {
            _14239 = false;
        }
        bool _14244;
        if (_14239)
        {
            _14244 = jetFailureSite == 0u;
        }
        else
        {
            _14244 = false;
        }
        if (_14244)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(8.0, 8.0, 10.0, 10.0);
        }
        Interval _12907 = Interval{ f.settings.x, f.settings.x };
        Interval _12908 = _14234;
        Interval _14248 = imul(_12907, _12908, intervalFailed, optical_product_upper);
        Interval _12909 = Interval{ 0.0, 0.0 };
        Interval _12910 = _14234;
        Interval _14249 = jet_mul_derivative(_12909, _12910, intervalFailed, optical_product_upper);
        Interval _12911 = _14249;
        Interval _12912 = Interval{ f.settings.x, f.settings.x };
        Interval _12913 = Interval{ 0.0, 0.0 };
        Interval _14251 = jet_mul_derivative(_12912, _12913, intervalFailed, optical_product_upper);
        Interval _12914 = _14251;
        Interval _14252 = jet_add_derivative(_12911, _12914, intervalFailed);
        Interval _12915 = Interval{ 0.0, 0.0 };
        Interval _12916 = _14234;
        Interval _14253 = jet_mul_derivative(_12915, _12916, intervalFailed, optical_product_upper);
        Interval _12917 = _14253;
        Interval _12918 = Interval{ f.settings.x, f.settings.x };
        Interval _12919 = Interval{ 0.0, 0.0 };
        Interval _14255 = jet_mul_derivative(_12918, _12919, intervalFailed, optical_product_upper);
        Interval _12920 = _14255;
        Interval _14256 = jet_add_derivative(_12917, _12920, intervalFailed);
        Interval _12901 = _14230;
        Interval _12902 = Interval{ as_type<float>(as_type<uint>(_14248.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14248.lo) ^ 2147483648u) };
        Interval _14282 = iadd(_12901, _12902, intervalFailed);
        Interval _12903 = _14231;
        Interval _12904 = Interval{ as_type<float>(as_type<uint>(_14252.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14252.lo) ^ 2147483648u) };
        Interval _14284 = jet_add_derivative(_12903, _12904, intervalFailed);
        Interval _12905 = _14232;
        Interval _12906 = Interval{ as_type<float>(as_type<uint>(_14256.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14256.lo) ^ 2147483648u) };
        Interval _14286 = jet_add_derivative(_12905, _12906, intervalFailed);
        float _12899 = 47.0;
        float _12900 = 1000.0;
        Interval _14288 = iratio(_12899, _12900, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14293;
        if (!intervalFailed)
        {
            _14293 = intervalFailed;
        }
        else
        {
            _14293 = false;
        }
        bool _14298;
        if (_14293)
        {
            _14298 = jetFailureSite == 0u;
        }
        else
        {
            _14298 = false;
        }
        if (_14298)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(47.0, 47.0, 1000.0, 1000.0);
        }
        Interval _12885 = _13621;
        Interval _12886 = _14288;
        Interval _14301 = imul(_12885, _12886, intervalFailed, optical_product_upper);
        Interval _12887 = _13622;
        Interval _12888 = _14288;
        Interval _14302 = jet_mul_derivative(_12887, _12888, intervalFailed, optical_product_upper);
        Interval _12889 = _14302;
        Interval _12890 = _13621;
        Interval _12891 = Interval{ 0.0, 0.0 };
        Interval _14303 = jet_mul_derivative(_12890, _12891, intervalFailed, optical_product_upper);
        Interval _12892 = _14303;
        Interval _14304 = jet_add_derivative(_12889, _12892, intervalFailed);
        Interval _12893 = _13623;
        Interval _12894 = _14288;
        Interval _14305 = jet_mul_derivative(_12893, _12894, intervalFailed, optical_product_upper);
        Interval _12895 = _14305;
        Interval _12896 = _13621;
        Interval _12897 = Interval{ 0.0, 0.0 };
        Interval _14306 = jet_mul_derivative(_12896, _12897, intervalFailed, optical_product_upper);
        Interval _12898 = _14306;
        Interval _14307 = jet_add_derivative(_12895, _12898, intervalFailed);
        float _12883 = 25.0;
        float _12884 = 1000.0;
        Interval _14309 = iratio(_12883, _12884, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14314;
        if (!intervalFailed)
        {
            _14314 = intervalFailed;
        }
        else
        {
            _14314 = false;
        }
        bool _14319;
        if (_14314)
        {
            _14319 = jetFailureSite == 0u;
        }
        else
        {
            _14319 = false;
        }
        if (_14319)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
        }
        Interval _12869 = _13629;
        Interval _12870 = _14309;
        Interval _14322 = imul(_12869, _12870, intervalFailed, optical_product_upper);
        Interval _12871 = _13630;
        Interval _12872 = _14309;
        Interval _14323 = jet_mul_derivative(_12871, _12872, intervalFailed, optical_product_upper);
        Interval _12873 = _14323;
        Interval _12874 = _13629;
        Interval _12875 = Interval{ 0.0, 0.0 };
        Interval _14324 = jet_mul_derivative(_12874, _12875, intervalFailed, optical_product_upper);
        Interval _12876 = _14324;
        Interval _14325 = jet_add_derivative(_12873, _12876, intervalFailed);
        Interval _12877 = _13631;
        Interval _12878 = _14309;
        Interval _14326 = jet_mul_derivative(_12877, _12878, intervalFailed, optical_product_upper);
        Interval _12879 = _14326;
        Interval _12880 = _13629;
        Interval _12881 = Interval{ 0.0, 0.0 };
        Interval _14327 = jet_mul_derivative(_12880, _12881, intervalFailed, optical_product_upper);
        Interval _12882 = _14327;
        Interval _14328 = jet_add_derivative(_12879, _12882, intervalFailed);
        Interval _12863 = _14301;
        Interval _12864 = Interval{ as_type<float>(as_type<uint>(_14322.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14322.lo) ^ 2147483648u) };
        Interval _14354 = iadd(_12863, _12864, intervalFailed);
        Interval _12865 = _14304;
        Interval _12866 = Interval{ as_type<float>(as_type<uint>(_14325.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14325.lo) ^ 2147483648u) };
        Interval _14356 = jet_add_derivative(_12865, _12866, intervalFailed);
        Interval _12867 = _14307;
        Interval _12868 = Interval{ as_type<float>(as_type<uint>(_14328.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14328.lo) ^ 2147483648u) };
        Interval _14358 = jet_add_derivative(_12867, _12868, intervalFailed);
        float _12861 = 12.0;
        float _12862 = 10.0;
        Interval _14360 = iratio(_12861, _12862, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14365;
        if (!intervalFailed)
        {
            _14365 = intervalFailed;
        }
        else
        {
            _14365 = false;
        }
        bool _14370;
        if (_14365)
        {
            _14370 = jetFailureSite == 0u;
        }
        else
        {
            _14370 = false;
        }
        if (_14370)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(12.0, 12.0, 10.0, 10.0);
        }
        Interval _12847 = Interval{ f.settings.x, f.settings.x };
        Interval _12848 = _14360;
        Interval _14374 = imul(_12847, _12848, intervalFailed, optical_product_upper);
        Interval _12849 = Interval{ 0.0, 0.0 };
        Interval _12850 = _14360;
        Interval _14375 = jet_mul_derivative(_12849, _12850, intervalFailed, optical_product_upper);
        Interval _12851 = _14375;
        Interval _12852 = Interval{ f.settings.x, f.settings.x };
        Interval _12853 = Interval{ 0.0, 0.0 };
        Interval _14377 = jet_mul_derivative(_12852, _12853, intervalFailed, optical_product_upper);
        Interval _12854 = _14377;
        Interval _14378 = jet_add_derivative(_12851, _12854, intervalFailed);
        Interval _12855 = Interval{ 0.0, 0.0 };
        Interval _12856 = _14360;
        Interval _14379 = jet_mul_derivative(_12855, _12856, intervalFailed, optical_product_upper);
        Interval _12857 = _14379;
        Interval _12858 = Interval{ f.settings.x, f.settings.x };
        Interval _12859 = Interval{ 0.0, 0.0 };
        Interval _14381 = jet_mul_derivative(_12858, _12859, intervalFailed, optical_product_upper);
        Interval _12860 = _14381;
        Interval _14382 = jet_add_derivative(_12857, _12860, intervalFailed);
        Interval _12841 = _14354;
        Interval _12842 = _14374;
        Interval _14383 = iadd(_12841, _12842, intervalFailed);
        Interval _12843 = _14356;
        Interval _12844 = _14378;
        Interval _14384 = jet_add_derivative(_12843, _12844, intervalFailed);
        Interval _12845 = _14358;
        Interval _12846 = _14382;
        Interval _14385 = jet_add_derivative(_12845, _12846, intervalFailed);
        float _12839 = 22.0;
        float _12840 = 1000.0;
        Interval _14387 = iratio(_12839, _12840, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14392;
        if (!intervalFailed)
        {
            _14392 = intervalFailed;
        }
        else
        {
            _14392 = false;
        }
        bool _14397;
        if (_14392)
        {
            _14397 = jetFailureSite == 0u;
        }
        else
        {
            _14397 = false;
        }
        if (_14397)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
        }
        Interval _12825 = _13629;
        Interval _12826 = _14387;
        Interval _14400 = imul(_12825, _12826, intervalFailed, optical_product_upper);
        Interval _12827 = _13630;
        Interval _12828 = _14387;
        Interval _14401 = jet_mul_derivative(_12827, _12828, intervalFailed, optical_product_upper);
        Interval _12829 = _14401;
        Interval _12830 = _13629;
        Interval _12831 = Interval{ 0.0, 0.0 };
        Interval _14402 = jet_mul_derivative(_12830, _12831, intervalFailed, optical_product_upper);
        Interval _12832 = _14402;
        Interval _14403 = jet_add_derivative(_12829, _12832, intervalFailed);
        Interval _12833 = _13631;
        Interval _12834 = _14387;
        Interval _14404 = jet_mul_derivative(_12833, _12834, intervalFailed, optical_product_upper);
        Interval _12835 = _14404;
        Interval _12836 = _13629;
        Interval _12837 = Interval{ 0.0, 0.0 };
        Interval _14405 = jet_mul_derivative(_12836, _12837, intervalFailed, optical_product_upper);
        Interval _12838 = _14405;
        Interval _14406 = jet_add_derivative(_12835, _12838, intervalFailed);
        float _12823 = 9.0;
        float _12824 = 1000.0;
        Interval _14408 = iratio(_12823, _12824, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14413;
        if (!intervalFailed)
        {
            _14413 = intervalFailed;
        }
        else
        {
            _14413 = false;
        }
        bool _14418;
        if (_14413)
        {
            _14418 = jetFailureSite == 0u;
        }
        else
        {
            _14418 = false;
        }
        if (_14418)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(9.0, 9.0, 1000.0, 1000.0);
        }
        Interval _12809 = _13621;
        Interval _12810 = _14408;
        Interval _14421 = imul(_12809, _12810, intervalFailed, optical_product_upper);
        Interval _12811 = _13622;
        Interval _12812 = _14408;
        Interval _14422 = jet_mul_derivative(_12811, _12812, intervalFailed, optical_product_upper);
        Interval _12813 = _14422;
        Interval _12814 = _13621;
        Interval _12815 = Interval{ 0.0, 0.0 };
        Interval _14423 = jet_mul_derivative(_12814, _12815, intervalFailed, optical_product_upper);
        Interval _12816 = _14423;
        Interval _14424 = jet_add_derivative(_12813, _12816, intervalFailed);
        Interval _12817 = _13623;
        Interval _12818 = _14408;
        Interval _14425 = jet_mul_derivative(_12817, _12818, intervalFailed, optical_product_upper);
        Interval _12819 = _14425;
        Interval _12820 = _13621;
        Interval _12821 = Interval{ 0.0, 0.0 };
        Interval _14426 = jet_mul_derivative(_12820, _12821, intervalFailed, optical_product_upper);
        Interval _12822 = _14426;
        Interval _14427 = jet_add_derivative(_12819, _12822, intervalFailed);
        Interval _12803 = _14400;
        Interval _12804 = Interval{ as_type<float>(as_type<uint>(_14421.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14421.lo) ^ 2147483648u) };
        Interval _14453 = iadd(_12803, _12804, intervalFailed);
        Interval _12805 = _14403;
        Interval _12806 = Interval{ as_type<float>(as_type<uint>(_14424.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14424.lo) ^ 2147483648u) };
        Interval _14455 = jet_add_derivative(_12805, _12806, intervalFailed);
        Interval _12807 = _14406;
        Interval _12808 = Interval{ as_type<float>(as_type<uint>(_14427.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14427.lo) ^ 2147483648u) };
        Interval _14457 = jet_add_derivative(_12807, _12808, intervalFailed);
        float _12801 = 65.0;
        float _12802 = 100.0;
        Interval _14459 = iratio(_12801, _12802, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14464;
        if (!intervalFailed)
        {
            _14464 = intervalFailed;
        }
        else
        {
            _14464 = false;
        }
        bool _14469;
        if (_14464)
        {
            _14469 = jetFailureSite == 0u;
        }
        else
        {
            _14469 = false;
        }
        if (_14469)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(65.0, 65.0, 100.0, 100.0);
        }
        Interval _12787 = Interval{ f.settings.x, f.settings.x };
        Interval _12788 = _14459;
        Interval _14473 = imul(_12787, _12788, intervalFailed, optical_product_upper);
        Interval _12789 = Interval{ 0.0, 0.0 };
        Interval _12790 = _14459;
        Interval _14474 = jet_mul_derivative(_12789, _12790, intervalFailed, optical_product_upper);
        Interval _12791 = _14474;
        Interval _12792 = Interval{ f.settings.x, f.settings.x };
        Interval _12793 = Interval{ 0.0, 0.0 };
        Interval _14476 = jet_mul_derivative(_12792, _12793, intervalFailed, optical_product_upper);
        Interval _12794 = _14476;
        Interval _14477 = jet_add_derivative(_12791, _12794, intervalFailed);
        Interval _12795 = Interval{ 0.0, 0.0 };
        Interval _12796 = _14459;
        Interval _14478 = jet_mul_derivative(_12795, _12796, intervalFailed, optical_product_upper);
        Interval _12797 = _14478;
        Interval _12798 = Interval{ f.settings.x, f.settings.x };
        Interval _12799 = Interval{ 0.0, 0.0 };
        Interval _14480 = jet_mul_derivative(_12798, _12799, intervalFailed, optical_product_upper);
        Interval _12800 = _14480;
        Interval _14481 = jet_add_derivative(_12797, _12800, intervalFailed);
        Interval _12781 = _14453;
        Interval _12782 = Interval{ as_type<float>(as_type<uint>(_14473.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14473.lo) ^ 2147483648u) };
        Interval _14507 = iadd(_12781, _12782, intervalFailed);
        Interval _12783 = _14455;
        Interval _12784 = Interval{ as_type<float>(as_type<uint>(_14477.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14477.lo) ^ 2147483648u) };
        Interval _14509 = jet_add_derivative(_12783, _12784, intervalFailed);
        Interval _12785 = _14457;
        Interval _12786 = Interval{ as_type<float>(as_type<uint>(_14481.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14481.lo) ^ 2147483648u) };
        Interval _14511 = jet_add_derivative(_12785, _12786, intervalFailed);
        float _12779 = 55.0;
        float _12780 = 1000.0;
        Interval _14513 = iratio(_12779, _12780, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14518;
        if (!intervalFailed)
        {
            _14518 = intervalFailed;
        }
        else
        {
            _14518 = false;
        }
        bool _14523;
        if (_14518)
        {
            _14523 = jetFailureSite == 0u;
        }
        else
        {
            _14523 = false;
        }
        if (_14523)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(55.0, 55.0, 1000.0, 1000.0);
        }
        float _12773 = _14282.lo;
        float _12774 = _14282.hi;
        float _14528 = sine_bounds(_12773, _12774, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14532 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14535 = as_type<float>(as_type<uint>(_14528) ^ 2147483648u);
        Interval _12769 = _14282;
        float _12767 = 3.1415927410125732421875;
        float _14536 = interval_down(_12767, intervalFailed);
        float _12768 = 3.1415927410125732421875;
        float _14537 = interval_up(_12768, intervalFailed);
        Interval _12770 = Interval{ _14536, _14537 };
        Interval _12771 = Interval{ 0.5, 0.5 };
        Interval _14539 = imul(_12770, _12771, intervalFailed, optical_product_upper);
        Interval _12772 = _14539;
        Interval _14540 = iadd(_12769, _12772, intervalFailed);
        float _12765 = _14540.lo;
        float _12766 = _14540.hi;
        float _14543 = sine_bounds(_12765, _12766, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12775 = Interval{ _14532, _14535 };
        Interval _12776 = _14284;
        Interval _14546 = jet_mul_derivative(_12775, _12776, intervalFailed, optical_product_upper);
        Interval _12777 = Interval{ _14532, _14535 };
        Interval _12778 = _14286;
        Interval _14548 = jet_mul_derivative(_12777, _12778, intervalFailed, optical_product_upper);
        Interval _12751 = _14513;
        Interval _12752 = Interval{ _14543, interval_sine_upper };
        Interval _14550 = imul(_12751, _12752, intervalFailed, optical_product_upper);
        Interval _12753 = Interval{ 0.0, 0.0 };
        Interval _12754 = Interval{ _14543, interval_sine_upper };
        Interval _14552 = jet_mul_derivative(_12753, _12754, intervalFailed, optical_product_upper);
        Interval _12755 = _14552;
        Interval _12756 = _14513;
        Interval _12757 = _14546;
        Interval _14553 = jet_mul_derivative(_12756, _12757, intervalFailed, optical_product_upper);
        Interval _12758 = _14553;
        Interval _14554 = jet_add_derivative(_12755, _12758, intervalFailed);
        Interval _12759 = Interval{ 0.0, 0.0 };
        Interval _12760 = Interval{ _14543, interval_sine_upper };
        Interval _14556 = jet_mul_derivative(_12759, _12760, intervalFailed, optical_product_upper);
        Interval _12761 = _14556;
        Interval _12762 = _14513;
        Interval _12763 = _14548;
        Interval _14557 = jet_mul_derivative(_12762, _12763, intervalFailed, optical_product_upper);
        Interval _12764 = _14557;
        Interval _14558 = jet_add_derivative(_12761, _12764, intervalFailed);
        float _12749 = 22.0;
        float _12750 = 1000.0;
        Interval _14561 = iratio(_12749, _12750, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14566;
        if (!intervalFailed)
        {
            _14566 = intervalFailed;
        }
        else
        {
            _14566 = false;
        }
        bool _14571;
        if (_14566)
        {
            _14571 = jetFailureSite == 0u;
        }
        else
        {
            _14571 = false;
        }
        if (_14571)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(22.0, 22.0, 1000.0, 1000.0);
        }
        Interval _12735 = footprint.v;
        Interval _12736 = _14561;
        Interval _14577 = imul(_12735, _12736, intervalFailed, optical_product_upper);
        Interval _12737 = footprint.dx;
        Interval _12738 = _14561;
        Interval _14578 = jet_mul_derivative(_12737, _12738, intervalFailed, optical_product_upper);
        Interval _12739 = _14578;
        Interval _12740 = footprint.v;
        Interval _12741 = Interval{ 0.0, 0.0 };
        Interval _14579 = jet_mul_derivative(_12740, _12741, intervalFailed, optical_product_upper);
        Interval _12742 = _14579;
        Interval _14580 = jet_add_derivative(_12739, _12742, intervalFailed);
        Interval _12743 = footprint.dy;
        Interval _12744 = _14561;
        Interval _14581 = jet_mul_derivative(_12743, _12744, intervalFailed, optical_product_upper);
        Interval _12745 = _14581;
        Interval _12746 = footprint.v;
        Interval _12747 = Interval{ 0.0, 0.0 };
        Interval _14582 = jet_mul_derivative(_12746, _12747, intervalFailed, optical_product_upper);
        Interval _12748 = _14582;
        Interval _14583 = jet_add_derivative(_12745, _12748, intervalFailed);
        bool _14590;
        if (_14577.lo <= 0.0)
        {
            _14590 = _14577.hi >= 0.0;
        }
        else
        {
            _14590 = false;
        }
        float _14597;
        if (_14590)
        {
            _14597 = 0.0;
        }
        else
        {
            _14597 = precise::min(abs(_14577.lo), abs(_14577.hi));
        }
        float _14600 = precise::max(abs(_14577.lo), abs(_14577.hi));
        float _12725 = spvFMul(_14597, _14597);
        float _14601 = interval_down(_12725, intervalFailed);
        float _14602 = precise::max(0.0, _14601);
        float _12726 = spvFMul(_14600, _14600);
        float _14603 = interval_up(_12726, intervalFailed);
        Interval _12727 = Interval{ 2.0, 2.0 };
        Interval _12728 = _14577;
        Interval _14604 = imul(_12727, _12728, intervalFailed, optical_product_upper);
        Interval _12729 = _14604;
        Interval _12730 = _14580;
        Interval _14605 = jet_mul_derivative(_12729, _12730, intervalFailed, optical_product_upper);
        Interval _12731 = Interval{ 2.0, 2.0 };
        Interval _12732 = _14577;
        Interval _14606 = imul(_12731, _12732, intervalFailed, optical_product_upper);
        Interval _12733 = _14606;
        Interval _12734 = _14583;
        Interval _14607 = jet_mul_derivative(_12733, _12734, intervalFailed, optical_product_upper);
        bool _14612;
        if (_14602 <= 0.0)
        {
            _14612 = _14603 >= 0.0;
        }
        else
        {
            _14612 = false;
        }
        float _14619;
        if (_14612)
        {
            _14619 = 0.0;
        }
        else
        {
            _14619 = precise::min(abs(_14602), abs(_14603));
        }
        float _14622 = precise::max(abs(_14602), abs(_14603));
        float _12715 = spvFMul(_14619, _14619);
        float _14623 = interval_down(_12715, intervalFailed);
        float _12716 = spvFMul(_14622, _14622);
        float _14625 = interval_up(_12716, intervalFailed);
        Interval _12717 = Interval{ 2.0, 2.0 };
        Interval _12718 = Interval{ _14602, _14603 };
        Interval _14627 = imul(_12717, _12718, intervalFailed, optical_product_upper);
        Interval _12719 = _14627;
        Interval _12720 = _14605;
        Interval _14628 = jet_mul_derivative(_12719, _12720, intervalFailed, optical_product_upper);
        Interval _12721 = Interval{ 2.0, 2.0 };
        Interval _12722 = Interval{ _14602, _14603 };
        Interval _14630 = imul(_12721, _12722, intervalFailed, optical_product_upper);
        Interval _12723 = _14630;
        Interval _12724 = _14607;
        Interval _14631 = jet_mul_derivative(_12723, _12724, intervalFailed, optical_product_upper);
        Interval _12709 = Interval{ 1.0, 1.0 };
        Interval _12710 = Interval{ precise::max(0.0, _14623), _14625 };
        Interval _14633 = iadd(_12709, _12710, intervalFailed);
        Interval _12711 = Interval{ 0.0, 0.0 };
        Interval _12712 = _14628;
        Interval _14634 = jet_add_derivative(_12711, _12712, intervalFailed);
        Interval _12713 = Interval{ 0.0, 0.0 };
        Interval _12714 = _14631;
        Interval _14635 = jet_add_derivative(_12713, _12714, intervalFailed);
        Interval _12695 = Interval{ 1.0, 1.0 };
        Interval _12696 = _14633;
        Interval _14637 = idiv(_12695, _12696, intervalFailed, interval_divide_upper);
        bool _14644;
        if (!intervalFailed)
        {
            _14644 = intervalFailed;
        }
        else
        {
            _14644 = false;
        }
        bool _14649;
        if (_14644)
        {
            _14649 = jetFailureSite == 0u;
        }
        else
        {
            _14649 = false;
        }
        if (_14649)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14633.lo, _14633.hi);
        }
        Interval _12697 = Interval{ 0.0, 0.0 };
        Interval _12698 = _14637;
        Interval _12699 = _14634;
        Interval _14653 = jet_mul_derivative(_12698, _12699, intervalFailed, optical_product_upper);
        Interval _12700 = Interval{ as_type<float>(as_type<uint>(_14653.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14653.lo) ^ 2147483648u) };
        Interval _14663 = jet_add_derivative(_12697, _12700, intervalFailed);
        Interval _12701 = _14663;
        Interval _12702 = _14633;
        Interval _14664 = jet_div_derivative(_12701, _12702, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12703 = Interval{ 0.0, 0.0 };
        Interval _12704 = _14637;
        Interval _12705 = _14635;
        Interval _14665 = jet_mul_derivative(_12704, _12705, intervalFailed, optical_product_upper);
        Interval _12706 = Interval{ as_type<float>(as_type<uint>(_14665.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14665.lo) ^ 2147483648u) };
        Interval _14675 = jet_add_derivative(_12703, _12706, intervalFailed);
        Interval _12707 = _14675;
        Interval _12708 = _14633;
        Interval _14676 = jet_div_derivative(_12707, _12708, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12681 = _14550;
        Interval _12682 = _14637;
        Interval _14677 = imul(_12681, _12682, intervalFailed, optical_product_upper);
        Interval _12683 = _14554;
        Interval _12684 = _14637;
        Interval _14678 = jet_mul_derivative(_12683, _12684, intervalFailed, optical_product_upper);
        Interval _12685 = _14678;
        Interval _12686 = _14550;
        Interval _12687 = _14664;
        Interval _14679 = jet_mul_derivative(_12686, _12687, intervalFailed, optical_product_upper);
        Interval _12688 = _14679;
        Interval _14680 = jet_add_derivative(_12685, _12688, intervalFailed);
        Interval _12689 = _14558;
        Interval _12690 = _14637;
        Interval _14681 = jet_mul_derivative(_12689, _12690, intervalFailed, optical_product_upper);
        Interval _12691 = _14681;
        Interval _12692 = _14550;
        Interval _12693 = _14676;
        Interval _14682 = jet_mul_derivative(_12692, _12693, intervalFailed, optical_product_upper);
        Interval _12694 = _14682;
        Interval _14683 = jet_add_derivative(_12691, _12694, intervalFailed);
        float _12679 = 25.0;
        float _12680 = 1000.0;
        Interval _14685 = iratio(_12679, _12680, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14690;
        if (!intervalFailed)
        {
            _14690 = intervalFailed;
        }
        else
        {
            _14690 = false;
        }
        bool _14695;
        if (_14690)
        {
            _14695 = jetFailureSite == 0u;
        }
        else
        {
            _14695 = false;
        }
        if (_14695)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(25.0, 25.0, 1000.0, 1000.0);
        }
        float _12673 = _14383.lo;
        float _12674 = _14383.hi;
        float _14700 = sine_bounds(_12673, _12674, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14704 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14707 = as_type<float>(as_type<uint>(_14700) ^ 2147483648u);
        Interval _12669 = _14383;
        float _12667 = 3.1415927410125732421875;
        float _14708 = interval_down(_12667, intervalFailed);
        float _12668 = 3.1415927410125732421875;
        float _14709 = interval_up(_12668, intervalFailed);
        Interval _12670 = Interval{ _14708, _14709 };
        Interval _12671 = Interval{ 0.5, 0.5 };
        Interval _14711 = imul(_12670, _12671, intervalFailed, optical_product_upper);
        Interval _12672 = _14711;
        Interval _14712 = iadd(_12669, _12672, intervalFailed);
        float _12665 = _14712.lo;
        float _12666 = _14712.hi;
        float _14715 = sine_bounds(_12665, _12666, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12675 = Interval{ _14704, _14707 };
        Interval _12676 = _14384;
        Interval _14718 = jet_mul_derivative(_12675, _12676, intervalFailed, optical_product_upper);
        Interval _12677 = Interval{ _14704, _14707 };
        Interval _12678 = _14385;
        Interval _14720 = jet_mul_derivative(_12677, _12678, intervalFailed, optical_product_upper);
        Interval _12651 = _14685;
        Interval _12652 = Interval{ _14715, interval_sine_upper };
        Interval _14722 = imul(_12651, _12652, intervalFailed, optical_product_upper);
        Interval _12653 = Interval{ 0.0, 0.0 };
        Interval _12654 = Interval{ _14715, interval_sine_upper };
        Interval _14724 = jet_mul_derivative(_12653, _12654, intervalFailed, optical_product_upper);
        Interval _12655 = _14724;
        Interval _12656 = _14685;
        Interval _12657 = _14718;
        Interval _14725 = jet_mul_derivative(_12656, _12657, intervalFailed, optical_product_upper);
        Interval _12658 = _14725;
        Interval _14726 = jet_add_derivative(_12655, _12658, intervalFailed);
        Interval _12659 = Interval{ 0.0, 0.0 };
        Interval _12660 = Interval{ _14715, interval_sine_upper };
        Interval _14728 = jet_mul_derivative(_12659, _12660, intervalFailed, optical_product_upper);
        Interval _12661 = _14728;
        Interval _12662 = _14685;
        Interval _12663 = _14720;
        Interval _14729 = jet_mul_derivative(_12662, _12663, intervalFailed, optical_product_upper);
        Interval _12664 = _14729;
        Interval _14730 = jet_add_derivative(_12661, _12664, intervalFailed);
        float _12649 = 54.0;
        float _12650 = 1000.0;
        Interval _14733 = iratio(_12649, _12650, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14738;
        if (!intervalFailed)
        {
            _14738 = intervalFailed;
        }
        else
        {
            _14738 = false;
        }
        bool _14743;
        if (_14738)
        {
            _14743 = jetFailureSite == 0u;
        }
        else
        {
            _14743 = false;
        }
        if (_14743)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
        }
        Interval _12635 = footprint.v;
        Interval _12636 = _14733;
        Interval _14749 = imul(_12635, _12636, intervalFailed, optical_product_upper);
        Interval _12637 = footprint.dx;
        Interval _12638 = _14733;
        Interval _14750 = jet_mul_derivative(_12637, _12638, intervalFailed, optical_product_upper);
        Interval _12639 = _14750;
        Interval _12640 = footprint.v;
        Interval _12641 = Interval{ 0.0, 0.0 };
        Interval _14751 = jet_mul_derivative(_12640, _12641, intervalFailed, optical_product_upper);
        Interval _12642 = _14751;
        Interval _14752 = jet_add_derivative(_12639, _12642, intervalFailed);
        Interval _12643 = footprint.dy;
        Interval _12644 = _14733;
        Interval _14753 = jet_mul_derivative(_12643, _12644, intervalFailed, optical_product_upper);
        Interval _12645 = _14753;
        Interval _12646 = footprint.v;
        Interval _12647 = Interval{ 0.0, 0.0 };
        Interval _14754 = jet_mul_derivative(_12646, _12647, intervalFailed, optical_product_upper);
        Interval _12648 = _14754;
        Interval _14755 = jet_add_derivative(_12645, _12648, intervalFailed);
        bool _14762;
        if (_14749.lo <= 0.0)
        {
            _14762 = _14749.hi >= 0.0;
        }
        else
        {
            _14762 = false;
        }
        float _14769;
        if (_14762)
        {
            _14769 = 0.0;
        }
        else
        {
            _14769 = precise::min(abs(_14749.lo), abs(_14749.hi));
        }
        float _14772 = precise::max(abs(_14749.lo), abs(_14749.hi));
        float _12625 = spvFMul(_14769, _14769);
        float _14773 = interval_down(_12625, intervalFailed);
        float _14774 = precise::max(0.0, _14773);
        float _12626 = spvFMul(_14772, _14772);
        float _14775 = interval_up(_12626, intervalFailed);
        Interval _12627 = Interval{ 2.0, 2.0 };
        Interval _12628 = _14749;
        Interval _14776 = imul(_12627, _12628, intervalFailed, optical_product_upper);
        Interval _12629 = _14776;
        Interval _12630 = _14752;
        Interval _14777 = jet_mul_derivative(_12629, _12630, intervalFailed, optical_product_upper);
        Interval _12631 = Interval{ 2.0, 2.0 };
        Interval _12632 = _14749;
        Interval _14778 = imul(_12631, _12632, intervalFailed, optical_product_upper);
        Interval _12633 = _14778;
        Interval _12634 = _14755;
        Interval _14779 = jet_mul_derivative(_12633, _12634, intervalFailed, optical_product_upper);
        bool _14784;
        if (_14774 <= 0.0)
        {
            _14784 = _14775 >= 0.0;
        }
        else
        {
            _14784 = false;
        }
        float _14791;
        if (_14784)
        {
            _14791 = 0.0;
        }
        else
        {
            _14791 = precise::min(abs(_14774), abs(_14775));
        }
        float _14794 = precise::max(abs(_14774), abs(_14775));
        float _12615 = spvFMul(_14791, _14791);
        float _14795 = interval_down(_12615, intervalFailed);
        float _12616 = spvFMul(_14794, _14794);
        float _14797 = interval_up(_12616, intervalFailed);
        Interval _12617 = Interval{ 2.0, 2.0 };
        Interval _12618 = Interval{ _14774, _14775 };
        Interval _14799 = imul(_12617, _12618, intervalFailed, optical_product_upper);
        Interval _12619 = _14799;
        Interval _12620 = _14777;
        Interval _14800 = jet_mul_derivative(_12619, _12620, intervalFailed, optical_product_upper);
        Interval _12621 = Interval{ 2.0, 2.0 };
        Interval _12622 = Interval{ _14774, _14775 };
        Interval _14802 = imul(_12621, _12622, intervalFailed, optical_product_upper);
        Interval _12623 = _14802;
        Interval _12624 = _14779;
        Interval _14803 = jet_mul_derivative(_12623, _12624, intervalFailed, optical_product_upper);
        Interval _12609 = Interval{ 1.0, 1.0 };
        Interval _12610 = Interval{ precise::max(0.0, _14795), _14797 };
        Interval _14805 = iadd(_12609, _12610, intervalFailed);
        Interval _12611 = Interval{ 0.0, 0.0 };
        Interval _12612 = _14800;
        Interval _14806 = jet_add_derivative(_12611, _12612, intervalFailed);
        Interval _12613 = Interval{ 0.0, 0.0 };
        Interval _12614 = _14803;
        Interval _14807 = jet_add_derivative(_12613, _12614, intervalFailed);
        Interval _12595 = Interval{ 1.0, 1.0 };
        Interval _12596 = _14805;
        Interval _14809 = idiv(_12595, _12596, intervalFailed, interval_divide_upper);
        bool _14816;
        if (!intervalFailed)
        {
            _14816 = intervalFailed;
        }
        else
        {
            _14816 = false;
        }
        bool _14821;
        if (_14816)
        {
            _14821 = jetFailureSite == 0u;
        }
        else
        {
            _14821 = false;
        }
        if (_14821)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14805.lo, _14805.hi);
        }
        Interval _12597 = Interval{ 0.0, 0.0 };
        Interval _12598 = _14809;
        Interval _12599 = _14806;
        Interval _14825 = jet_mul_derivative(_12598, _12599, intervalFailed, optical_product_upper);
        Interval _12600 = Interval{ as_type<float>(as_type<uint>(_14825.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14825.lo) ^ 2147483648u) };
        Interval _14835 = jet_add_derivative(_12597, _12600, intervalFailed);
        Interval _12601 = _14835;
        Interval _12602 = _14805;
        Interval _14836 = jet_div_derivative(_12601, _12602, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12603 = Interval{ 0.0, 0.0 };
        Interval _12604 = _14809;
        Interval _12605 = _14807;
        Interval _14837 = jet_mul_derivative(_12604, _12605, intervalFailed, optical_product_upper);
        Interval _12606 = Interval{ as_type<float>(as_type<uint>(_14837.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_14837.lo) ^ 2147483648u) };
        Interval _14847 = jet_add_derivative(_12603, _12606, intervalFailed);
        Interval _12607 = _14847;
        Interval _12608 = _14805;
        Interval _14848 = jet_div_derivative(_12607, _12608, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12581 = _14722;
        Interval _12582 = _14809;
        Interval _14849 = imul(_12581, _12582, intervalFailed, optical_product_upper);
        Interval _12583 = _14726;
        Interval _12584 = _14809;
        Interval _14850 = jet_mul_derivative(_12583, _12584, intervalFailed, optical_product_upper);
        Interval _12585 = _14850;
        Interval _12586 = _14722;
        Interval _12587 = _14836;
        Interval _14851 = jet_mul_derivative(_12586, _12587, intervalFailed, optical_product_upper);
        Interval _12588 = _14851;
        Interval _14852 = jet_add_derivative(_12585, _12588, intervalFailed);
        Interval _12589 = _14730;
        Interval _12590 = _14809;
        Interval _14853 = jet_mul_derivative(_12589, _12590, intervalFailed, optical_product_upper);
        Interval _12591 = _14853;
        Interval _12592 = _14722;
        Interval _12593 = _14848;
        Interval _14854 = jet_mul_derivative(_12592, _12593, intervalFailed, optical_product_upper);
        Interval _12594 = _14854;
        Interval _14855 = jet_add_derivative(_12591, _12594, intervalFailed);
        Interval _12575 = _14677;
        Interval _12576 = _14849;
        Interval _14856 = iadd(_12575, _12576, intervalFailed);
        Interval _12577 = _14680;
        Interval _12578 = _14852;
        Interval _14857 = jet_add_derivative(_12577, _12578, intervalFailed);
        Interval _12579 = _14683;
        Interval _12580 = _14855;
        Interval _14858 = jet_add_derivative(_12579, _12580, intervalFailed);
        float _12573 = 45.0;
        float _12574 = 1000.0;
        Interval _14866 = iratio(_12573, _12574, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14871;
        if (!intervalFailed)
        {
            _14871 = intervalFailed;
        }
        else
        {
            _14871 = false;
        }
        bool _14876;
        if (_14871)
        {
            _14876 = jetFailureSite == 0u;
        }
        else
        {
            _14876 = false;
        }
        if (_14876)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(45.0, 45.0, 1000.0, 1000.0);
        }
        float _12567 = _14507.lo;
        float _12568 = _14507.hi;
        float _14881 = sine_bounds(_12567, _12568, intervalFailed, optical_product_upper, interval_sine_upper);
        float _14885 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _14888 = as_type<float>(as_type<uint>(_14881) ^ 2147483648u);
        Interval _12563 = _14507;
        float _12561 = 3.1415927410125732421875;
        float _14889 = interval_down(_12561, intervalFailed);
        float _12562 = 3.1415927410125732421875;
        float _14890 = interval_up(_12562, intervalFailed);
        Interval _12564 = Interval{ _14889, _14890 };
        Interval _12565 = Interval{ 0.5, 0.5 };
        Interval _14892 = imul(_12564, _12565, intervalFailed, optical_product_upper);
        Interval _12566 = _14892;
        Interval _14893 = iadd(_12563, _12566, intervalFailed);
        float _12559 = _14893.lo;
        float _12560 = _14893.hi;
        float _14896 = sine_bounds(_12559, _12560, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12569 = Interval{ _14885, _14888 };
        Interval _12570 = _14509;
        Interval _14899 = jet_mul_derivative(_12569, _12570, intervalFailed, optical_product_upper);
        Interval _12571 = Interval{ _14885, _14888 };
        Interval _12572 = _14511;
        Interval _14901 = jet_mul_derivative(_12571, _12572, intervalFailed, optical_product_upper);
        Interval _12545 = _14866;
        Interval _12546 = Interval{ _14896, interval_sine_upper };
        Interval _14903 = imul(_12545, _12546, intervalFailed, optical_product_upper);
        Interval _12547 = Interval{ 0.0, 0.0 };
        Interval _12548 = Interval{ _14896, interval_sine_upper };
        Interval _14905 = jet_mul_derivative(_12547, _12548, intervalFailed, optical_product_upper);
        Interval _12549 = _14905;
        Interval _12550 = _14866;
        Interval _12551 = _14899;
        Interval _14906 = jet_mul_derivative(_12550, _12551, intervalFailed, optical_product_upper);
        Interval _12552 = _14906;
        Interval _14907 = jet_add_derivative(_12549, _12552, intervalFailed);
        Interval _12553 = Interval{ 0.0, 0.0 };
        Interval _12554 = Interval{ _14896, interval_sine_upper };
        Interval _14909 = jet_mul_derivative(_12553, _12554, intervalFailed, optical_product_upper);
        Interval _12555 = _14909;
        Interval _12556 = _14866;
        Interval _12557 = _14901;
        Interval _14910 = jet_mul_derivative(_12556, _12557, intervalFailed, optical_product_upper);
        Interval _12558 = _14910;
        Interval _14911 = jet_add_derivative(_12555, _12558, intervalFailed);
        float _12543 = 24.0;
        float _12544 = 1000.0;
        Interval _14914 = iratio(_12543, _12544, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _14919;
        if (!intervalFailed)
        {
            _14919 = intervalFailed;
        }
        else
        {
            _14919 = false;
        }
        bool _14924;
        if (_14919)
        {
            _14924 = jetFailureSite == 0u;
        }
        else
        {
            _14924 = false;
        }
        if (_14924)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(24.0, 24.0, 1000.0, 1000.0);
        }
        Interval _12529 = footprint.v;
        Interval _12530 = _14914;
        Interval _14930 = imul(_12529, _12530, intervalFailed, optical_product_upper);
        Interval _12531 = footprint.dx;
        Interval _12532 = _14914;
        Interval _14931 = jet_mul_derivative(_12531, _12532, intervalFailed, optical_product_upper);
        Interval _12533 = _14931;
        Interval _12534 = footprint.v;
        Interval _12535 = Interval{ 0.0, 0.0 };
        Interval _14932 = jet_mul_derivative(_12534, _12535, intervalFailed, optical_product_upper);
        Interval _12536 = _14932;
        Interval _14933 = jet_add_derivative(_12533, _12536, intervalFailed);
        Interval _12537 = footprint.dy;
        Interval _12538 = _14914;
        Interval _14934 = jet_mul_derivative(_12537, _12538, intervalFailed, optical_product_upper);
        Interval _12539 = _14934;
        Interval _12540 = footprint.v;
        Interval _12541 = Interval{ 0.0, 0.0 };
        Interval _14935 = jet_mul_derivative(_12540, _12541, intervalFailed, optical_product_upper);
        Interval _12542 = _14935;
        Interval _14936 = jet_add_derivative(_12539, _12542, intervalFailed);
        bool _14943;
        if (_14930.lo <= 0.0)
        {
            _14943 = _14930.hi >= 0.0;
        }
        else
        {
            _14943 = false;
        }
        float _14950;
        if (_14943)
        {
            _14950 = 0.0;
        }
        else
        {
            _14950 = precise::min(abs(_14930.lo), abs(_14930.hi));
        }
        float _14953 = precise::max(abs(_14930.lo), abs(_14930.hi));
        float _12519 = spvFMul(_14950, _14950);
        float _14954 = interval_down(_12519, intervalFailed);
        float _14955 = precise::max(0.0, _14954);
        float _12520 = spvFMul(_14953, _14953);
        float _14956 = interval_up(_12520, intervalFailed);
        Interval _12521 = Interval{ 2.0, 2.0 };
        Interval _12522 = _14930;
        Interval _14957 = imul(_12521, _12522, intervalFailed, optical_product_upper);
        Interval _12523 = _14957;
        Interval _12524 = _14933;
        Interval _14958 = jet_mul_derivative(_12523, _12524, intervalFailed, optical_product_upper);
        Interval _12525 = Interval{ 2.0, 2.0 };
        Interval _12526 = _14930;
        Interval _14959 = imul(_12525, _12526, intervalFailed, optical_product_upper);
        Interval _12527 = _14959;
        Interval _12528 = _14936;
        Interval _14960 = jet_mul_derivative(_12527, _12528, intervalFailed, optical_product_upper);
        bool _14965;
        if (_14955 <= 0.0)
        {
            _14965 = _14956 >= 0.0;
        }
        else
        {
            _14965 = false;
        }
        float _14972;
        if (_14965)
        {
            _14972 = 0.0;
        }
        else
        {
            _14972 = precise::min(abs(_14955), abs(_14956));
        }
        float _14975 = precise::max(abs(_14955), abs(_14956));
        float _12509 = spvFMul(_14972, _14972);
        float _14976 = interval_down(_12509, intervalFailed);
        float _12510 = spvFMul(_14975, _14975);
        float _14978 = interval_up(_12510, intervalFailed);
        Interval _12511 = Interval{ 2.0, 2.0 };
        Interval _12512 = Interval{ _14955, _14956 };
        Interval _14980 = imul(_12511, _12512, intervalFailed, optical_product_upper);
        Interval _12513 = _14980;
        Interval _12514 = _14958;
        Interval _14981 = jet_mul_derivative(_12513, _12514, intervalFailed, optical_product_upper);
        Interval _12515 = Interval{ 2.0, 2.0 };
        Interval _12516 = Interval{ _14955, _14956 };
        Interval _14983 = imul(_12515, _12516, intervalFailed, optical_product_upper);
        Interval _12517 = _14983;
        Interval _12518 = _14960;
        Interval _14984 = jet_mul_derivative(_12517, _12518, intervalFailed, optical_product_upper);
        Interval _12503 = Interval{ 1.0, 1.0 };
        Interval _12504 = Interval{ precise::max(0.0, _14976), _14978 };
        Interval _14986 = iadd(_12503, _12504, intervalFailed);
        Interval _12505 = Interval{ 0.0, 0.0 };
        Interval _12506 = _14981;
        Interval _14987 = jet_add_derivative(_12505, _12506, intervalFailed);
        Interval _12507 = Interval{ 0.0, 0.0 };
        Interval _12508 = _14984;
        Interval _14988 = jet_add_derivative(_12507, _12508, intervalFailed);
        Interval _12489 = Interval{ 1.0, 1.0 };
        Interval _12490 = _14986;
        Interval _14990 = idiv(_12489, _12490, intervalFailed, interval_divide_upper);
        bool _14997;
        if (!intervalFailed)
        {
            _14997 = intervalFailed;
        }
        else
        {
            _14997 = false;
        }
        bool _15002;
        if (_14997)
        {
            _15002 = jetFailureSite == 0u;
        }
        else
        {
            _15002 = false;
        }
        if (_15002)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _14986.lo, _14986.hi);
        }
        Interval _12491 = Interval{ 0.0, 0.0 };
        Interval _12492 = _14990;
        Interval _12493 = _14987;
        Interval _15006 = jet_mul_derivative(_12492, _12493, intervalFailed, optical_product_upper);
        Interval _12494 = Interval{ as_type<float>(as_type<uint>(_15006.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15006.lo) ^ 2147483648u) };
        Interval _15016 = jet_add_derivative(_12491, _12494, intervalFailed);
        Interval _12495 = _15016;
        Interval _12496 = _14986;
        Interval _15017 = jet_div_derivative(_12495, _12496, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12497 = Interval{ 0.0, 0.0 };
        Interval _12498 = _14990;
        Interval _12499 = _14988;
        Interval _15018 = jet_mul_derivative(_12498, _12499, intervalFailed, optical_product_upper);
        Interval _12500 = Interval{ as_type<float>(as_type<uint>(_15018.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15018.lo) ^ 2147483648u) };
        Interval _15028 = jet_add_derivative(_12497, _12500, intervalFailed);
        Interval _12501 = _15028;
        Interval _12502 = _14986;
        Interval _15029 = jet_div_derivative(_12501, _12502, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12475 = _14903;
        Interval _12476 = _14990;
        Interval _15030 = imul(_12475, _12476, intervalFailed, optical_product_upper);
        Interval _12477 = _14907;
        Interval _12478 = _14990;
        Interval _15031 = jet_mul_derivative(_12477, _12478, intervalFailed, optical_product_upper);
        Interval _12479 = _15031;
        Interval _12480 = _14903;
        Interval _12481 = _15017;
        Interval _15032 = jet_mul_derivative(_12480, _12481, intervalFailed, optical_product_upper);
        Interval _12482 = _15032;
        Interval _15033 = jet_add_derivative(_12479, _12482, intervalFailed);
        Interval _12483 = _14911;
        Interval _12484 = _14990;
        Interval _15034 = jet_mul_derivative(_12483, _12484, intervalFailed, optical_product_upper);
        Interval _12485 = _15034;
        Interval _12486 = _14903;
        Interval _12487 = _15029;
        Interval _15035 = jet_mul_derivative(_12486, _12487, intervalFailed, optical_product_upper);
        Interval _12488 = _15035;
        Interval _15036 = jet_add_derivative(_12485, _12488, intervalFailed);
        float _12473 = 20.0;
        float _12474 = 1000.0;
        Interval _15038 = iratio(_12473, _12474, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _15043;
        if (!intervalFailed)
        {
            _15043 = intervalFailed;
        }
        else
        {
            _15043 = false;
        }
        bool _15048;
        if (_15043)
        {
            _15048 = jetFailureSite == 0u;
        }
        else
        {
            _15048 = false;
        }
        if (_15048)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(20.0, 20.0, 1000.0, 1000.0);
        }
        float _12467 = _14383.lo;
        float _12468 = _14383.hi;
        float _15053 = sine_bounds(_12467, _12468, intervalFailed, optical_product_upper, interval_sine_upper);
        float _15057 = as_type<float>(as_type<uint>(interval_sine_upper) ^ 2147483648u);
        float _15060 = as_type<float>(as_type<uint>(_15053) ^ 2147483648u);
        Interval _12463 = _14383;
        float _12461 = 3.1415927410125732421875;
        float _15061 = interval_down(_12461, intervalFailed);
        float _12462 = 3.1415927410125732421875;
        float _15062 = interval_up(_12462, intervalFailed);
        Interval _12464 = Interval{ _15061, _15062 };
        Interval _12465 = Interval{ 0.5, 0.5 };
        Interval _15064 = imul(_12464, _12465, intervalFailed, optical_product_upper);
        Interval _12466 = _15064;
        Interval _15065 = iadd(_12463, _12466, intervalFailed);
        float _12459 = _15065.lo;
        float _12460 = _15065.hi;
        float _15068 = sine_bounds(_12459, _12460, intervalFailed, optical_product_upper, interval_sine_upper);
        Interval _12469 = Interval{ _15057, _15060 };
        Interval _12470 = _14384;
        Interval _15071 = jet_mul_derivative(_12469, _12470, intervalFailed, optical_product_upper);
        Interval _12471 = Interval{ _15057, _15060 };
        Interval _12472 = _14385;
        Interval _15073 = jet_mul_derivative(_12471, _12472, intervalFailed, optical_product_upper);
        Interval _12445 = _15038;
        Interval _12446 = Interval{ _15068, interval_sine_upper };
        Interval _15075 = imul(_12445, _12446, intervalFailed, optical_product_upper);
        Interval _12447 = Interval{ 0.0, 0.0 };
        Interval _12448 = Interval{ _15068, interval_sine_upper };
        Interval _15077 = jet_mul_derivative(_12447, _12448, intervalFailed, optical_product_upper);
        Interval _12449 = _15077;
        Interval _12450 = _15038;
        Interval _12451 = _15071;
        Interval _15078 = jet_mul_derivative(_12450, _12451, intervalFailed, optical_product_upper);
        Interval _12452 = _15078;
        Interval _15079 = jet_add_derivative(_12449, _12452, intervalFailed);
        Interval _12453 = Interval{ 0.0, 0.0 };
        Interval _12454 = Interval{ _15068, interval_sine_upper };
        Interval _15081 = jet_mul_derivative(_12453, _12454, intervalFailed, optical_product_upper);
        Interval _12455 = _15081;
        Interval _12456 = _15038;
        Interval _12457 = _15073;
        Interval _15082 = jet_mul_derivative(_12456, _12457, intervalFailed, optical_product_upper);
        Interval _12458 = _15082;
        Interval _15083 = jet_add_derivative(_12455, _12458, intervalFailed);
        float _12443 = 54.0;
        float _12444 = 1000.0;
        Interval _15086 = iratio(_12443, _12444, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _15091;
        if (!intervalFailed)
        {
            _15091 = intervalFailed;
        }
        else
        {
            _15091 = false;
        }
        bool _15096;
        if (_15091)
        {
            _15096 = jetFailureSite == 0u;
        }
        else
        {
            _15096 = false;
        }
        if (_15096)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(54.0, 54.0, 1000.0, 1000.0);
        }
        Interval _12429 = footprint.v;
        Interval _12430 = _15086;
        Interval _15102 = imul(_12429, _12430, intervalFailed, optical_product_upper);
        Interval _12431 = footprint.dx;
        Interval _12432 = _15086;
        Interval _15103 = jet_mul_derivative(_12431, _12432, intervalFailed, optical_product_upper);
        Interval _12433 = _15103;
        Interval _12434 = footprint.v;
        Interval _12435 = Interval{ 0.0, 0.0 };
        Interval _15104 = jet_mul_derivative(_12434, _12435, intervalFailed, optical_product_upper);
        Interval _12436 = _15104;
        Interval _15105 = jet_add_derivative(_12433, _12436, intervalFailed);
        Interval _12437 = footprint.dy;
        Interval _12438 = _15086;
        Interval _15106 = jet_mul_derivative(_12437, _12438, intervalFailed, optical_product_upper);
        Interval _12439 = _15106;
        Interval _12440 = footprint.v;
        Interval _12441 = Interval{ 0.0, 0.0 };
        Interval _15107 = jet_mul_derivative(_12440, _12441, intervalFailed, optical_product_upper);
        Interval _12442 = _15107;
        Interval _15108 = jet_add_derivative(_12439, _12442, intervalFailed);
        bool _15115;
        if (_15102.lo <= 0.0)
        {
            _15115 = _15102.hi >= 0.0;
        }
        else
        {
            _15115 = false;
        }
        float _15122;
        if (_15115)
        {
            _15122 = 0.0;
        }
        else
        {
            _15122 = precise::min(abs(_15102.lo), abs(_15102.hi));
        }
        float _15125 = precise::max(abs(_15102.lo), abs(_15102.hi));
        float _12419 = spvFMul(_15122, _15122);
        float _15126 = interval_down(_12419, intervalFailed);
        float _15127 = precise::max(0.0, _15126);
        float _12420 = spvFMul(_15125, _15125);
        float _15128 = interval_up(_12420, intervalFailed);
        Interval _12421 = Interval{ 2.0, 2.0 };
        Interval _12422 = _15102;
        Interval _15129 = imul(_12421, _12422, intervalFailed, optical_product_upper);
        Interval _12423 = _15129;
        Interval _12424 = _15105;
        Interval _15130 = jet_mul_derivative(_12423, _12424, intervalFailed, optical_product_upper);
        Interval _12425 = Interval{ 2.0, 2.0 };
        Interval _12426 = _15102;
        Interval _15131 = imul(_12425, _12426, intervalFailed, optical_product_upper);
        Interval _12427 = _15131;
        Interval _12428 = _15108;
        Interval _15132 = jet_mul_derivative(_12427, _12428, intervalFailed, optical_product_upper);
        bool _15137;
        if (_15127 <= 0.0)
        {
            _15137 = _15128 >= 0.0;
        }
        else
        {
            _15137 = false;
        }
        float _15144;
        if (_15137)
        {
            _15144 = 0.0;
        }
        else
        {
            _15144 = precise::min(abs(_15127), abs(_15128));
        }
        float _15147 = precise::max(abs(_15127), abs(_15128));
        float _12409 = spvFMul(_15144, _15144);
        float _15148 = interval_down(_12409, intervalFailed);
        float _12410 = spvFMul(_15147, _15147);
        float _15150 = interval_up(_12410, intervalFailed);
        Interval _12411 = Interval{ 2.0, 2.0 };
        Interval _12412 = Interval{ _15127, _15128 };
        Interval _15152 = imul(_12411, _12412, intervalFailed, optical_product_upper);
        Interval _12413 = _15152;
        Interval _12414 = _15130;
        Interval _15153 = jet_mul_derivative(_12413, _12414, intervalFailed, optical_product_upper);
        Interval _12415 = Interval{ 2.0, 2.0 };
        Interval _12416 = Interval{ _15127, _15128 };
        Interval _15155 = imul(_12415, _12416, intervalFailed, optical_product_upper);
        Interval _12417 = _15155;
        Interval _12418 = _15132;
        Interval _15156 = jet_mul_derivative(_12417, _12418, intervalFailed, optical_product_upper);
        Interval _12403 = Interval{ 1.0, 1.0 };
        Interval _12404 = Interval{ precise::max(0.0, _15148), _15150 };
        Interval _15158 = iadd(_12403, _12404, intervalFailed);
        Interval _12405 = Interval{ 0.0, 0.0 };
        Interval _12406 = _15153;
        Interval _15159 = jet_add_derivative(_12405, _12406, intervalFailed);
        Interval _12407 = Interval{ 0.0, 0.0 };
        Interval _12408 = _15156;
        Interval _15160 = jet_add_derivative(_12407, _12408, intervalFailed);
        Interval _12389 = Interval{ 1.0, 1.0 };
        Interval _12390 = _15158;
        Interval _15162 = idiv(_12389, _12390, intervalFailed, interval_divide_upper);
        bool _15169;
        if (!intervalFailed)
        {
            _15169 = intervalFailed;
        }
        else
        {
            _15169 = false;
        }
        bool _15174;
        if (_15169)
        {
            _15174 = jetFailureSite == 0u;
        }
        else
        {
            _15174 = false;
        }
        if (_15174)
        {
            jetFailureSite = 1u;
            jetFailureArguments = float4(1.0, 1.0, _15158.lo, _15158.hi);
        }
        Interval _12391 = Interval{ 0.0, 0.0 };
        Interval _12392 = _15162;
        Interval _12393 = _15159;
        Interval _15178 = jet_mul_derivative(_12392, _12393, intervalFailed, optical_product_upper);
        Interval _12394 = Interval{ as_type<float>(as_type<uint>(_15178.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15178.lo) ^ 2147483648u) };
        Interval _15188 = jet_add_derivative(_12391, _12394, intervalFailed);
        Interval _12395 = _15188;
        Interval _12396 = _15158;
        Interval _15189 = jet_div_derivative(_12395, _12396, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12397 = Interval{ 0.0, 0.0 };
        Interval _12398 = _15162;
        Interval _12399 = _15160;
        Interval _15190 = jet_mul_derivative(_12398, _12399, intervalFailed, optical_product_upper);
        Interval _12400 = Interval{ as_type<float>(as_type<uint>(_15190.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15190.lo) ^ 2147483648u) };
        Interval _15200 = jet_add_derivative(_12397, _12400, intervalFailed);
        Interval _12401 = _15200;
        Interval _12402 = _15158;
        Interval _15201 = jet_div_derivative(_12401, _12402, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
        Interval _12375 = _15075;
        Interval _12376 = _15162;
        Interval _15202 = imul(_12375, _12376, intervalFailed, optical_product_upper);
        Interval _12377 = _15079;
        Interval _12378 = _15162;
        Interval _15203 = jet_mul_derivative(_12377, _12378, intervalFailed, optical_product_upper);
        Interval _12379 = _15203;
        Interval _12380 = _15075;
        Interval _12381 = _15189;
        Interval _15204 = jet_mul_derivative(_12380, _12381, intervalFailed, optical_product_upper);
        Interval _12382 = _15204;
        Interval _15205 = jet_add_derivative(_12379, _12382, intervalFailed);
        Interval _12383 = _15083;
        Interval _12384 = _15162;
        Interval _15206 = jet_mul_derivative(_12383, _12384, intervalFailed, optical_product_upper);
        Interval _12385 = _15206;
        Interval _12386 = _15075;
        Interval _12387 = _15201;
        Interval _15207 = jet_mul_derivative(_12386, _12387, intervalFailed, optical_product_upper);
        Interval _12388 = _15207;
        Interval _15208 = jet_add_derivative(_12385, _12388, intervalFailed);
        Interval _12369 = _15030;
        Interval _12370 = Interval{ as_type<float>(as_type<uint>(_15202.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15202.lo) ^ 2147483648u) };
        Interval _15234 = iadd(_12369, _12370, intervalFailed);
        Interval _12371 = _15033;
        Interval _12372 = Interval{ as_type<float>(as_type<uint>(_15205.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15205.lo) ^ 2147483648u) };
        Interval _15236 = jet_add_derivative(_12371, _12372, intervalFailed);
        Interval _12373 = _15036;
        Interval _12374 = Interval{ as_type<float>(as_type<uint>(_15208.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15208.lo) ^ 2147483648u) };
        Interval _15238 = jet_add_derivative(_12373, _12374, intervalFailed);
        _15245 = _15234.lo;
        _15246 = _15234.hi;
        _15247 = _15236.lo;
        _15248 = _15236.hi;
        _15249 = _15238.lo;
        _15250 = _15238.hi;
        _15251 = _14856.lo;
        _15252 = _14856.hi;
        _15253 = _14857.lo;
        _15254 = _14857.hi;
        _15255 = _14858.lo;
        _15256 = _14858.hi;
    }
    Interval _12355 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12356 = Interval{ _15251, _15252 };
    Interval _15266 = imul(_12355, _12356, intervalFailed, optical_product_upper);
    Interval _12357 = Interval{ 0.0, 0.0 };
    Interval _12358 = Interval{ _15251, _15252 };
    Interval _15268 = jet_mul_derivative(_12357, _12358, intervalFailed, optical_product_upper);
    Interval _12359 = _15268;
    Interval _12360 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12361 = Interval{ _15253, _15254 };
    Interval _15271 = jet_mul_derivative(_12360, _12361, intervalFailed, optical_product_upper);
    Interval _12362 = _15271;
    Interval _15272 = jet_add_derivative(_12359, _12362, intervalFailed);
    Interval _12363 = Interval{ 0.0, 0.0 };
    Interval _12364 = Interval{ _15251, _15252 };
    Interval _15274 = jet_mul_derivative(_12363, _12364, intervalFailed, optical_product_upper);
    Interval _12365 = _15274;
    Interval _12366 = Interval{ f.rotation0.x, f.rotation0.x };
    Interval _12367 = Interval{ _15255, _15256 };
    Interval _15277 = jet_mul_derivative(_12366, _12367, intervalFailed, optical_product_upper);
    Interval _12368 = _15277;
    Interval _15278 = jet_add_derivative(_12365, _12368, intervalFailed);
    Interval _12341 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12342 = Interval{ -1.0, -1.0 };
    Interval _15280 = imul(_12341, _12342, intervalFailed, optical_product_upper);
    Interval _12343 = Interval{ 0.0, 0.0 };
    Interval _12344 = Interval{ -1.0, -1.0 };
    Interval _15281 = jet_mul_derivative(_12343, _12344, intervalFailed, optical_product_upper);
    Interval _12345 = _15281;
    Interval _12346 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12347 = Interval{ 0.0, 0.0 };
    Interval _15283 = jet_mul_derivative(_12346, _12347, intervalFailed, optical_product_upper);
    Interval _12348 = _15283;
    Interval _15284 = jet_add_derivative(_12345, _12348, intervalFailed);
    Interval _12349 = Interval{ 0.0, 0.0 };
    Interval _12350 = Interval{ -1.0, -1.0 };
    Interval _15285 = jet_mul_derivative(_12349, _12350, intervalFailed, optical_product_upper);
    Interval _12351 = _15285;
    Interval _12352 = Interval{ f.rotation0.y, f.rotation0.y };
    Interval _12353 = Interval{ 0.0, 0.0 };
    Interval _15287 = jet_mul_derivative(_12352, _12353, intervalFailed, optical_product_upper);
    Interval _12354 = _15287;
    Interval _15288 = jet_add_derivative(_12351, _12354, intervalFailed);
    Interval _12335 = _15266;
    Interval _12336 = _15280;
    Interval _15289 = iadd(_12335, _12336, intervalFailed);
    Interval _12337 = _15272;
    Interval _12338 = _15284;
    Interval _15290 = jet_add_derivative(_12337, _12338, intervalFailed);
    Interval _12339 = _15278;
    Interval _12340 = _15288;
    Interval _15291 = jet_add_derivative(_12339, _12340, intervalFailed);
    Interval _12321 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12322 = Interval{ _15245, _15246 };
    Interval _15294 = imul(_12321, _12322, intervalFailed, optical_product_upper);
    Interval _12323 = Interval{ 0.0, 0.0 };
    Interval _12324 = Interval{ _15245, _15246 };
    Interval _15296 = jet_mul_derivative(_12323, _12324, intervalFailed, optical_product_upper);
    Interval _12325 = _15296;
    Interval _12326 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12327 = Interval{ _15247, _15248 };
    Interval _15299 = jet_mul_derivative(_12326, _12327, intervalFailed, optical_product_upper);
    Interval _12328 = _15299;
    Interval _15300 = jet_add_derivative(_12325, _12328, intervalFailed);
    Interval _12329 = Interval{ 0.0, 0.0 };
    Interval _12330 = Interval{ _15245, _15246 };
    Interval _15302 = jet_mul_derivative(_12329, _12330, intervalFailed, optical_product_upper);
    Interval _12331 = _15302;
    Interval _12332 = Interval{ f.rotation0.z, f.rotation0.z };
    Interval _12333 = Interval{ _15249, _15250 };
    Interval _15305 = jet_mul_derivative(_12332, _12333, intervalFailed, optical_product_upper);
    Interval _12334 = _15305;
    Interval _15306 = jet_add_derivative(_12331, _12334, intervalFailed);
    Interval _12315 = _15289;
    Interval _12316 = _15294;
    Interval _15307 = iadd(_12315, _12316, intervalFailed);
    Interval _12317 = _15290;
    Interval _12318 = _15300;
    Interval _15308 = jet_add_derivative(_12317, _12318, intervalFailed);
    Interval _12319 = _15291;
    Interval _12320 = _15306;
    Interval _15309 = jet_add_derivative(_12319, _12320, intervalFailed);
    Interval _12301 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12302 = Interval{ _15251, _15252 };
    Interval _15315 = imul(_12301, _12302, intervalFailed, optical_product_upper);
    Interval _12303 = Interval{ 0.0, 0.0 };
    Interval _12304 = Interval{ _15251, _15252 };
    Interval _15317 = jet_mul_derivative(_12303, _12304, intervalFailed, optical_product_upper);
    Interval _12305 = _15317;
    Interval _12306 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12307 = Interval{ _15253, _15254 };
    Interval _15320 = jet_mul_derivative(_12306, _12307, intervalFailed, optical_product_upper);
    Interval _12308 = _15320;
    Interval _15321 = jet_add_derivative(_12305, _12308, intervalFailed);
    Interval _12309 = Interval{ 0.0, 0.0 };
    Interval _12310 = Interval{ _15251, _15252 };
    Interval _15323 = jet_mul_derivative(_12309, _12310, intervalFailed, optical_product_upper);
    Interval _12311 = _15323;
    Interval _12312 = Interval{ f.rotation1.x, f.rotation1.x };
    Interval _12313 = Interval{ _15255, _15256 };
    Interval _15326 = jet_mul_derivative(_12312, _12313, intervalFailed, optical_product_upper);
    Interval _12314 = _15326;
    Interval _15327 = jet_add_derivative(_12311, _12314, intervalFailed);
    Interval _12287 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12288 = Interval{ -1.0, -1.0 };
    Interval _15329 = imul(_12287, _12288, intervalFailed, optical_product_upper);
    Interval _12289 = Interval{ 0.0, 0.0 };
    Interval _12290 = Interval{ -1.0, -1.0 };
    Interval _15330 = jet_mul_derivative(_12289, _12290, intervalFailed, optical_product_upper);
    Interval _12291 = _15330;
    Interval _12292 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12293 = Interval{ 0.0, 0.0 };
    Interval _15332 = jet_mul_derivative(_12292, _12293, intervalFailed, optical_product_upper);
    Interval _12294 = _15332;
    Interval _15333 = jet_add_derivative(_12291, _12294, intervalFailed);
    Interval _12295 = Interval{ 0.0, 0.0 };
    Interval _12296 = Interval{ -1.0, -1.0 };
    Interval _15334 = jet_mul_derivative(_12295, _12296, intervalFailed, optical_product_upper);
    Interval _12297 = _15334;
    Interval _12298 = Interval{ f.rotation1.y, f.rotation1.y };
    Interval _12299 = Interval{ 0.0, 0.0 };
    Interval _15336 = jet_mul_derivative(_12298, _12299, intervalFailed, optical_product_upper);
    Interval _12300 = _15336;
    Interval _15337 = jet_add_derivative(_12297, _12300, intervalFailed);
    Interval _12281 = _15315;
    Interval _12282 = _15329;
    Interval _15338 = iadd(_12281, _12282, intervalFailed);
    Interval _12283 = _15321;
    Interval _12284 = _15333;
    Interval _15339 = jet_add_derivative(_12283, _12284, intervalFailed);
    Interval _12285 = _15327;
    Interval _12286 = _15337;
    Interval _15340 = jet_add_derivative(_12285, _12286, intervalFailed);
    Interval _12267 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12268 = Interval{ _15245, _15246 };
    Interval _15343 = imul(_12267, _12268, intervalFailed, optical_product_upper);
    Interval _12269 = Interval{ 0.0, 0.0 };
    Interval _12270 = Interval{ _15245, _15246 };
    Interval _15345 = jet_mul_derivative(_12269, _12270, intervalFailed, optical_product_upper);
    Interval _12271 = _15345;
    Interval _12272 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12273 = Interval{ _15247, _15248 };
    Interval _15348 = jet_mul_derivative(_12272, _12273, intervalFailed, optical_product_upper);
    Interval _12274 = _15348;
    Interval _15349 = jet_add_derivative(_12271, _12274, intervalFailed);
    Interval _12275 = Interval{ 0.0, 0.0 };
    Interval _12276 = Interval{ _15245, _15246 };
    Interval _15351 = jet_mul_derivative(_12275, _12276, intervalFailed, optical_product_upper);
    Interval _12277 = _15351;
    Interval _12278 = Interval{ f.rotation1.z, f.rotation1.z };
    Interval _12279 = Interval{ _15249, _15250 };
    Interval _15354 = jet_mul_derivative(_12278, _12279, intervalFailed, optical_product_upper);
    Interval _12280 = _15354;
    Interval _15355 = jet_add_derivative(_12277, _12280, intervalFailed);
    Interval _12261 = _15338;
    Interval _12262 = _15343;
    Interval _15356 = iadd(_12261, _12262, intervalFailed);
    Interval _12263 = _15339;
    Interval _12264 = _15349;
    Interval _15357 = jet_add_derivative(_12263, _12264, intervalFailed);
    Interval _12265 = _15340;
    Interval _12266 = _15355;
    Interval _15358 = jet_add_derivative(_12265, _12266, intervalFailed);
    Interval _12247 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12248 = Interval{ _15251, _15252 };
    Interval _15364 = imul(_12247, _12248, intervalFailed, optical_product_upper);
    Interval _12249 = Interval{ 0.0, 0.0 };
    Interval _12250 = Interval{ _15251, _15252 };
    Interval _15366 = jet_mul_derivative(_12249, _12250, intervalFailed, optical_product_upper);
    Interval _12251 = _15366;
    Interval _12252 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12253 = Interval{ _15253, _15254 };
    Interval _15369 = jet_mul_derivative(_12252, _12253, intervalFailed, optical_product_upper);
    Interval _12254 = _15369;
    Interval _15370 = jet_add_derivative(_12251, _12254, intervalFailed);
    Interval _12255 = Interval{ 0.0, 0.0 };
    Interval _12256 = Interval{ _15251, _15252 };
    Interval _15372 = jet_mul_derivative(_12255, _12256, intervalFailed, optical_product_upper);
    Interval _12257 = _15372;
    Interval _12258 = Interval{ f.rotation2.x, f.rotation2.x };
    Interval _12259 = Interval{ _15255, _15256 };
    Interval _15375 = jet_mul_derivative(_12258, _12259, intervalFailed, optical_product_upper);
    Interval _12260 = _15375;
    Interval _15376 = jet_add_derivative(_12257, _12260, intervalFailed);
    Interval _12233 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12234 = Interval{ -1.0, -1.0 };
    Interval _15378 = imul(_12233, _12234, intervalFailed, optical_product_upper);
    Interval _12235 = Interval{ 0.0, 0.0 };
    Interval _12236 = Interval{ -1.0, -1.0 };
    Interval _15379 = jet_mul_derivative(_12235, _12236, intervalFailed, optical_product_upper);
    Interval _12237 = _15379;
    Interval _12238 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12239 = Interval{ 0.0, 0.0 };
    Interval _15381 = jet_mul_derivative(_12238, _12239, intervalFailed, optical_product_upper);
    Interval _12240 = _15381;
    Interval _15382 = jet_add_derivative(_12237, _12240, intervalFailed);
    Interval _12241 = Interval{ 0.0, 0.0 };
    Interval _12242 = Interval{ -1.0, -1.0 };
    Interval _15383 = jet_mul_derivative(_12241, _12242, intervalFailed, optical_product_upper);
    Interval _12243 = _15383;
    Interval _12244 = Interval{ f.rotation2.y, f.rotation2.y };
    Interval _12245 = Interval{ 0.0, 0.0 };
    Interval _15385 = jet_mul_derivative(_12244, _12245, intervalFailed, optical_product_upper);
    Interval _12246 = _15385;
    Interval _15386 = jet_add_derivative(_12243, _12246, intervalFailed);
    Interval _12227 = _15364;
    Interval _12228 = _15378;
    Interval _15387 = iadd(_12227, _12228, intervalFailed);
    Interval _12229 = _15370;
    Interval _12230 = _15382;
    Interval _15388 = jet_add_derivative(_12229, _12230, intervalFailed);
    Interval _12231 = _15376;
    Interval _12232 = _15386;
    Interval _15389 = jet_add_derivative(_12231, _12232, intervalFailed);
    Interval _12213 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12214 = Interval{ _15245, _15246 };
    Interval _15392 = imul(_12213, _12214, intervalFailed, optical_product_upper);
    Interval _12215 = Interval{ 0.0, 0.0 };
    Interval _12216 = Interval{ _15245, _15246 };
    Interval _15394 = jet_mul_derivative(_12215, _12216, intervalFailed, optical_product_upper);
    Interval _12217 = _15394;
    Interval _12218 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12219 = Interval{ _15247, _15248 };
    Interval _15397 = jet_mul_derivative(_12218, _12219, intervalFailed, optical_product_upper);
    Interval _12220 = _15397;
    Interval _15398 = jet_add_derivative(_12217, _12220, intervalFailed);
    Interval _12221 = Interval{ 0.0, 0.0 };
    Interval _12222 = Interval{ _15245, _15246 };
    Interval _15400 = jet_mul_derivative(_12221, _12222, intervalFailed, optical_product_upper);
    Interval _12223 = _15400;
    Interval _12224 = Interval{ f.rotation2.z, f.rotation2.z };
    Interval _12225 = Interval{ _15249, _15250 };
    Interval _15403 = jet_mul_derivative(_12224, _12225, intervalFailed, optical_product_upper);
    Interval _12226 = _15403;
    Interval _15404 = jet_add_derivative(_12223, _12226, intervalFailed);
    Interval _12207 = _15387;
    Interval _12208 = _15392;
    Interval _15405 = iadd(_12207, _12208, intervalFailed);
    Interval _12209 = _15388;
    Interval _12210 = _15398;
    Interval _15406 = jet_add_derivative(_12209, _12210, intervalFailed);
    Interval _12211 = _15389;
    Interval _12212 = _15404;
    Interval _15407 = jet_add_derivative(_12211, _12212, intervalFailed);
    bool _15414;
    if (_15307.lo <= 0.0)
    {
        _15414 = _15307.hi >= 0.0;
    }
    else
    {
        _15414 = false;
    }
    float _15421;
    if (_15414)
    {
        _15421 = 0.0;
    }
    else
    {
        _15421 = precise::min(abs(_15307.lo), abs(_15307.hi));
    }
    float _15424 = precise::max(abs(_15307.lo), abs(_15307.hi));
    float _12197 = spvFMul(_15421, _15421);
    float _15425 = interval_down(_12197, intervalFailed);
    float _12198 = spvFMul(_15424, _15424);
    float _15427 = interval_up(_12198, intervalFailed);
    Interval _12199 = Interval{ 2.0, 2.0 };
    Interval _12200 = _15307;
    Interval _15428 = imul(_12199, _12200, intervalFailed, optical_product_upper);
    Interval _12201 = _15428;
    Interval _12202 = _15308;
    Interval _15429 = jet_mul_derivative(_12201, _12202, intervalFailed, optical_product_upper);
    Interval _12203 = Interval{ 2.0, 2.0 };
    Interval _12204 = _15307;
    Interval _15430 = imul(_12203, _12204, intervalFailed, optical_product_upper);
    Interval _12205 = _15430;
    Interval _12206 = _15309;
    Interval _15431 = jet_mul_derivative(_12205, _12206, intervalFailed, optical_product_upper);
    bool _15438;
    if (_15356.lo <= 0.0)
    {
        _15438 = _15356.hi >= 0.0;
    }
    else
    {
        _15438 = false;
    }
    float _15445;
    if (_15438)
    {
        _15445 = 0.0;
    }
    else
    {
        _15445 = precise::min(abs(_15356.lo), abs(_15356.hi));
    }
    float _15448 = precise::max(abs(_15356.lo), abs(_15356.hi));
    float _12187 = spvFMul(_15445, _15445);
    float _15449 = interval_down(_12187, intervalFailed);
    float _12188 = spvFMul(_15448, _15448);
    float _15451 = interval_up(_12188, intervalFailed);
    Interval _12189 = Interval{ 2.0, 2.0 };
    Interval _12190 = _15356;
    Interval _15452 = imul(_12189, _12190, intervalFailed, optical_product_upper);
    Interval _12191 = _15452;
    Interval _12192 = _15357;
    Interval _15453 = jet_mul_derivative(_12191, _12192, intervalFailed, optical_product_upper);
    Interval _12193 = Interval{ 2.0, 2.0 };
    Interval _12194 = _15356;
    Interval _15454 = imul(_12193, _12194, intervalFailed, optical_product_upper);
    Interval _12195 = _15454;
    Interval _12196 = _15358;
    Interval _15455 = jet_mul_derivative(_12195, _12196, intervalFailed, optical_product_upper);
    Interval _12181 = Interval{ precise::max(0.0, _15425), _15427 };
    Interval _12182 = Interval{ precise::max(0.0, _15449), _15451 };
    Interval _15458 = iadd(_12181, _12182, intervalFailed);
    Interval _12183 = _15429;
    Interval _12184 = _15453;
    Interval _15459 = jet_add_derivative(_12183, _12184, intervalFailed);
    Interval _12185 = _15431;
    Interval _12186 = _15455;
    Interval _15460 = jet_add_derivative(_12185, _12186, intervalFailed);
    bool _15467;
    if (_15405.lo <= 0.0)
    {
        _15467 = _15405.hi >= 0.0;
    }
    else
    {
        _15467 = false;
    }
    float _15474;
    if (_15467)
    {
        _15474 = 0.0;
    }
    else
    {
        _15474 = precise::min(abs(_15405.lo), abs(_15405.hi));
    }
    float _15477 = precise::max(abs(_15405.lo), abs(_15405.hi));
    float _12171 = spvFMul(_15474, _15474);
    float _15478 = interval_down(_12171, intervalFailed);
    float _12172 = spvFMul(_15477, _15477);
    float _15480 = interval_up(_12172, intervalFailed);
    Interval _12173 = Interval{ 2.0, 2.0 };
    Interval _12174 = _15405;
    Interval _15481 = imul(_12173, _12174, intervalFailed, optical_product_upper);
    Interval _12175 = _15481;
    Interval _12176 = _15406;
    Interval _15482 = jet_mul_derivative(_12175, _12176, intervalFailed, optical_product_upper);
    Interval _12177 = Interval{ 2.0, 2.0 };
    Interval _12178 = _15405;
    Interval _15483 = imul(_12177, _12178, intervalFailed, optical_product_upper);
    Interval _12179 = _15483;
    Interval _12180 = _15407;
    Interval _15484 = jet_mul_derivative(_12179, _12180, intervalFailed, optical_product_upper);
    Interval _12165 = _15458;
    Interval _12166 = Interval{ precise::max(0.0, _15478), _15480 };
    Interval _15486 = iadd(_12165, _12166, intervalFailed);
    Interval _12167 = _15459;
    Interval _12168 = _15482;
    Interval _15487 = jet_add_derivative(_12167, _12168, intervalFailed);
    Interval _12169 = _15460;
    Interval _12170 = _15484;
    Interval _15488 = jet_add_derivative(_12169, _12170, intervalFailed);
    Interval _12156 = _15486;
    Interval _15490 = isqrt(_12156, intervalFailed);
    bool _15498;
    if (!intervalFailed)
    {
        _15498 = intervalFailed;
    }
    else
    {
        _15498 = false;
    }
    bool _15503;
    if (_15498)
    {
        _15503 = jetFailureSite == 0u;
    }
    else
    {
        _15503 = false;
    }
    if (_15503)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_15486.lo, _15486.hi, 0.0, 0.0);
    }
    if (_15490.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _12157 = Interval{ 2.0, 2.0 };
    Interval _12158 = _15490;
    Interval _15510 = imul(_12157, _12158, intervalFailed, optical_product_upper);
    Interval _12159 = Interval{ 1.0, 1.0 };
    Interval _12160 = _15510;
    Interval _15512 = idiv(_12159, _12160, intervalFailed, interval_divide_upper);
    bool _15519;
    if (!intervalFailed)
    {
        _15519 = intervalFailed;
    }
    else
    {
        _15519 = false;
    }
    bool _15524;
    if (_15519)
    {
        _15524 = jetFailureSite == 0u;
    }
    else
    {
        _15524 = false;
    }
    if (_15524)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _15510.lo, _15510.hi);
    }
    Interval _12161 = _15512;
    Interval _12162 = _15487;
    Interval _15528 = jet_mul_derivative(_12161, _12162, intervalFailed, optical_product_upper);
    Interval _12163 = _15512;
    Interval _12164 = _15488;
    Interval _15529 = jet_mul_derivative(_12163, _12164, intervalFailed, optical_product_upper);
    Interval _12142 = Interval{ 1.0, 1.0 };
    Interval _12143 = _15490;
    Interval _15531 = idiv(_12142, _12143, intervalFailed, interval_divide_upper);
    bool _15538;
    if (!intervalFailed)
    {
        _15538 = intervalFailed;
    }
    else
    {
        _15538 = false;
    }
    bool _15543;
    if (_15538)
    {
        _15543 = jetFailureSite == 0u;
    }
    else
    {
        _15543 = false;
    }
    if (_15543)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _15490.lo, _15490.hi);
    }
    Interval _12144 = Interval{ 0.0, 0.0 };
    Interval _12145 = _15531;
    Interval _12146 = _15528;
    Interval _15547 = jet_mul_derivative(_12145, _12146, intervalFailed, optical_product_upper);
    Interval _12147 = Interval{ as_type<float>(as_type<uint>(_15547.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15547.lo) ^ 2147483648u) };
    Interval _15557 = jet_add_derivative(_12144, _12147, intervalFailed);
    Interval _12148 = _15557;
    Interval _12149 = _15490;
    Interval _15558 = jet_div_derivative(_12148, _12149, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _12150 = Interval{ 0.0, 0.0 };
    Interval _12151 = _15531;
    Interval _12152 = _15529;
    Interval _15559 = jet_mul_derivative(_12151, _12152, intervalFailed, optical_product_upper);
    Interval _12153 = Interval{ as_type<float>(as_type<uint>(_15559.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15559.lo) ^ 2147483648u) };
    Interval _15569 = jet_add_derivative(_12150, _12153, intervalFailed);
    Interval _12154 = _15569;
    Interval _12155 = _15490;
    Interval _15570 = jet_div_derivative(_12154, _12155, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _12128 = _15307;
    Interval _12129 = _15531;
    Interval _15571 = imul(_12128, _12129, intervalFailed, optical_product_upper);
    Interval _12130 = _15308;
    Interval _12131 = _15531;
    Interval _15572 = jet_mul_derivative(_12130, _12131, intervalFailed, optical_product_upper);
    Interval _12132 = _15572;
    Interval _12133 = _15307;
    Interval _12134 = _15558;
    Interval _15573 = jet_mul_derivative(_12133, _12134, intervalFailed, optical_product_upper);
    Interval _12135 = _15573;
    Interval _15574 = jet_add_derivative(_12132, _12135, intervalFailed);
    Interval _12136 = _15309;
    Interval _12137 = _15531;
    Interval _15575 = jet_mul_derivative(_12136, _12137, intervalFailed, optical_product_upper);
    Interval _12138 = _15575;
    Interval _12139 = _15307;
    Interval _12140 = _15570;
    Interval _15576 = jet_mul_derivative(_12139, _12140, intervalFailed, optical_product_upper);
    Interval _12141 = _15576;
    Interval _15577 = jet_add_derivative(_12138, _12141, intervalFailed);
    Interval _12114 = _15356;
    Interval _12115 = _15531;
    Interval _15578 = imul(_12114, _12115, intervalFailed, optical_product_upper);
    Interval _12116 = _15357;
    Interval _12117 = _15531;
    Interval _15579 = jet_mul_derivative(_12116, _12117, intervalFailed, optical_product_upper);
    Interval _12118 = _15579;
    Interval _12119 = _15356;
    Interval _12120 = _15558;
    Interval _15580 = jet_mul_derivative(_12119, _12120, intervalFailed, optical_product_upper);
    Interval _12121 = _15580;
    Interval _15581 = jet_add_derivative(_12118, _12121, intervalFailed);
    Interval _12122 = _15358;
    Interval _12123 = _15531;
    Interval _15582 = jet_mul_derivative(_12122, _12123, intervalFailed, optical_product_upper);
    Interval _12124 = _15582;
    Interval _12125 = _15356;
    Interval _12126 = _15570;
    Interval _15583 = jet_mul_derivative(_12125, _12126, intervalFailed, optical_product_upper);
    Interval _12127 = _15583;
    Interval _15584 = jet_add_derivative(_12124, _12127, intervalFailed);
    Interval _12100 = _15405;
    Interval _12101 = _15531;
    Interval _15585 = imul(_12100, _12101, intervalFailed, optical_product_upper);
    Interval _12102 = _15406;
    Interval _12103 = _15531;
    Interval _15586 = jet_mul_derivative(_12102, _12103, intervalFailed, optical_product_upper);
    Interval _12104 = _15586;
    Interval _12105 = _15405;
    Interval _12106 = _15558;
    Interval _15587 = jet_mul_derivative(_12105, _12106, intervalFailed, optical_product_upper);
    Interval _12107 = _15587;
    Interval _15588 = jet_add_derivative(_12104, _12107, intervalFailed);
    Interval _12108 = _15407;
    Interval _12109 = _15531;
    Interval _15589 = jet_mul_derivative(_12108, _12109, intervalFailed, optical_product_upper);
    Interval _12110 = _15589;
    Interval _12111 = _15405;
    Interval _12112 = _15570;
    Interval _15590 = jet_mul_derivative(_12111, _12112, intervalFailed, optical_product_upper);
    Interval _12113 = _15590;
    Interval _15591 = jet_add_derivative(_12110, _12113, intervalFailed);
    OpticalJet3 param_var_n = OpticalJet3{ OpticalJet{ _15571, _15574, _15577 }, OpticalJet{ _15578, _15581, _15584 }, OpticalJet{ _15585, _15588, _15591 } };
    OpticalJet3 param_var_direction = direction;
    OpticalJet3 _15597 = jet_oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper, jetBranchKnown);
    return _15597;
}

static inline __attribute__((always_inline))
bool jet_inside_face(thread const ReflectionSpecularPlane& plane, thread const OpticalJet3& hit, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper)
{
    bool _15690;
    if (plane.a.w == 2.0)
    {
        _15690 = plane.b.w == 2.0;
    }
    else
    {
        _15690 = false;
    }
    bool _15695;
    if (_15690)
    {
        _15695 = plane.c.w == 2.0;
    }
    else
    {
        _15695 = false;
    }
    if (_15695)
    {
        return true;
    }
    Interval _15674 = Interval{ plane.b.x, plane.b.x };
    Interval _15675 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15726 = iadd(_15674, _15675, intervalFailed);
    Interval _15676 = Interval{ plane.b.y, plane.b.y };
    Interval _15677 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15729 = iadd(_15676, _15677, intervalFailed);
    Interval _15678 = Interval{ plane.b.z, plane.b.z };
    Interval _15679 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15732 = iadd(_15678, _15679, intervalFailed);
    Interval _15668 = Interval{ plane.c.x, plane.c.x };
    Interval _15669 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15763 = iadd(_15668, _15669, intervalFailed);
    Interval _15670 = Interval{ plane.c.y, plane.c.y };
    Interval _15671 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15766 = iadd(_15670, _15671, intervalFailed);
    Interval _15672 = Interval{ plane.c.z, plane.c.z };
    Interval _15673 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15769 = iadd(_15672, _15673, intervalFailed);
    Interval _15662 = hit.x.v;
    Interval _15663 = Interval{ as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.x) ^ 2147483648u) };
    Interval _15800 = iadd(_15662, _15663, intervalFailed);
    Interval _15664 = hit.y.v;
    Interval _15665 = Interval{ as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.y) ^ 2147483648u) };
    Interval _15802 = iadd(_15664, _15665, intervalFailed);
    Interval _15666 = hit.z.v;
    Interval _15667 = Interval{ as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u), as_type<float>(as_type<uint>(plane.a.z) ^ 2147483648u) };
    Interval _15804 = iadd(_15666, _15667, intervalFailed);
    Interval _15652 = _15726;
    Interval _15653 = _15726;
    Interval _15805 = imul(_15652, _15653, intervalFailed, optical_product_upper);
    Interval _15654 = _15805;
    Interval _15655 = _15729;
    Interval _15656 = _15729;
    Interval _15806 = imul(_15655, _15656, intervalFailed, optical_product_upper);
    Interval _15657 = _15806;
    Interval _15807 = iadd(_15654, _15657, intervalFailed);
    Interval _15658 = _15807;
    Interval _15659 = _15732;
    Interval _15660 = _15732;
    Interval _15808 = imul(_15659, _15660, intervalFailed, optical_product_upper);
    Interval _15661 = _15808;
    Interval _15809 = iadd(_15658, _15661, intervalFailed);
    Interval _15642 = _15726;
    Interval _15643 = _15763;
    Interval _15810 = imul(_15642, _15643, intervalFailed, optical_product_upper);
    Interval _15644 = _15810;
    Interval _15645 = _15729;
    Interval _15646 = _15766;
    Interval _15811 = imul(_15645, _15646, intervalFailed, optical_product_upper);
    Interval _15647 = _15811;
    Interval _15812 = iadd(_15644, _15647, intervalFailed);
    Interval _15648 = _15812;
    Interval _15649 = _15732;
    Interval _15650 = _15769;
    Interval _15813 = imul(_15649, _15650, intervalFailed, optical_product_upper);
    Interval _15651 = _15813;
    Interval _15814 = iadd(_15648, _15651, intervalFailed);
    Interval _15632 = _15763;
    Interval _15633 = _15763;
    Interval _15815 = imul(_15632, _15633, intervalFailed, optical_product_upper);
    Interval _15634 = _15815;
    Interval _15635 = _15766;
    Interval _15636 = _15766;
    Interval _15816 = imul(_15635, _15636, intervalFailed, optical_product_upper);
    Interval _15637 = _15816;
    Interval _15817 = iadd(_15634, _15637, intervalFailed);
    Interval _15638 = _15817;
    Interval _15639 = _15769;
    Interval _15640 = _15769;
    Interval _15818 = imul(_15639, _15640, intervalFailed, optical_product_upper);
    Interval _15641 = _15818;
    Interval _15819 = iadd(_15638, _15641, intervalFailed);
    Interval _15622 = _15800;
    Interval _15623 = _15726;
    Interval _15820 = imul(_15622, _15623, intervalFailed, optical_product_upper);
    Interval _15624 = _15820;
    Interval _15625 = _15802;
    Interval _15626 = _15729;
    Interval _15821 = imul(_15625, _15626, intervalFailed, optical_product_upper);
    Interval _15627 = _15821;
    Interval _15822 = iadd(_15624, _15627, intervalFailed);
    Interval _15628 = _15822;
    Interval _15629 = _15804;
    Interval _15630 = _15732;
    Interval _15823 = imul(_15629, _15630, intervalFailed, optical_product_upper);
    Interval _15631 = _15823;
    Interval _15824 = iadd(_15628, _15631, intervalFailed);
    Interval _15612 = _15800;
    Interval _15613 = _15763;
    Interval _15825 = imul(_15612, _15613, intervalFailed, optical_product_upper);
    Interval _15614 = _15825;
    Interval _15615 = _15802;
    Interval _15616 = _15766;
    Interval _15826 = imul(_15615, _15616, intervalFailed, optical_product_upper);
    Interval _15617 = _15826;
    Interval _15827 = iadd(_15614, _15617, intervalFailed);
    Interval _15618 = _15827;
    Interval _15619 = _15804;
    Interval _15620 = _15769;
    Interval _15828 = imul(_15619, _15620, intervalFailed, optical_product_upper);
    Interval _15621 = _15828;
    Interval _15829 = iadd(_15618, _15621, intervalFailed);
    Interval param_var_a = _15809;
    Interval param_var_b = _15819;
    Interval _15830 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
    bool _15837;
    if (_15814.lo <= 0.0)
    {
        _15837 = _15814.hi >= 0.0;
    }
    else
    {
        _15837 = false;
    }
    float _15844;
    if (_15837)
    {
        _15844 = 0.0;
    }
    else
    {
        _15844 = precise::min(abs(_15814.lo), abs(_15814.hi));
    }
    float _15847 = precise::max(abs(_15814.lo), abs(_15814.hi));
    float _15610 = spvFMul(_15844, _15844);
    float _15848 = interval_down(_15610, intervalFailed);
    float _15611 = spvFMul(_15847, _15847);
    float _15850 = interval_up(_15611, intervalFailed);
    Interval _15608 = _15830;
    Interval _15609 = Interval{ as_type<float>(as_type<uint>(_15850) ^ 2147483648u), as_type<float>(as_type<uint>(precise::max(0.0, _15848)) ^ 2147483648u) };
    Interval _15858 = iadd(_15608, _15609, intervalFailed);
    if (_15858.lo <= 0.0)
    {
        return false;
    }
    Interval param_var_a_1 = _15819;
    Interval param_var_b_1 = _15824;
    Interval _15861 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
    Interval param_var_a_2 = _15814;
    Interval param_var_b_2 = _15829;
    Interval _15862 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval _15606 = _15861;
    Interval _15607 = Interval{ as_type<float>(as_type<uint>(_15862.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15862.lo) ^ 2147483648u) };
    Interval _15872 = iadd(_15606, _15607, intervalFailed);
    Interval param_var_a_3 = _15872;
    Interval param_var_b_3 = _15858;
    Interval _15873 = idiv(param_var_a_3, param_var_b_3, intervalFailed, interval_divide_upper);
    Interval param_var_a_4 = _15809;
    Interval param_var_b_4 = _15829;
    Interval _15875 = imul(param_var_a_4, param_var_b_4, intervalFailed, optical_product_upper);
    Interval param_var_a_5 = _15814;
    Interval param_var_b_5 = _15824;
    Interval _15876 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
    Interval _15604 = _15875;
    Interval _15605 = Interval{ as_type<float>(as_type<uint>(_15876.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_15876.lo) ^ 2147483648u) };
    Interval _15886 = iadd(_15604, _15605, intervalFailed);
    Interval param_var_a_6 = _15886;
    Interval param_var_b_6 = _15858;
    Interval _15887 = idiv(param_var_a_6, param_var_b_6, intervalFailed, interval_divide_upper);
    float _15602 = -9.9999997473787516355514526367188e-06;
    __attribute__((unused)) float _15889 = interval_down(_15602, intervalFailed);
    float _15603 = -9.9999997473787516355514526367188e-06;
    float _15890 = interval_up(_15603, intervalFailed);
    bool _15895;
    if (_15873.lo >= _15890)
    {
        float _15600 = -9.9999997473787516355514526367188e-06;
        __attribute__((unused)) float _15892 = interval_down(_15600, intervalFailed);
        float _15601 = -9.9999997473787516355514526367188e-06;
        float _15893 = interval_up(_15601, intervalFailed);
        _15895 = _15887.lo >= _15893;
    }
    else
    {
        _15895 = false;
    }
    bool _15901;
    if (_15895)
    {
        Interval param_var_a_7 = _15873;
        Interval param_var_b_7 = _15887;
        Interval _15896 = iadd(param_var_a_7, param_var_b_7, intervalFailed);
        float _15598 = 1.000010013580322265625;
        float _15898 = interval_down(_15598, intervalFailed);
        float _15599 = 1.000010013580322265625;
        __attribute__((unused)) float _15899 = interval_up(_15599, intervalFailed);
        _15901 = _15896.hi <= _15898;
    }
    else
    {
        _15901 = false;
    }
    return _15901;
}

static inline __attribute__((always_inline))
bool optical_jet_forward(thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread OpticalJetRay& ray, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    ray.outgoing = OpticalJet3{ OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
    ray.origin = OpticalJet3{ OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } }, OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } } };
    ray.bias0 = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    ray.depth = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    bool4 _6288 = isnan(box);
    bool4 _6289 = isinf(box);
    bool _6299;
    if (all(not(bool4(_6288.x || _6289.x, _6288.y || _6289.y, _6288.z || _6289.z, _6288.w || _6289.w))))
    {
        _6299 = any(box.xy > box.zw);
    }
    else
    {
        _6299 = true;
    }
    bool _6305;
    if (!_6299)
    {
        _6305 = any(box.xy < float2(0.0));
    }
    else
    {
        _6305 = true;
    }
    bool _6313;
    if (!_6305)
    {
        _6313 = box.z >= receiver.extentClip.x;
    }
    else
    {
        _6313 = true;
    }
    bool _6321;
    if (!_6313)
    {
        _6321 = box.w >= receiver.extentClip.y;
    }
    else
    {
        _6321 = true;
    }
    bool _6326;
    if (!_6321)
    {
        _6326 = control.x > 4u;
    }
    else
    {
        _6326 = true;
    }
    bool _6335;
    if (!_6326)
    {
        _6335 = (control.y >> (control.x & 31u)) != 0u;
    }
    else
    {
        _6335 = true;
    }
    if (_6335)
    {
        return false;
    }
    Interval _6277 = Interval{ box.x, box.z };
    Interval _6278 = Interval{ as_type<float>(as_type<uint>(receiver.projection.z) ^ 2147483648u), as_type<float>(as_type<uint>(receiver.projection.z) ^ 2147483648u) };
    Interval _6359 = iadd(_6277, _6278, intervalFailed);
    Interval _6279 = Interval{ 1.0, 1.0 };
    Interval _6280 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6361 = jet_add_derivative(_6279, _6280, intervalFailed);
    Interval _6281 = Interval{ 0.0, 0.0 };
    Interval _6282 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6363 = jet_add_derivative(_6281, _6282, intervalFailed);
    Interval _6263 = _6359;
    Interval _6264 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6369 = idiv(_6263, _6264, intervalFailed, interval_divide_upper);
    bool _6376;
    if (!intervalFailed)
    {
        _6376 = intervalFailed;
    }
    else
    {
        _6376 = false;
    }
    bool _6381;
    if (_6376)
    {
        _6381 = jetFailureSite == 0u;
    }
    else
    {
        _6381 = false;
    }
    if (_6381)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(_6359.lo, _6359.hi, receiver.projection.x, receiver.projection.x);
    }
    Interval _6265 = _6361;
    Interval _6266 = _6369;
    Interval _6267 = Interval{ 0.0, 0.0 };
    Interval _6385 = jet_mul_derivative(_6266, _6267, intervalFailed, optical_product_upper);
    Interval _6268 = Interval{ as_type<float>(as_type<uint>(_6385.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6385.lo) ^ 2147483648u) };
    Interval _6395 = jet_add_derivative(_6265, _6268, intervalFailed);
    Interval _6269 = _6395;
    Interval _6270 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6397 = jet_div_derivative(_6269, _6270, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6271 = _6363;
    Interval _6272 = _6369;
    Interval _6273 = Interval{ 0.0, 0.0 };
    Interval _6398 = jet_mul_derivative(_6272, _6273, intervalFailed, optical_product_upper);
    Interval _6274 = Interval{ as_type<float>(as_type<uint>(_6398.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6398.lo) ^ 2147483648u) };
    Interval _6408 = jet_add_derivative(_6271, _6274, intervalFailed);
    Interval _6275 = _6408;
    Interval _6276 = Interval{ receiver.projection.x, receiver.projection.x };
    Interval _6410 = jet_div_derivative(_6275, _6276, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6257 = Interval{ box.y, box.w };
    Interval _6258 = Interval{ as_type<float>(as_type<uint>(receiver.projection.w) ^ 2147483648u), as_type<float>(as_type<uint>(receiver.projection.w) ^ 2147483648u) };
    Interval _6426 = iadd(_6257, _6258, intervalFailed);
    Interval _6259 = Interval{ 0.0, 0.0 };
    Interval _6260 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6428 = jet_add_derivative(_6259, _6260, intervalFailed);
    Interval _6261 = Interval{ 1.0, 1.0 };
    Interval _6262 = Interval{ as_type<float>(2147483648u), as_type<float>(2147483648u) };
    Interval _6430 = jet_add_derivative(_6261, _6262, intervalFailed);
    Interval _6243 = _6426;
    Interval _6244 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6436 = idiv(_6243, _6244, intervalFailed, interval_divide_upper);
    bool _6443;
    if (!intervalFailed)
    {
        _6443 = intervalFailed;
    }
    else
    {
        _6443 = false;
    }
    bool _6448;
    if (_6443)
    {
        _6448 = jetFailureSite == 0u;
    }
    else
    {
        _6448 = false;
    }
    if (_6448)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(_6426.lo, _6426.hi, receiver.projection.y, receiver.projection.y);
    }
    Interval _6245 = _6428;
    Interval _6246 = _6436;
    Interval _6247 = Interval{ 0.0, 0.0 };
    Interval _6452 = jet_mul_derivative(_6246, _6247, intervalFailed, optical_product_upper);
    Interval _6248 = Interval{ as_type<float>(as_type<uint>(_6452.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6452.lo) ^ 2147483648u) };
    Interval _6462 = jet_add_derivative(_6245, _6248, intervalFailed);
    Interval _6249 = _6462;
    Interval _6250 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6464 = jet_div_derivative(_6249, _6250, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6251 = _6430;
    Interval _6252 = _6436;
    Interval _6253 = Interval{ 0.0, 0.0 };
    Interval _6465 = jet_mul_derivative(_6252, _6253, intervalFailed, optical_product_upper);
    Interval _6254 = Interval{ as_type<float>(as_type<uint>(_6465.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6465.lo) ^ 2147483648u) };
    Interval _6475 = jet_add_derivative(_6251, _6254, intervalFailed);
    Interval _6255 = _6475;
    Interval _6256 = Interval{ receiver.projection.y, receiver.projection.y };
    Interval _6477 = jet_div_derivative(_6255, _6256, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    bool _6484;
    if (_6369.lo <= 0.0)
    {
        _6484 = _6369.hi >= 0.0;
    }
    else
    {
        _6484 = false;
    }
    float _6491;
    if (_6484)
    {
        _6491 = 0.0;
    }
    else
    {
        _6491 = precise::min(abs(_6369.lo), abs(_6369.hi));
    }
    float _6494 = precise::max(abs(_6369.lo), abs(_6369.hi));
    float _6233 = spvFMul(_6491, _6491);
    float _6495 = interval_down(_6233, intervalFailed);
    float _6234 = spvFMul(_6494, _6494);
    float _6497 = interval_up(_6234, intervalFailed);
    Interval _6235 = Interval{ 2.0, 2.0 };
    Interval _6236 = _6369;
    Interval _6498 = imul(_6235, _6236, intervalFailed, optical_product_upper);
    Interval _6237 = _6498;
    Interval _6238 = _6397;
    Interval _6499 = jet_mul_derivative(_6237, _6238, intervalFailed, optical_product_upper);
    Interval _6239 = Interval{ 2.0, 2.0 };
    Interval _6240 = _6369;
    Interval _6500 = imul(_6239, _6240, intervalFailed, optical_product_upper);
    Interval _6241 = _6500;
    Interval _6242 = _6410;
    Interval _6501 = jet_mul_derivative(_6241, _6242, intervalFailed, optical_product_upper);
    bool _6508;
    if (_6436.lo <= 0.0)
    {
        _6508 = _6436.hi >= 0.0;
    }
    else
    {
        _6508 = false;
    }
    float _6515;
    if (_6508)
    {
        _6515 = 0.0;
    }
    else
    {
        _6515 = precise::min(abs(_6436.lo), abs(_6436.hi));
    }
    float _6518 = precise::max(abs(_6436.lo), abs(_6436.hi));
    float _6223 = spvFMul(_6515, _6515);
    float _6519 = interval_down(_6223, intervalFailed);
    float _6224 = spvFMul(_6518, _6518);
    float _6521 = interval_up(_6224, intervalFailed);
    Interval _6225 = Interval{ 2.0, 2.0 };
    Interval _6226 = _6436;
    Interval _6522 = imul(_6225, _6226, intervalFailed, optical_product_upper);
    Interval _6227 = _6522;
    Interval _6228 = _6464;
    Interval _6523 = jet_mul_derivative(_6227, _6228, intervalFailed, optical_product_upper);
    Interval _6229 = Interval{ 2.0, 2.0 };
    Interval _6230 = _6436;
    Interval _6524 = imul(_6229, _6230, intervalFailed, optical_product_upper);
    Interval _6231 = _6524;
    Interval _6232 = _6477;
    Interval _6525 = jet_mul_derivative(_6231, _6232, intervalFailed, optical_product_upper);
    Interval _6217 = Interval{ precise::max(0.0, _6495), _6497 };
    Interval _6218 = Interval{ precise::max(0.0, _6519), _6521 };
    Interval _6528 = iadd(_6217, _6218, intervalFailed);
    Interval _6219 = _6499;
    Interval _6220 = _6523;
    Interval _6529 = jet_add_derivative(_6219, _6220, intervalFailed);
    Interval _6221 = _6501;
    Interval _6222 = _6525;
    Interval _6530 = jet_add_derivative(_6221, _6222, intervalFailed);
    bool _6535;
    if (1.0 <= 0.0)
    {
        _6535 = 1.0 >= 0.0;
    }
    else
    {
        _6535 = false;
    }
    float _6542;
    if (_6535)
    {
        _6542 = 0.0;
    }
    else
    {
        _6542 = precise::min(abs(1.0), abs(1.0));
    }
    float _6545 = precise::max(abs(1.0), abs(1.0));
    float _6207 = spvFMul(_6542, _6542);
    float _6546 = interval_down(_6207, intervalFailed);
    float _6208 = spvFMul(_6545, _6545);
    float _6548 = interval_up(_6208, intervalFailed);
    Interval _6209 = Interval{ 2.0, 2.0 };
    Interval _6210 = Interval{ 1.0, 1.0 };
    Interval _6549 = imul(_6209, _6210, intervalFailed, optical_product_upper);
    Interval _6211 = _6549;
    Interval _6212 = Interval{ 0.0, 0.0 };
    Interval _6550 = jet_mul_derivative(_6211, _6212, intervalFailed, optical_product_upper);
    Interval _6213 = Interval{ 2.0, 2.0 };
    Interval _6214 = Interval{ 1.0, 1.0 };
    Interval _6551 = imul(_6213, _6214, intervalFailed, optical_product_upper);
    Interval _6215 = _6551;
    Interval _6216 = Interval{ 0.0, 0.0 };
    Interval _6552 = jet_mul_derivative(_6215, _6216, intervalFailed, optical_product_upper);
    Interval _6201 = _6528;
    Interval _6202 = Interval{ precise::max(0.0, _6546), _6548 };
    Interval _6554 = iadd(_6201, _6202, intervalFailed);
    Interval _6203 = _6529;
    Interval _6204 = _6550;
    Interval _6555 = jet_add_derivative(_6203, _6204, intervalFailed);
    Interval _6205 = _6530;
    Interval _6206 = _6552;
    Interval _6556 = jet_add_derivative(_6205, _6206, intervalFailed);
    Interval _6192 = _6554;
    Interval _6558 = isqrt(_6192, intervalFailed);
    bool _6566;
    if (!intervalFailed)
    {
        _6566 = intervalFailed;
    }
    else
    {
        _6566 = false;
    }
    bool _6571;
    if (_6566)
    {
        _6571 = jetFailureSite == 0u;
    }
    else
    {
        _6571 = false;
    }
    if (_6571)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_6554.lo, _6554.hi, 0.0, 0.0);
    }
    if (_6558.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _6193 = Interval{ 2.0, 2.0 };
    Interval _6194 = _6558;
    Interval _6578 = imul(_6193, _6194, intervalFailed, optical_product_upper);
    Interval _6195 = Interval{ 1.0, 1.0 };
    Interval _6196 = _6578;
    Interval _6580 = idiv(_6195, _6196, intervalFailed, interval_divide_upper);
    bool _6587;
    if (!intervalFailed)
    {
        _6587 = intervalFailed;
    }
    else
    {
        _6587 = false;
    }
    bool _6592;
    if (_6587)
    {
        _6592 = jetFailureSite == 0u;
    }
    else
    {
        _6592 = false;
    }
    if (_6592)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _6578.lo, _6578.hi);
    }
    Interval _6197 = _6580;
    Interval _6198 = _6555;
    Interval _6596 = jet_mul_derivative(_6197, _6198, intervalFailed, optical_product_upper);
    Interval _6199 = _6580;
    Interval _6200 = _6556;
    Interval _6597 = jet_mul_derivative(_6199, _6200, intervalFailed, optical_product_upper);
    Interval _6178 = Interval{ 1.0, 1.0 };
    Interval _6179 = _6558;
    Interval _6599 = idiv(_6178, _6179, intervalFailed, interval_divide_upper);
    bool _6606;
    if (!intervalFailed)
    {
        _6606 = intervalFailed;
    }
    else
    {
        _6606 = false;
    }
    bool _6611;
    if (_6606)
    {
        _6611 = jetFailureSite == 0u;
    }
    else
    {
        _6611 = false;
    }
    if (_6611)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _6558.lo, _6558.hi);
    }
    Interval _6180 = Interval{ 0.0, 0.0 };
    Interval _6181 = _6599;
    Interval _6182 = _6596;
    Interval _6615 = jet_mul_derivative(_6181, _6182, intervalFailed, optical_product_upper);
    Interval _6183 = Interval{ as_type<float>(as_type<uint>(_6615.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6615.lo) ^ 2147483648u) };
    Interval _6625 = jet_add_derivative(_6180, _6183, intervalFailed);
    Interval _6184 = _6625;
    Interval _6185 = _6558;
    Interval _6626 = jet_div_derivative(_6184, _6185, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6186 = Interval{ 0.0, 0.0 };
    Interval _6187 = _6599;
    Interval _6188 = _6597;
    Interval _6627 = jet_mul_derivative(_6187, _6188, intervalFailed, optical_product_upper);
    Interval _6189 = Interval{ as_type<float>(as_type<uint>(_6627.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_6627.lo) ^ 2147483648u) };
    Interval _6637 = jet_add_derivative(_6186, _6189, intervalFailed);
    Interval _6190 = _6637;
    Interval _6191 = _6558;
    Interval _6638 = jet_div_derivative(_6190, _6191, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _6164 = _6369;
    Interval _6165 = _6599;
    Interval _6639 = imul(_6164, _6165, intervalFailed, optical_product_upper);
    Interval _6166 = _6397;
    Interval _6167 = _6599;
    Interval _6640 = jet_mul_derivative(_6166, _6167, intervalFailed, optical_product_upper);
    Interval _6168 = _6640;
    Interval _6169 = _6369;
    Interval _6170 = _6626;
    Interval _6641 = jet_mul_derivative(_6169, _6170, intervalFailed, optical_product_upper);
    Interval _6171 = _6641;
    Interval _6642 = jet_add_derivative(_6168, _6171, intervalFailed);
    Interval _6172 = _6410;
    Interval _6173 = _6599;
    Interval _6643 = jet_mul_derivative(_6172, _6173, intervalFailed, optical_product_upper);
    Interval _6174 = _6643;
    Interval _6175 = _6369;
    Interval _6176 = _6638;
    Interval _6644 = jet_mul_derivative(_6175, _6176, intervalFailed, optical_product_upper);
    Interval _6177 = _6644;
    Interval _6645 = jet_add_derivative(_6174, _6177, intervalFailed);
    Interval _6150 = _6436;
    Interval _6151 = _6599;
    Interval _6646 = imul(_6150, _6151, intervalFailed, optical_product_upper);
    Interval _6152 = _6464;
    Interval _6153 = _6599;
    Interval _6647 = jet_mul_derivative(_6152, _6153, intervalFailed, optical_product_upper);
    Interval _6154 = _6647;
    Interval _6155 = _6436;
    Interval _6156 = _6626;
    Interval _6648 = jet_mul_derivative(_6155, _6156, intervalFailed, optical_product_upper);
    Interval _6157 = _6648;
    Interval _6649 = jet_add_derivative(_6154, _6157, intervalFailed);
    Interval _6158 = _6477;
    Interval _6159 = _6599;
    Interval _6650 = jet_mul_derivative(_6158, _6159, intervalFailed, optical_product_upper);
    Interval _6160 = _6650;
    Interval _6161 = _6436;
    Interval _6162 = _6638;
    Interval _6651 = jet_mul_derivative(_6161, _6162, intervalFailed, optical_product_upper);
    Interval _6163 = _6651;
    Interval _6652 = jet_add_derivative(_6160, _6163, intervalFailed);
    Interval _6136 = Interval{ 1.0, 1.0 };
    Interval _6137 = _6599;
    Interval _6653 = imul(_6136, _6137, intervalFailed, optical_product_upper);
    Interval _6138 = Interval{ 0.0, 0.0 };
    Interval _6139 = _6599;
    Interval _6654 = jet_mul_derivative(_6138, _6139, intervalFailed, optical_product_upper);
    Interval _6140 = _6654;
    Interval _6141 = Interval{ 1.0, 1.0 };
    Interval _6142 = _6626;
    Interval _6655 = jet_mul_derivative(_6141, _6142, intervalFailed, optical_product_upper);
    Interval _6143 = _6655;
    Interval _6656 = jet_add_derivative(_6140, _6143, intervalFailed);
    Interval _6144 = Interval{ 0.0, 0.0 };
    Interval _6145 = _6599;
    Interval _6657 = jet_mul_derivative(_6144, _6145, intervalFailed, optical_product_upper);
    Interval _6146 = _6657;
    Interval _6147 = Interval{ 1.0, 1.0 };
    Interval _6148 = _6638;
    Interval _6658 = jet_mul_derivative(_6147, _6148, intervalFailed, optical_product_upper);
    Interval _6149 = _6658;
    Interval _6659 = jet_add_derivative(_6146, _6149, intervalFailed);
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
    float _6756;
    float _6758;
    float _6760;
    _6678 = 0.0;
    _6680 = 0.0;
    _6682 = 0.0;
    _6684 = 0.0;
    _6686 = 0.0;
    _6688 = 0.0;
    _6690 = _6639.lo;
    _6692 = _6639.hi;
    _6694 = _6642.lo;
    _6696 = _6642.hi;
    _6698 = _6645.lo;
    _6700 = _6645.hi;
    _6702 = _6646.lo;
    _6704 = _6646.hi;
    _6706 = _6649.lo;
    _6708 = _6649.hi;
    _6710 = _6652.lo;
    _6712 = _6652.hi;
    _6714 = _6653.lo;
    _6716 = _6653.hi;
    _6718 = _6656.lo;
    _6720 = _6656.hi;
    _6722 = _6659.lo;
    _6724 = _6659.hi;
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
    _6756 = 0.0;
    _6758 = 0.0;
    _6760 = 0.0;
    float _6679;
    float _6681;
    float _6683;
    float _6685;
    float _6687;
    float _6689;
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
    float _6757;
    float _6759;
    float _6761;
    OpticalJet3 hit;
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
    float _6721;
    float _6723;
    float _6725;
    for (uint _6762 = 0u; _6762 <= control.x; _6678 = _6679, _6680 = _6681, _6682 = _6683, _6684 = _6685, _6686 = _6687, _6688 = _6689, _6690 = _6691, _6692 = _6693, _6694 = _6695, _6696 = _6697, _6698 = _6699, _6700 = _6701, _6702 = _6703, _6704 = _6705, _6706 = _6707, _6708 = _6709, _6710 = _6711, _6712 = _6713, _6714 = _6715, _6716 = _6717, _6718 = _6719, _6720 = _6721, _6722 = _6723, _6724 = _6725, _6726 = _6727, _6728 = _6729, _6730 = _6731, _6732 = _6733, _6734 = _6735, _6736 = _6737, _6738 = _6739, _6740 = _6741, _6742 = _6743, _6744 = _6745, _6746 = _6747, _6748 = _6749, _6750 = _6751, _6752 = _6753, _6754 = _6755, _6756 = _6757, _6758 = _6759, _6760 = _6761, _6762++)
    {
        bool _6766 = _6762 == 0u;
        bool _6776;
        if (_6766)
        {
            _6776 = control.z != 0u;
        }
        else
        {
            _6776 = (control.y & (1u << ((_6762 - 1u) & 31u))) != 0u;
        }
        float4 _6788;
        float4 _6789;
        float4 _6790;
        if (_6766)
        {
            _6788 = receiver.a;
            _6789 = receiver.b;
            _6790 = receiver.c;
        }
        else
        {
            uint _1689 = _6762 - 1u;
            _6788 = planes[_1689].a;
            _6789 = planes[_1689].b;
            _6790 = planes[_1689].c;
        }
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
        float _6854;
        float _6855;
        float _6856;
        float _6857;
        float _6858;
        float _6859;
        if (_6776)
        {
            _6842 = liquid.planeNormal.x;
            _6843 = liquid.planeNormal.x;
            _6844 = 0.0;
            _6845 = 0.0;
            _6846 = 0.0;
            _6847 = 0.0;
            _6848 = liquid.planeNormal.y;
            _6849 = liquid.planeNormal.y;
            _6850 = 0.0;
            _6851 = 0.0;
            _6852 = 0.0;
            _6853 = 0.0;
            _6854 = liquid.planeNormal.z;
            _6855 = liquid.planeNormal.z;
            _6856 = 0.0;
            _6857 = 0.0;
            _6858 = 0.0;
            _6859 = 0.0;
        }
        else
        {
            ReflectionSpecularPlane param_var_plane = ReflectionSpecularPlane{ _6788, _6789, _6790 };
            OpticalJet3 _6797 = jet_plane_normal(param_var_plane, intervalFailed, optical_product_upper, interval_divide_upper);
            OpticalJet3 param_var_n = _6797;
            OpticalJet3 param_var_direction = OpticalJet3{ OpticalJet{ Interval{ _6690, _6692 }, Interval{ _6694, _6696 }, Interval{ _6698, _6700 } }, OpticalJet{ Interval{ _6702, _6704 }, Interval{ _6706, _6708 }, Interval{ _6710, _6712 } }, OpticalJet{ Interval{ _6714, _6716 }, Interval{ _6718, _6720 }, Interval{ _6722, _6724 } } };
            OpticalJet3 _6811 = jet_oriented(param_var_n, param_var_direction, intervalFailed, optical_product_upper, jetBranchKnown);
            _6842 = _6811.x.v.lo;
            _6843 = _6811.x.v.hi;
            _6844 = _6811.x.dx.lo;
            _6845 = _6811.x.dx.hi;
            _6846 = _6811.x.dy.lo;
            _6847 = _6811.x.dy.hi;
            _6848 = _6811.y.v.lo;
            _6849 = _6811.y.v.hi;
            _6850 = _6811.y.dx.lo;
            _6851 = _6811.y.dx.hi;
            _6852 = _6811.y.dy.lo;
            _6853 = _6811.y.dy.hi;
            _6854 = _6811.z.v.lo;
            _6855 = _6811.z.v.hi;
            _6856 = _6811.z.dx.lo;
            _6857 = _6811.z.dx.hi;
            _6858 = _6811.z.dy.lo;
            _6859 = _6811.z.dy.hi;
        }
        Interval _6122 = Interval{ _6690, _6692 };
        Interval _6123 = Interval{ _6842, _6843 };
        Interval _6862 = imul(_6122, _6123, intervalFailed, optical_product_upper);
        Interval _6124 = Interval{ _6694, _6696 };
        Interval _6125 = Interval{ _6842, _6843 };
        Interval _6865 = jet_mul_derivative(_6124, _6125, intervalFailed, optical_product_upper);
        Interval _6126 = _6865;
        Interval _6127 = Interval{ _6690, _6692 };
        Interval _6128 = Interval{ _6844, _6845 };
        Interval _6868 = jet_mul_derivative(_6127, _6128, intervalFailed, optical_product_upper);
        Interval _6129 = _6868;
        Interval _6869 = jet_add_derivative(_6126, _6129, intervalFailed);
        Interval _6130 = Interval{ _6698, _6700 };
        Interval _6131 = Interval{ _6842, _6843 };
        Interval _6872 = jet_mul_derivative(_6130, _6131, intervalFailed, optical_product_upper);
        Interval _6132 = _6872;
        Interval _6133 = Interval{ _6690, _6692 };
        Interval _6134 = Interval{ _6846, _6847 };
        Interval _6875 = jet_mul_derivative(_6133, _6134, intervalFailed, optical_product_upper);
        Interval _6135 = _6875;
        Interval _6876 = jet_add_derivative(_6132, _6135, intervalFailed);
        Interval _6108 = Interval{ _6702, _6704 };
        Interval _6109 = Interval{ _6848, _6849 };
        Interval _6879 = imul(_6108, _6109, intervalFailed, optical_product_upper);
        Interval _6110 = Interval{ _6706, _6708 };
        Interval _6111 = Interval{ _6848, _6849 };
        Interval _6882 = jet_mul_derivative(_6110, _6111, intervalFailed, optical_product_upper);
        Interval _6112 = _6882;
        Interval _6113 = Interval{ _6702, _6704 };
        Interval _6114 = Interval{ _6850, _6851 };
        Interval _6885 = jet_mul_derivative(_6113, _6114, intervalFailed, optical_product_upper);
        Interval _6115 = _6885;
        Interval _6886 = jet_add_derivative(_6112, _6115, intervalFailed);
        Interval _6116 = Interval{ _6710, _6712 };
        Interval _6117 = Interval{ _6848, _6849 };
        Interval _6889 = jet_mul_derivative(_6116, _6117, intervalFailed, optical_product_upper);
        Interval _6118 = _6889;
        Interval _6119 = Interval{ _6702, _6704 };
        Interval _6120 = Interval{ _6852, _6853 };
        Interval _6892 = jet_mul_derivative(_6119, _6120, intervalFailed, optical_product_upper);
        Interval _6121 = _6892;
        Interval _6893 = jet_add_derivative(_6118, _6121, intervalFailed);
        Interval _6102 = _6862;
        Interval _6103 = _6879;
        Interval _6894 = iadd(_6102, _6103, intervalFailed);
        Interval _6104 = _6869;
        Interval _6105 = _6886;
        Interval _6895 = jet_add_derivative(_6104, _6105, intervalFailed);
        Interval _6106 = _6876;
        Interval _6107 = _6893;
        Interval _6896 = jet_add_derivative(_6106, _6107, intervalFailed);
        Interval _6088 = Interval{ _6714, _6716 };
        Interval _6089 = Interval{ _6854, _6855 };
        Interval _6899 = imul(_6088, _6089, intervalFailed, optical_product_upper);
        Interval _6090 = Interval{ _6718, _6720 };
        Interval _6091 = Interval{ _6854, _6855 };
        Interval _6902 = jet_mul_derivative(_6090, _6091, intervalFailed, optical_product_upper);
        Interval _6092 = _6902;
        Interval _6093 = Interval{ _6714, _6716 };
        Interval _6094 = Interval{ _6856, _6857 };
        Interval _6905 = jet_mul_derivative(_6093, _6094, intervalFailed, optical_product_upper);
        Interval _6095 = _6905;
        Interval _6906 = jet_add_derivative(_6092, _6095, intervalFailed);
        Interval _6096 = Interval{ _6722, _6724 };
        Interval _6097 = Interval{ _6854, _6855 };
        Interval _6909 = jet_mul_derivative(_6096, _6097, intervalFailed, optical_product_upper);
        Interval _6098 = _6909;
        Interval _6099 = Interval{ _6714, _6716 };
        Interval _6100 = Interval{ _6858, _6859 };
        Interval _6912 = jet_mul_derivative(_6099, _6100, intervalFailed, optical_product_upper);
        Interval _6101 = _6912;
        Interval _6913 = jet_add_derivative(_6098, _6101, intervalFailed);
        Interval _6082 = _6894;
        Interval _6083 = _6899;
        Interval _6914 = iadd(_6082, _6083, intervalFailed);
        Interval _6084 = _6895;
        Interval _6085 = _6906;
        Interval _6915 = jet_add_derivative(_6084, _6085, intervalFailed);
        Interval _6086 = _6896;
        Interval _6087 = _6913;
        Interval _6916 = jet_add_derivative(_6086, _6087, intervalFailed);
        bool _6923;
        if (_6914.lo <= 0.0)
        {
            _6923 = _6914.hi >= 0.0;
        }
        else
        {
            _6923 = false;
        }
        float _6930;
        if (_6923)
        {
            _6930 = 0.0;
        }
        else
        {
            _6930 = precise::min(abs(_6914.lo), abs(_6914.hi));
        }
        bool _6932;
        if (!_6766)
        {
            _6932 = _6776;
        }
        else
        {
            _6932 = false;
        }
        float _6933;
        if (_6932)
        {
            _6933 = 9.9999999392252902907785028219223e-09;
        }
        else
        {
            _6933 = 9.9999999600419720025001879548654e-13;
        }
        float _6080 = _6933;
        __attribute__((unused)) float _6934 = interval_down(_6080, intervalFailed);
        float _6081 = _6933;
        float _6935 = interval_up(_6081, intervalFailed);
        if (_6930 <= _6935)
        {
            return false;
        }
        float _7530;
        float _7531;
        float _7532;
        float _7533;
        float _7534;
        float _7535;
        if (_6766)
        {
            float3 _6941;
            if (_6776)
            {
                _6941 = liquid.planePoint.xyz;
            }
            else
            {
                _6941 = _6788.xyz;
            }
            Interval _6066 = Interval{ _6941.x, _6941.x };
            Interval _6067 = Interval{ _6842, _6843 };
            Interval _6947 = imul(_6066, _6067, intervalFailed, optical_product_upper);
            Interval _6068 = Interval{ 0.0, 0.0 };
            Interval _6069 = Interval{ _6842, _6843 };
            Interval _6949 = jet_mul_derivative(_6068, _6069, intervalFailed, optical_product_upper);
            Interval _6070 = _6949;
            Interval _6071 = Interval{ _6941.x, _6941.x };
            Interval _6072 = Interval{ _6844, _6845 };
            Interval _6952 = jet_mul_derivative(_6071, _6072, intervalFailed, optical_product_upper);
            Interval _6073 = _6952;
            Interval _6953 = jet_add_derivative(_6070, _6073, intervalFailed);
            Interval _6074 = Interval{ 0.0, 0.0 };
            Interval _6075 = Interval{ _6842, _6843 };
            Interval _6955 = jet_mul_derivative(_6074, _6075, intervalFailed, optical_product_upper);
            Interval _6076 = _6955;
            Interval _6077 = Interval{ _6941.x, _6941.x };
            Interval _6078 = Interval{ _6846, _6847 };
            Interval _6958 = jet_mul_derivative(_6077, _6078, intervalFailed, optical_product_upper);
            Interval _6079 = _6958;
            Interval _6959 = jet_add_derivative(_6076, _6079, intervalFailed);
            Interval _6052 = Interval{ _6941.y, _6941.y };
            Interval _6053 = Interval{ _6848, _6849 };
            Interval _6962 = imul(_6052, _6053, intervalFailed, optical_product_upper);
            Interval _6054 = Interval{ 0.0, 0.0 };
            Interval _6055 = Interval{ _6848, _6849 };
            Interval _6964 = jet_mul_derivative(_6054, _6055, intervalFailed, optical_product_upper);
            Interval _6056 = _6964;
            Interval _6057 = Interval{ _6941.y, _6941.y };
            Interval _6058 = Interval{ _6850, _6851 };
            Interval _6967 = jet_mul_derivative(_6057, _6058, intervalFailed, optical_product_upper);
            Interval _6059 = _6967;
            Interval _6968 = jet_add_derivative(_6056, _6059, intervalFailed);
            Interval _6060 = Interval{ 0.0, 0.0 };
            Interval _6061 = Interval{ _6848, _6849 };
            Interval _6970 = jet_mul_derivative(_6060, _6061, intervalFailed, optical_product_upper);
            Interval _6062 = _6970;
            Interval _6063 = Interval{ _6941.y, _6941.y };
            Interval _6064 = Interval{ _6852, _6853 };
            Interval _6973 = jet_mul_derivative(_6063, _6064, intervalFailed, optical_product_upper);
            Interval _6065 = _6973;
            Interval _6974 = jet_add_derivative(_6062, _6065, intervalFailed);
            Interval _6046 = _6947;
            Interval _6047 = _6962;
            Interval _6975 = iadd(_6046, _6047, intervalFailed);
            Interval _6048 = _6953;
            Interval _6049 = _6968;
            Interval _6976 = jet_add_derivative(_6048, _6049, intervalFailed);
            Interval _6050 = _6959;
            Interval _6051 = _6974;
            Interval _6977 = jet_add_derivative(_6050, _6051, intervalFailed);
            Interval _6032 = Interval{ _6941.z, _6941.z };
            Interval _6033 = Interval{ _6854, _6855 };
            Interval _6980 = imul(_6032, _6033, intervalFailed, optical_product_upper);
            Interval _6034 = Interval{ 0.0, 0.0 };
            Interval _6035 = Interval{ _6854, _6855 };
            Interval _6982 = jet_mul_derivative(_6034, _6035, intervalFailed, optical_product_upper);
            Interval _6036 = _6982;
            Interval _6037 = Interval{ _6941.z, _6941.z };
            Interval _6038 = Interval{ _6856, _6857 };
            Interval _6985 = jet_mul_derivative(_6037, _6038, intervalFailed, optical_product_upper);
            Interval _6039 = _6985;
            Interval _6986 = jet_add_derivative(_6036, _6039, intervalFailed);
            Interval _6040 = Interval{ 0.0, 0.0 };
            Interval _6041 = Interval{ _6854, _6855 };
            Interval _6988 = jet_mul_derivative(_6040, _6041, intervalFailed, optical_product_upper);
            Interval _6042 = _6988;
            Interval _6043 = Interval{ _6941.z, _6941.z };
            Interval _6044 = Interval{ _6858, _6859 };
            Interval _6991 = jet_mul_derivative(_6043, _6044, intervalFailed, optical_product_upper);
            Interval _6045 = _6991;
            Interval _6992 = jet_add_derivative(_6042, _6045, intervalFailed);
            Interval _6026 = _6975;
            Interval _6027 = _6980;
            Interval _6993 = iadd(_6026, _6027, intervalFailed);
            Interval _6028 = _6976;
            Interval _6029 = _6986;
            Interval _6994 = jet_add_derivative(_6028, _6029, intervalFailed);
            Interval _6030 = _6977;
            Interval _6031 = _6992;
            Interval _6995 = jet_add_derivative(_6030, _6031, intervalFailed);
            Interval _6012 = _6369;
            Interval _6013 = Interval{ _6842, _6843 };
            Interval _6997 = imul(_6012, _6013, intervalFailed, optical_product_upper);
            Interval _6014 = _6397;
            Interval _6015 = Interval{ _6842, _6843 };
            Interval _6999 = jet_mul_derivative(_6014, _6015, intervalFailed, optical_product_upper);
            Interval _6016 = _6999;
            Interval _6017 = _6369;
            Interval _6018 = Interval{ _6844, _6845 };
            Interval _7001 = jet_mul_derivative(_6017, _6018, intervalFailed, optical_product_upper);
            Interval _6019 = _7001;
            Interval _7002 = jet_add_derivative(_6016, _6019, intervalFailed);
            Interval _6020 = _6410;
            Interval _6021 = Interval{ _6842, _6843 };
            Interval _7004 = jet_mul_derivative(_6020, _6021, intervalFailed, optical_product_upper);
            Interval _6022 = _7004;
            Interval _6023 = _6369;
            Interval _6024 = Interval{ _6846, _6847 };
            Interval _7006 = jet_mul_derivative(_6023, _6024, intervalFailed, optical_product_upper);
            Interval _6025 = _7006;
            Interval _7007 = jet_add_derivative(_6022, _6025, intervalFailed);
            Interval _5998 = _6436;
            Interval _5999 = Interval{ _6848, _6849 };
            Interval _7009 = imul(_5998, _5999, intervalFailed, optical_product_upper);
            Interval _6000 = _6464;
            Interval _6001 = Interval{ _6848, _6849 };
            Interval _7011 = jet_mul_derivative(_6000, _6001, intervalFailed, optical_product_upper);
            Interval _6002 = _7011;
            Interval _6003 = _6436;
            Interval _6004 = Interval{ _6850, _6851 };
            Interval _7013 = jet_mul_derivative(_6003, _6004, intervalFailed, optical_product_upper);
            Interval _6005 = _7013;
            Interval _7014 = jet_add_derivative(_6002, _6005, intervalFailed);
            Interval _6006 = _6477;
            Interval _6007 = Interval{ _6848, _6849 };
            Interval _7016 = jet_mul_derivative(_6006, _6007, intervalFailed, optical_product_upper);
            Interval _6008 = _7016;
            Interval _6009 = _6436;
            Interval _6010 = Interval{ _6852, _6853 };
            Interval _7018 = jet_mul_derivative(_6009, _6010, intervalFailed, optical_product_upper);
            Interval _6011 = _7018;
            Interval _7019 = jet_add_derivative(_6008, _6011, intervalFailed);
            Interval _5992 = _6997;
            Interval _5993 = _7009;
            Interval _7020 = iadd(_5992, _5993, intervalFailed);
            Interval _5994 = _7002;
            Interval _5995 = _7014;
            Interval _7021 = jet_add_derivative(_5994, _5995, intervalFailed);
            Interval _5996 = _7007;
            Interval _5997 = _7019;
            Interval _7022 = jet_add_derivative(_5996, _5997, intervalFailed);
            Interval _5978 = Interval{ 1.0, 1.0 };
            Interval _5979 = Interval{ _6854, _6855 };
            Interval _7024 = imul(_5978, _5979, intervalFailed, optical_product_upper);
            Interval _5980 = Interval{ 0.0, 0.0 };
            Interval _5981 = Interval{ _6854, _6855 };
            Interval _7026 = jet_mul_derivative(_5980, _5981, intervalFailed, optical_product_upper);
            Interval _5982 = _7026;
            Interval _5983 = Interval{ 1.0, 1.0 };
            Interval _5984 = Interval{ _6856, _6857 };
            Interval _7028 = jet_mul_derivative(_5983, _5984, intervalFailed, optical_product_upper);
            Interval _5985 = _7028;
            Interval _7029 = jet_add_derivative(_5982, _5985, intervalFailed);
            Interval _5986 = Interval{ 0.0, 0.0 };
            Interval _5987 = Interval{ _6854, _6855 };
            Interval _7031 = jet_mul_derivative(_5986, _5987, intervalFailed, optical_product_upper);
            Interval _5988 = _7031;
            Interval _5989 = Interval{ 1.0, 1.0 };
            Interval _5990 = Interval{ _6858, _6859 };
            Interval _7033 = jet_mul_derivative(_5989, _5990, intervalFailed, optical_product_upper);
            Interval _5991 = _7033;
            Interval _7034 = jet_add_derivative(_5988, _5991, intervalFailed);
            Interval _5972 = _7020;
            Interval _5973 = _7024;
            Interval _7035 = iadd(_5972, _5973, intervalFailed);
            Interval _5974 = _7021;
            Interval _5975 = _7029;
            Interval _7036 = jet_add_derivative(_5974, _5975, intervalFailed);
            Interval _5976 = _7022;
            Interval _5977 = _7034;
            Interval _7037 = jet_add_derivative(_5976, _5977, intervalFailed);
            Interval _5958 = _6993;
            Interval _5959 = _7035;
            Interval _7039 = idiv(_5958, _5959, intervalFailed, interval_divide_upper);
            bool _7048;
            if (!intervalFailed)
            {
                _7048 = intervalFailed;
            }
            else
            {
                _7048 = false;
            }
            bool _7053;
            if (_7048)
            {
                _7053 = jetFailureSite == 0u;
            }
            else
            {
                _7053 = false;
            }
            if (_7053)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_6993.lo, _6993.hi, _7035.lo, _7035.hi);
            }
            Interval _5960 = _6994;
            Interval _5961 = _7039;
            Interval _5962 = _7036;
            Interval _7057 = jet_mul_derivative(_5961, _5962, intervalFailed, optical_product_upper);
            Interval _5963 = Interval{ as_type<float>(as_type<uint>(_7057.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7057.lo) ^ 2147483648u) };
            Interval _7067 = jet_add_derivative(_5960, _5963, intervalFailed);
            Interval _5964 = _7067;
            Interval _5965 = _7035;
            Interval _7068 = jet_div_derivative(_5964, _5965, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5966 = _6995;
            Interval _5967 = _7039;
            Interval _5968 = _7037;
            Interval _7069 = jet_mul_derivative(_5967, _5968, intervalFailed, optical_product_upper);
            Interval _5969 = Interval{ as_type<float>(as_type<uint>(_7069.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7069.lo) ^ 2147483648u) };
            Interval _7079 = jet_add_derivative(_5966, _5969, intervalFailed);
            Interval _5970 = _7079;
            Interval _5971 = _7035;
            Interval _7080 = jet_div_derivative(_5970, _5971, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            ray.depth = OpticalJet{ _7039, _7068, _7080 };
            Interval _5944 = ray.depth.v;
            Interval _5945 = _6558;
            Interval _7088 = imul(_5944, _5945, intervalFailed, optical_product_upper);
            Interval _5946 = ray.depth.dx;
            Interval _5947 = _6558;
            Interval _7089 = jet_mul_derivative(_5946, _5947, intervalFailed, optical_product_upper);
            Interval _5948 = _7089;
            Interval _5949 = ray.depth.v;
            Interval _5950 = _6596;
            Interval _7090 = jet_mul_derivative(_5949, _5950, intervalFailed, optical_product_upper);
            Interval _5951 = _7090;
            Interval _7091 = jet_add_derivative(_5948, _5951, intervalFailed);
            Interval _5952 = ray.depth.dy;
            Interval _5953 = _6558;
            Interval _7092 = jet_mul_derivative(_5952, _5953, intervalFailed, optical_product_upper);
            Interval _5954 = _7092;
            Interval _5955 = ray.depth.v;
            Interval _5956 = _6597;
            Interval _7093 = jet_mul_derivative(_5955, _5956, intervalFailed, optical_product_upper);
            Interval _5957 = _7093;
            Interval _7094 = jet_add_derivative(_5954, _5957, intervalFailed);
            Interval _5930 = _6369;
            Interval _5931 = ray.depth.v;
            Interval _7106 = imul(_5930, _5931, intervalFailed, optical_product_upper);
            Interval _5932 = _6397;
            Interval _5933 = ray.depth.v;
            Interval _7107 = jet_mul_derivative(_5932, _5933, intervalFailed, optical_product_upper);
            Interval _5934 = _7107;
            Interval _5935 = _6369;
            Interval _5936 = ray.depth.dx;
            Interval _7108 = jet_mul_derivative(_5935, _5936, intervalFailed, optical_product_upper);
            Interval _5937 = _7108;
            Interval _7109 = jet_add_derivative(_5934, _5937, intervalFailed);
            Interval _5938 = _6410;
            Interval _5939 = ray.depth.v;
            Interval _7110 = jet_mul_derivative(_5938, _5939, intervalFailed, optical_product_upper);
            Interval _5940 = _7110;
            Interval _5941 = _6369;
            Interval _5942 = ray.depth.dy;
            Interval _7111 = jet_mul_derivative(_5941, _5942, intervalFailed, optical_product_upper);
            Interval _5943 = _7111;
            Interval _7112 = jet_add_derivative(_5940, _5943, intervalFailed);
            Interval _5916 = _6436;
            Interval _5917 = ray.depth.v;
            Interval _7116 = imul(_5916, _5917, intervalFailed, optical_product_upper);
            Interval _5918 = _6464;
            Interval _5919 = ray.depth.v;
            Interval _7117 = jet_mul_derivative(_5918, _5919, intervalFailed, optical_product_upper);
            Interval _5920 = _7117;
            Interval _5921 = _6436;
            Interval _5922 = ray.depth.dx;
            Interval _7118 = jet_mul_derivative(_5921, _5922, intervalFailed, optical_product_upper);
            Interval _5923 = _7118;
            Interval _7119 = jet_add_derivative(_5920, _5923, intervalFailed);
            Interval _5924 = _6477;
            Interval _5925 = ray.depth.v;
            Interval _7120 = jet_mul_derivative(_5924, _5925, intervalFailed, optical_product_upper);
            Interval _5926 = _7120;
            Interval _5927 = _6436;
            Interval _5928 = ray.depth.dy;
            Interval _7121 = jet_mul_derivative(_5927, _5928, intervalFailed, optical_product_upper);
            Interval _5929 = _7121;
            Interval _7122 = jet_add_derivative(_5926, _5929, intervalFailed);
            Interval _5902 = Interval{ 1.0, 1.0 };
            Interval _5903 = ray.depth.v;
            Interval _7126 = imul(_5902, _5903, intervalFailed, optical_product_upper);
            Interval _5904 = Interval{ 0.0, 0.0 };
            Interval _5905 = ray.depth.v;
            Interval _7127 = jet_mul_derivative(_5904, _5905, intervalFailed, optical_product_upper);
            Interval _5906 = _7127;
            Interval _5907 = Interval{ 1.0, 1.0 };
            Interval _5908 = ray.depth.dx;
            Interval _7128 = jet_mul_derivative(_5907, _5908, intervalFailed, optical_product_upper);
            Interval _5909 = _7128;
            Interval _7129 = jet_add_derivative(_5906, _5909, intervalFailed);
            Interval _5910 = Interval{ 0.0, 0.0 };
            Interval _5911 = ray.depth.v;
            Interval _7130 = jet_mul_derivative(_5910, _5911, intervalFailed, optical_product_upper);
            Interval _5912 = _7130;
            Interval _5913 = Interval{ 1.0, 1.0 };
            Interval _5914 = ray.depth.dy;
            Interval _7131 = jet_mul_derivative(_5913, _5914, intervalFailed, optical_product_upper);
            Interval _5915 = _7131;
            Interval _7132 = jet_add_derivative(_5912, _5915, intervalFailed);
            hit = OpticalJet3{ OpticalJet{ _7106, _7109, _7112 }, OpticalJet{ _7116, _7119, _7122 }, OpticalJet{ _7126, _7129, _7132 } };
            float3 _7141;
            if (_6776)
            {
                _7141 = liquid.planePoint.xyz;
            }
            else
            {
                _7141 = _6788.xyz;
            }
            Interval _5888 = Interval{ _7141.x, _7141.x };
            Interval _5889 = Interval{ _6842, _6843 };
            Interval _7147 = imul(_5888, _5889, intervalFailed, optical_product_upper);
            Interval _5890 = Interval{ 0.0, 0.0 };
            Interval _5891 = Interval{ _6842, _6843 };
            Interval _7149 = jet_mul_derivative(_5890, _5891, intervalFailed, optical_product_upper);
            Interval _5892 = _7149;
            Interval _5893 = Interval{ _7141.x, _7141.x };
            Interval _5894 = Interval{ _6844, _6845 };
            Interval _7152 = jet_mul_derivative(_5893, _5894, intervalFailed, optical_product_upper);
            Interval _5895 = _7152;
            Interval _7153 = jet_add_derivative(_5892, _5895, intervalFailed);
            Interval _5896 = Interval{ 0.0, 0.0 };
            Interval _5897 = Interval{ _6842, _6843 };
            Interval _7155 = jet_mul_derivative(_5896, _5897, intervalFailed, optical_product_upper);
            Interval _5898 = _7155;
            Interval _5899 = Interval{ _7141.x, _7141.x };
            Interval _5900 = Interval{ _6846, _6847 };
            Interval _7158 = jet_mul_derivative(_5899, _5900, intervalFailed, optical_product_upper);
            Interval _5901 = _7158;
            Interval _7159 = jet_add_derivative(_5898, _5901, intervalFailed);
            Interval _5874 = Interval{ _7141.y, _7141.y };
            Interval _5875 = Interval{ _6848, _6849 };
            Interval _7162 = imul(_5874, _5875, intervalFailed, optical_product_upper);
            Interval _5876 = Interval{ 0.0, 0.0 };
            Interval _5877 = Interval{ _6848, _6849 };
            Interval _7164 = jet_mul_derivative(_5876, _5877, intervalFailed, optical_product_upper);
            Interval _5878 = _7164;
            Interval _5879 = Interval{ _7141.y, _7141.y };
            Interval _5880 = Interval{ _6850, _6851 };
            Interval _7167 = jet_mul_derivative(_5879, _5880, intervalFailed, optical_product_upper);
            Interval _5881 = _7167;
            Interval _7168 = jet_add_derivative(_5878, _5881, intervalFailed);
            Interval _5882 = Interval{ 0.0, 0.0 };
            Interval _5883 = Interval{ _6848, _6849 };
            Interval _7170 = jet_mul_derivative(_5882, _5883, intervalFailed, optical_product_upper);
            Interval _5884 = _7170;
            Interval _5885 = Interval{ _7141.y, _7141.y };
            Interval _5886 = Interval{ _6852, _6853 };
            Interval _7173 = jet_mul_derivative(_5885, _5886, intervalFailed, optical_product_upper);
            Interval _5887 = _7173;
            Interval _7174 = jet_add_derivative(_5884, _5887, intervalFailed);
            Interval _5868 = _7147;
            Interval _5869 = _7162;
            Interval _7175 = iadd(_5868, _5869, intervalFailed);
            Interval _5870 = _7153;
            Interval _5871 = _7168;
            Interval _7176 = jet_add_derivative(_5870, _5871, intervalFailed);
            Interval _5872 = _7159;
            Interval _5873 = _7174;
            Interval _7177 = jet_add_derivative(_5872, _5873, intervalFailed);
            Interval _5854 = Interval{ _7141.z, _7141.z };
            Interval _5855 = Interval{ _6854, _6855 };
            Interval _7180 = imul(_5854, _5855, intervalFailed, optical_product_upper);
            Interval _5856 = Interval{ 0.0, 0.0 };
            Interval _5857 = Interval{ _6854, _6855 };
            Interval _7182 = jet_mul_derivative(_5856, _5857, intervalFailed, optical_product_upper);
            Interval _5858 = _7182;
            Interval _5859 = Interval{ _7141.z, _7141.z };
            Interval _5860 = Interval{ _6856, _6857 };
            Interval _7185 = jet_mul_derivative(_5859, _5860, intervalFailed, optical_product_upper);
            Interval _5861 = _7185;
            Interval _7186 = jet_add_derivative(_5858, _5861, intervalFailed);
            Interval _5862 = Interval{ 0.0, 0.0 };
            Interval _5863 = Interval{ _6854, _6855 };
            Interval _7188 = jet_mul_derivative(_5862, _5863, intervalFailed, optical_product_upper);
            Interval _5864 = _7188;
            Interval _5865 = Interval{ _7141.z, _7141.z };
            Interval _5866 = Interval{ _6858, _6859 };
            Interval _7191 = jet_mul_derivative(_5865, _5866, intervalFailed, optical_product_upper);
            Interval _5867 = _7191;
            Interval _7192 = jet_add_derivative(_5864, _5867, intervalFailed);
            Interval _5848 = _7175;
            Interval _5849 = _7180;
            Interval _7193 = iadd(_5848, _5849, intervalFailed);
            Interval _5850 = _7176;
            Interval _5851 = _7186;
            Interval _7194 = jet_add_derivative(_5850, _5851, intervalFailed);
            Interval _5852 = _7177;
            Interval _5853 = _7192;
            Interval _7195 = jet_add_derivative(_5852, _5853, intervalFailed);
            Interval _5834 = _7193;
            Interval _5835 = _6914;
            Interval _7197 = idiv(_5834, _5835, intervalFailed, interval_divide_upper);
            bool _7206;
            if (!intervalFailed)
            {
                _7206 = intervalFailed;
            }
            else
            {
                _7206 = false;
            }
            bool _7211;
            if (_7206)
            {
                _7211 = jetFailureSite == 0u;
            }
            else
            {
                _7211 = false;
            }
            if (_7211)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7193.lo, _7193.hi, _6914.lo, _6914.hi);
            }
            Interval _5836 = _7194;
            Interval _5837 = _7197;
            Interval _5838 = _6915;
            Interval _7215 = jet_mul_derivative(_5837, _5838, intervalFailed, optical_product_upper);
            Interval _5839 = Interval{ as_type<float>(as_type<uint>(_7215.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7215.lo) ^ 2147483648u) };
            Interval _7225 = jet_add_derivative(_5836, _5839, intervalFailed);
            Interval _5840 = _7225;
            Interval _5841 = _6914;
            Interval _7226 = jet_div_derivative(_5840, _5841, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5842 = _7195;
            Interval _5843 = _7197;
            Interval _5844 = _6916;
            Interval _7227 = jet_mul_derivative(_5843, _5844, intervalFailed, optical_product_upper);
            Interval _5845 = Interval{ as_type<float>(as_type<uint>(_7227.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7227.lo) ^ 2147483648u) };
            Interval _7237 = jet_add_derivative(_5842, _5845, intervalFailed);
            Interval _5846 = _7237;
            Interval _5847 = _6914;
            Interval _7238 = jet_div_derivative(_5846, _5847, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5820 = _7197;
            Interval _5821 = Interval{ _6714, _6716 };
            Interval _7245 = imul(_5820, _5821, intervalFailed, optical_product_upper);
            Interval _5822 = _7226;
            Interval _5823 = Interval{ _6714, _6716 };
            Interval _7247 = jet_mul_derivative(_5822, _5823, intervalFailed, optical_product_upper);
            Interval _5824 = _7247;
            Interval _5825 = _7197;
            Interval _5826 = Interval{ _6718, _6720 };
            Interval _7249 = jet_mul_derivative(_5825, _5826, intervalFailed, optical_product_upper);
            Interval _5827 = _7249;
            Interval _7250 = jet_add_derivative(_5824, _5827, intervalFailed);
            Interval _5828 = _7238;
            Interval _5829 = Interval{ _6714, _6716 };
            Interval _7252 = jet_mul_derivative(_5828, _5829, intervalFailed, optical_product_upper);
            Interval _5830 = _7252;
            Interval _5831 = _7197;
            Interval _5832 = Interval{ _6722, _6724 };
            Interval _7254 = jet_mul_derivative(_5831, _5832, intervalFailed, optical_product_upper);
            Interval _5833 = _7254;
            Interval _7255 = jet_add_derivative(_5830, _5833, intervalFailed);
            ray.depth = OpticalJet{ Interval{ precise::min(ray.depth.v.lo, _7245.lo), precise::max(ray.depth.v.hi, _7245.hi) }, Interval{ precise::min(ray.depth.dx.lo, _7250.lo), precise::max(ray.depth.dx.hi, _7250.hi) }, Interval{ precise::min(ray.depth.dy.lo, _7255.lo), precise::max(ray.depth.dy.hi, _7255.hi) } };
            bool _7286;
            if ((isunordered(_7088.lo, 0.0) || _7088.lo > 0.0))
            {
                _7286 = ray.depth.v.lo < receiver.extentClip.z;
            }
            else
            {
                _7286 = true;
            }
            bool _7294;
            if (!_7286)
            {
                _7294 = ray.depth.v.hi > receiver.extentClip.w;
            }
            else
            {
                _7294 = true;
            }
            if (_7294)
            {
                return false;
            }
            _7530 = _7088.lo;
            _7531 = _7088.hi;
            _7532 = _7091.lo;
            _7533 = _7091.hi;
            _7534 = _7094.lo;
            _7535 = _7094.hi;
        }
        else
        {
            float3 _7299;
            if (_6776)
            {
                _7299 = liquid.planePoint.xyz;
            }
            else
            {
                _7299 = _6788.xyz;
            }
            Interval _5814 = Interval{ _7299.x, _7299.x };
            Interval _5815 = Interval{ as_type<float>(as_type<uint>(_6728) ^ 2147483648u), as_type<float>(as_type<uint>(_6726) ^ 2147483648u) };
            Interval _7359 = iadd(_5814, _5815, intervalFailed);
            Interval _5816 = Interval{ 0.0, 0.0 };
            Interval _5817 = Interval{ as_type<float>(as_type<uint>(_6732) ^ 2147483648u), as_type<float>(as_type<uint>(_6730) ^ 2147483648u) };
            Interval _7361 = jet_add_derivative(_5816, _5817, intervalFailed);
            Interval _5818 = Interval{ 0.0, 0.0 };
            Interval _5819 = Interval{ as_type<float>(as_type<uint>(_6736) ^ 2147483648u), as_type<float>(as_type<uint>(_6734) ^ 2147483648u) };
            Interval _7363 = jet_add_derivative(_5818, _5819, intervalFailed);
            Interval _5808 = Interval{ _7299.y, _7299.y };
            Interval _5809 = Interval{ as_type<float>(as_type<uint>(_6740) ^ 2147483648u), as_type<float>(as_type<uint>(_6738) ^ 2147483648u) };
            Interval _7366 = iadd(_5808, _5809, intervalFailed);
            Interval _5810 = Interval{ 0.0, 0.0 };
            Interval _5811 = Interval{ as_type<float>(as_type<uint>(_6744) ^ 2147483648u), as_type<float>(as_type<uint>(_6742) ^ 2147483648u) };
            Interval _7368 = jet_add_derivative(_5810, _5811, intervalFailed);
            Interval _5812 = Interval{ 0.0, 0.0 };
            Interval _5813 = Interval{ as_type<float>(as_type<uint>(_6748) ^ 2147483648u), as_type<float>(as_type<uint>(_6746) ^ 2147483648u) };
            Interval _7370 = jet_add_derivative(_5812, _5813, intervalFailed);
            Interval _5802 = Interval{ _7299.z, _7299.z };
            Interval _5803 = Interval{ as_type<float>(as_type<uint>(_6752) ^ 2147483648u), as_type<float>(as_type<uint>(_6750) ^ 2147483648u) };
            Interval _7373 = iadd(_5802, _5803, intervalFailed);
            Interval _5804 = Interval{ 0.0, 0.0 };
            Interval _5805 = Interval{ as_type<float>(as_type<uint>(_6756) ^ 2147483648u), as_type<float>(as_type<uint>(_6754) ^ 2147483648u) };
            Interval _7375 = jet_add_derivative(_5804, _5805, intervalFailed);
            Interval _5806 = Interval{ 0.0, 0.0 };
            Interval _5807 = Interval{ as_type<float>(as_type<uint>(_6760) ^ 2147483648u), as_type<float>(as_type<uint>(_6758) ^ 2147483648u) };
            Interval _7377 = jet_add_derivative(_5806, _5807, intervalFailed);
            Interval _5788 = _7359;
            Interval _5789 = Interval{ _6842, _6843 };
            Interval _7379 = imul(_5788, _5789, intervalFailed, optical_product_upper);
            Interval _5790 = _7361;
            Interval _5791 = Interval{ _6842, _6843 };
            Interval _7381 = jet_mul_derivative(_5790, _5791, intervalFailed, optical_product_upper);
            Interval _5792 = _7381;
            Interval _5793 = _7359;
            Interval _5794 = Interval{ _6844, _6845 };
            Interval _7383 = jet_mul_derivative(_5793, _5794, intervalFailed, optical_product_upper);
            Interval _5795 = _7383;
            Interval _7384 = jet_add_derivative(_5792, _5795, intervalFailed);
            Interval _5796 = _7363;
            Interval _5797 = Interval{ _6842, _6843 };
            Interval _7386 = jet_mul_derivative(_5796, _5797, intervalFailed, optical_product_upper);
            Interval _5798 = _7386;
            Interval _5799 = _7359;
            Interval _5800 = Interval{ _6846, _6847 };
            Interval _7388 = jet_mul_derivative(_5799, _5800, intervalFailed, optical_product_upper);
            Interval _5801 = _7388;
            Interval _7389 = jet_add_derivative(_5798, _5801, intervalFailed);
            Interval _5774 = _7366;
            Interval _5775 = Interval{ _6848, _6849 };
            Interval _7391 = imul(_5774, _5775, intervalFailed, optical_product_upper);
            Interval _5776 = _7368;
            Interval _5777 = Interval{ _6848, _6849 };
            Interval _7393 = jet_mul_derivative(_5776, _5777, intervalFailed, optical_product_upper);
            Interval _5778 = _7393;
            Interval _5779 = _7366;
            Interval _5780 = Interval{ _6850, _6851 };
            Interval _7395 = jet_mul_derivative(_5779, _5780, intervalFailed, optical_product_upper);
            Interval _5781 = _7395;
            Interval _7396 = jet_add_derivative(_5778, _5781, intervalFailed);
            Interval _5782 = _7370;
            Interval _5783 = Interval{ _6848, _6849 };
            Interval _7398 = jet_mul_derivative(_5782, _5783, intervalFailed, optical_product_upper);
            Interval _5784 = _7398;
            Interval _5785 = _7366;
            Interval _5786 = Interval{ _6852, _6853 };
            Interval _7400 = jet_mul_derivative(_5785, _5786, intervalFailed, optical_product_upper);
            Interval _5787 = _7400;
            Interval _7401 = jet_add_derivative(_5784, _5787, intervalFailed);
            Interval _5768 = _7379;
            Interval _5769 = _7391;
            Interval _7402 = iadd(_5768, _5769, intervalFailed);
            Interval _5770 = _7384;
            Interval _5771 = _7396;
            Interval _7403 = jet_add_derivative(_5770, _5771, intervalFailed);
            Interval _5772 = _7389;
            Interval _5773 = _7401;
            Interval _7404 = jet_add_derivative(_5772, _5773, intervalFailed);
            Interval _5754 = _7373;
            Interval _5755 = Interval{ _6854, _6855 };
            Interval _7406 = imul(_5754, _5755, intervalFailed, optical_product_upper);
            Interval _5756 = _7375;
            Interval _5757 = Interval{ _6854, _6855 };
            Interval _7408 = jet_mul_derivative(_5756, _5757, intervalFailed, optical_product_upper);
            Interval _5758 = _7408;
            Interval _5759 = _7373;
            Interval _5760 = Interval{ _6856, _6857 };
            Interval _7410 = jet_mul_derivative(_5759, _5760, intervalFailed, optical_product_upper);
            Interval _5761 = _7410;
            Interval _7411 = jet_add_derivative(_5758, _5761, intervalFailed);
            Interval _5762 = _7377;
            Interval _5763 = Interval{ _6854, _6855 };
            Interval _7413 = jet_mul_derivative(_5762, _5763, intervalFailed, optical_product_upper);
            Interval _5764 = _7413;
            Interval _5765 = _7373;
            Interval _5766 = Interval{ _6858, _6859 };
            Interval _7415 = jet_mul_derivative(_5765, _5766, intervalFailed, optical_product_upper);
            Interval _5767 = _7415;
            Interval _7416 = jet_add_derivative(_5764, _5767, intervalFailed);
            Interval _5748 = _7402;
            Interval _5749 = _7406;
            Interval _7417 = iadd(_5748, _5749, intervalFailed);
            Interval _5750 = _7403;
            Interval _5751 = _7411;
            Interval _7418 = jet_add_derivative(_5750, _5751, intervalFailed);
            Interval _5752 = _7404;
            Interval _5753 = _7416;
            Interval _7419 = jet_add_derivative(_5752, _5753, intervalFailed);
            Interval _5734 = _7417;
            Interval _5735 = _6914;
            Interval _7421 = idiv(_5734, _5735, intervalFailed, interval_divide_upper);
            bool _7430;
            if (!intervalFailed)
            {
                _7430 = intervalFailed;
            }
            else
            {
                _7430 = false;
            }
            bool _7435;
            if (_7430)
            {
                _7435 = jetFailureSite == 0u;
            }
            else
            {
                _7435 = false;
            }
            if (_7435)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7417.lo, _7417.hi, _6914.lo, _6914.hi);
            }
            Interval _5736 = _7418;
            Interval _5737 = _7421;
            Interval _5738 = _6915;
            Interval _7439 = jet_mul_derivative(_5737, _5738, intervalFailed, optical_product_upper);
            Interval _5739 = Interval{ as_type<float>(as_type<uint>(_7439.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7439.lo) ^ 2147483648u) };
            Interval _7449 = jet_add_derivative(_5736, _5739, intervalFailed);
            Interval _5740 = _7449;
            Interval _5741 = _6914;
            Interval _7450 = jet_div_derivative(_5740, _5741, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5742 = _7419;
            Interval _5743 = _7421;
            Interval _5744 = _6916;
            Interval _7451 = jet_mul_derivative(_5743, _5744, intervalFailed, optical_product_upper);
            Interval _5745 = Interval{ as_type<float>(as_type<uint>(_7451.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7451.lo) ^ 2147483648u) };
            Interval _7461 = jet_add_derivative(_5742, _5745, intervalFailed);
            Interval _5746 = _7461;
            Interval _5747 = _6914;
            Interval _7462 = jet_div_derivative(_5746, _5747, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            bool _7471;
            if ((isunordered(_7421.lo, _6680) || _7421.lo > _6680))
            {
                _7471 = _7421.hi >= 65536.0;
            }
            else
            {
                _7471 = true;
            }
            if (_7471)
            {
                return false;
            }
            Interval _5720 = Interval{ _6690, _6692 };
            Interval _5721 = _7421;
            Interval _7473 = imul(_5720, _5721, intervalFailed, optical_product_upper);
            Interval _5722 = Interval{ _6694, _6696 };
            Interval _5723 = _7421;
            Interval _7475 = jet_mul_derivative(_5722, _5723, intervalFailed, optical_product_upper);
            Interval _5724 = _7475;
            Interval _5725 = Interval{ _6690, _6692 };
            Interval _5726 = _7450;
            Interval _7477 = jet_mul_derivative(_5725, _5726, intervalFailed, optical_product_upper);
            Interval _5727 = _7477;
            Interval _7478 = jet_add_derivative(_5724, _5727, intervalFailed);
            Interval _5728 = Interval{ _6698, _6700 };
            Interval _5729 = _7421;
            Interval _7480 = jet_mul_derivative(_5728, _5729, intervalFailed, optical_product_upper);
            Interval _5730 = _7480;
            Interval _5731 = Interval{ _6690, _6692 };
            Interval _5732 = _7462;
            Interval _7482 = jet_mul_derivative(_5731, _5732, intervalFailed, optical_product_upper);
            Interval _5733 = _7482;
            Interval _7483 = jet_add_derivative(_5730, _5733, intervalFailed);
            Interval _5706 = Interval{ _6702, _6704 };
            Interval _5707 = _7421;
            Interval _7485 = imul(_5706, _5707, intervalFailed, optical_product_upper);
            Interval _5708 = Interval{ _6706, _6708 };
            Interval _5709 = _7421;
            Interval _7487 = jet_mul_derivative(_5708, _5709, intervalFailed, optical_product_upper);
            Interval _5710 = _7487;
            Interval _5711 = Interval{ _6702, _6704 };
            Interval _5712 = _7450;
            Interval _7489 = jet_mul_derivative(_5711, _5712, intervalFailed, optical_product_upper);
            Interval _5713 = _7489;
            Interval _7490 = jet_add_derivative(_5710, _5713, intervalFailed);
            Interval _5714 = Interval{ _6710, _6712 };
            Interval _5715 = _7421;
            Interval _7492 = jet_mul_derivative(_5714, _5715, intervalFailed, optical_product_upper);
            Interval _5716 = _7492;
            Interval _5717 = Interval{ _6702, _6704 };
            Interval _5718 = _7462;
            Interval _7494 = jet_mul_derivative(_5717, _5718, intervalFailed, optical_product_upper);
            Interval _5719 = _7494;
            Interval _7495 = jet_add_derivative(_5716, _5719, intervalFailed);
            Interval _5692 = Interval{ _6714, _6716 };
            Interval _5693 = _7421;
            Interval _7497 = imul(_5692, _5693, intervalFailed, optical_product_upper);
            Interval _5694 = Interval{ _6718, _6720 };
            Interval _5695 = _7421;
            Interval _7499 = jet_mul_derivative(_5694, _5695, intervalFailed, optical_product_upper);
            Interval _5696 = _7499;
            Interval _5697 = Interval{ _6714, _6716 };
            Interval _5698 = _7450;
            Interval _7501 = jet_mul_derivative(_5697, _5698, intervalFailed, optical_product_upper);
            Interval _5699 = _7501;
            Interval _7502 = jet_add_derivative(_5696, _5699, intervalFailed);
            Interval _5700 = Interval{ _6722, _6724 };
            Interval _5701 = _7421;
            Interval _7504 = jet_mul_derivative(_5700, _5701, intervalFailed, optical_product_upper);
            Interval _5702 = _7504;
            Interval _5703 = Interval{ _6714, _6716 };
            Interval _5704 = _7462;
            Interval _7506 = jet_mul_derivative(_5703, _5704, intervalFailed, optical_product_upper);
            Interval _5705 = _7506;
            Interval _7507 = jet_add_derivative(_5702, _5705, intervalFailed);
            Interval _5686 = Interval{ _6726, _6728 };
            Interval _5687 = _7473;
            Interval _7509 = iadd(_5686, _5687, intervalFailed);
            Interval _5688 = Interval{ _6730, _6732 };
            Interval _5689 = _7478;
            Interval _7511 = jet_add_derivative(_5688, _5689, intervalFailed);
            Interval _5690 = Interval{ _6734, _6736 };
            Interval _5691 = _7483;
            Interval _7513 = jet_add_derivative(_5690, _5691, intervalFailed);
            Interval _5680 = Interval{ _6738, _6740 };
            Interval _5681 = _7485;
            Interval _7515 = iadd(_5680, _5681, intervalFailed);
            Interval _5682 = Interval{ _6742, _6744 };
            Interval _5683 = _7490;
            Interval _7517 = jet_add_derivative(_5682, _5683, intervalFailed);
            Interval _5684 = Interval{ _6746, _6748 };
            Interval _5685 = _7495;
            Interval _7519 = jet_add_derivative(_5684, _5685, intervalFailed);
            Interval _5674 = Interval{ _6750, _6752 };
            Interval _5675 = _7497;
            Interval _7521 = iadd(_5674, _5675, intervalFailed);
            Interval _5676 = Interval{ _6754, _6756 };
            Interval _5677 = _7502;
            Interval _7523 = jet_add_derivative(_5676, _5677, intervalFailed);
            Interval _5678 = Interval{ _6758, _6760 };
            Interval _5679 = _7507;
            Interval _7525 = jet_add_derivative(_5678, _5679, intervalFailed);
            hit = OpticalJet3{ OpticalJet{ _7509, _7511, _7513 }, OpticalJet{ _7515, _7517, _7519 }, OpticalJet{ _7521, _7523, _7525 } };
            _7530 = _7421.lo;
            _7531 = _7421.hi;
            _7532 = _7450.lo;
            _7533 = _7450.hi;
            _7534 = _7462.lo;
            _7535 = _7462.hi;
        }
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
        float _7862;
        float _7863;
        float _7864;
        float _7865;
        float _7866;
        float _7867;
        if (_6776)
        {
            float _7677;
            float _7678;
            float _7679;
            float _7680;
            float _7681;
            float _7682;
            if (_6766)
            {
                _7677 = _7530;
                _7678 = _7531;
                _7679 = _7532;
                _7680 = _7533;
                _7681 = _7534;
                _7682 = _7535;
            }
            else
            {
                bool _7549;
                if (hit.x.v.lo <= 0.0)
                {
                    _7549 = hit.x.v.hi >= 0.0;
                }
                else
                {
                    _7549 = false;
                }
                float _7556;
                if (_7549)
                {
                    _7556 = 0.0;
                }
                else
                {
                    _7556 = precise::min(abs(hit.x.v.lo), abs(hit.x.v.hi));
                }
                float _7559 = precise::max(abs(hit.x.v.lo), abs(hit.x.v.hi));
                float _5664 = spvFMul(_7556, _7556);
                float _7560 = interval_down(_5664, intervalFailed);
                float _5665 = spvFMul(_7559, _7559);
                float _7562 = interval_up(_5665, intervalFailed);
                Interval _5666 = Interval{ 2.0, 2.0 };
                Interval _5667 = hit.x.v;
                Interval _7563 = imul(_5666, _5667, intervalFailed, optical_product_upper);
                Interval _5668 = _7563;
                Interval _5669 = hit.x.dx;
                Interval _7564 = jet_mul_derivative(_5668, _5669, intervalFailed, optical_product_upper);
                Interval _5670 = Interval{ 2.0, 2.0 };
                Interval _5671 = hit.x.v;
                Interval _7565 = imul(_5670, _5671, intervalFailed, optical_product_upper);
                Interval _5672 = _7565;
                Interval _5673 = hit.x.dy;
                Interval _7566 = jet_mul_derivative(_5672, _5673, intervalFailed, optical_product_upper);
                bool _7576;
                if (hit.y.v.lo <= 0.0)
                {
                    _7576 = hit.y.v.hi >= 0.0;
                }
                else
                {
                    _7576 = false;
                }
                float _7583;
                if (_7576)
                {
                    _7583 = 0.0;
                }
                else
                {
                    _7583 = precise::min(abs(hit.y.v.lo), abs(hit.y.v.hi));
                }
                float _7586 = precise::max(abs(hit.y.v.lo), abs(hit.y.v.hi));
                float _5654 = spvFMul(_7583, _7583);
                float _7587 = interval_down(_5654, intervalFailed);
                float _5655 = spvFMul(_7586, _7586);
                float _7589 = interval_up(_5655, intervalFailed);
                Interval _5656 = Interval{ 2.0, 2.0 };
                Interval _5657 = hit.y.v;
                Interval _7590 = imul(_5656, _5657, intervalFailed, optical_product_upper);
                Interval _5658 = _7590;
                Interval _5659 = hit.y.dx;
                Interval _7591 = jet_mul_derivative(_5658, _5659, intervalFailed, optical_product_upper);
                Interval _5660 = Interval{ 2.0, 2.0 };
                Interval _5661 = hit.y.v;
                Interval _7592 = imul(_5660, _5661, intervalFailed, optical_product_upper);
                Interval _5662 = _7592;
                Interval _5663 = hit.y.dy;
                Interval _7593 = jet_mul_derivative(_5662, _5663, intervalFailed, optical_product_upper);
                Interval _5648 = Interval{ precise::max(0.0, _7560), _7562 };
                Interval _5649 = Interval{ precise::max(0.0, _7587), _7589 };
                Interval _7596 = iadd(_5648, _5649, intervalFailed);
                Interval _5650 = _7564;
                Interval _5651 = _7591;
                Interval _7597 = jet_add_derivative(_5650, _5651, intervalFailed);
                Interval _5652 = _7566;
                Interval _5653 = _7593;
                Interval _7598 = jet_add_derivative(_5652, _5653, intervalFailed);
                bool _7608;
                if (hit.z.v.lo <= 0.0)
                {
                    _7608 = hit.z.v.hi >= 0.0;
                }
                else
                {
                    _7608 = false;
                }
                float _7615;
                if (_7608)
                {
                    _7615 = 0.0;
                }
                else
                {
                    _7615 = precise::min(abs(hit.z.v.lo), abs(hit.z.v.hi));
                }
                float _7618 = precise::max(abs(hit.z.v.lo), abs(hit.z.v.hi));
                float _5638 = spvFMul(_7615, _7615);
                float _7619 = interval_down(_5638, intervalFailed);
                float _5639 = spvFMul(_7618, _7618);
                float _7621 = interval_up(_5639, intervalFailed);
                Interval _5640 = Interval{ 2.0, 2.0 };
                Interval _5641 = hit.z.v;
                Interval _7622 = imul(_5640, _5641, intervalFailed, optical_product_upper);
                Interval _5642 = _7622;
                Interval _5643 = hit.z.dx;
                Interval _7623 = jet_mul_derivative(_5642, _5643, intervalFailed, optical_product_upper);
                Interval _5644 = Interval{ 2.0, 2.0 };
                Interval _5645 = hit.z.v;
                Interval _7624 = imul(_5644, _5645, intervalFailed, optical_product_upper);
                Interval _5646 = _7624;
                Interval _5647 = hit.z.dy;
                Interval _7625 = jet_mul_derivative(_5646, _5647, intervalFailed, optical_product_upper);
                Interval _5632 = _7596;
                Interval _5633 = Interval{ precise::max(0.0, _7619), _7621 };
                Interval _7627 = iadd(_5632, _5633, intervalFailed);
                Interval _5634 = _7597;
                Interval _5635 = _7623;
                Interval _7628 = jet_add_derivative(_5634, _5635, intervalFailed);
                Interval _5636 = _7598;
                Interval _5637 = _7625;
                Interval _7629 = jet_add_derivative(_5636, _5637, intervalFailed);
                Interval _5623 = _7627;
                Interval _7631 = isqrt(_5623, intervalFailed);
                bool _7639;
                if (!intervalFailed)
                {
                    _7639 = intervalFailed;
                }
                else
                {
                    _7639 = false;
                }
                bool _7644;
                if (_7639)
                {
                    _7644 = jetFailureSite == 0u;
                }
                else
                {
                    _7644 = false;
                }
                if (_7644)
                {
                    jetFailureSite = 3u;
                    jetFailureArguments = float4(_7627.lo, _7627.hi, 0.0, 0.0);
                }
                if (_7631.lo <= 0.0)
                {
                    jetBranchKnown = false;
                }
                Interval _5624 = Interval{ 2.0, 2.0 };
                Interval _5625 = _7631;
                Interval _7651 = imul(_5624, _5625, intervalFailed, optical_product_upper);
                Interval _5626 = Interval{ 1.0, 1.0 };
                Interval _5627 = _7651;
                Interval _7653 = idiv(_5626, _5627, intervalFailed, interval_divide_upper);
                bool _7660;
                if (!intervalFailed)
                {
                    _7660 = intervalFailed;
                }
                else
                {
                    _7660 = false;
                }
                bool _7665;
                if (_7660)
                {
                    _7665 = jetFailureSite == 0u;
                }
                else
                {
                    _7665 = false;
                }
                if (_7665)
                {
                    jetFailureSite = 4u;
                    jetFailureArguments = float4(1.0, 1.0, _7651.lo, _7651.hi);
                }
                Interval _5628 = _7653;
                Interval _5629 = _7628;
                Interval _7669 = jet_mul_derivative(_5628, _5629, intervalFailed, optical_product_upper);
                Interval _5630 = _7653;
                Interval _5631 = _7629;
                Interval _7670 = jet_mul_derivative(_5630, _5631, intervalFailed, optical_product_upper);
                _7677 = _7631.lo;
                _7678 = _7631.hi;
                _7679 = _7669.lo;
                _7680 = _7669.hi;
                _7681 = _7670.lo;
                _7682 = _7670.hi;
            }
            float _7686 = precise::max(liquid.projection.x, 1.0);
            Interval _5609 = Interval{ _7677, _7678 };
            Interval _5610 = Interval{ _7686, _7686 };
            Interval _7690 = idiv(_5609, _5610, intervalFailed, interval_divide_upper);
            bool _7695;
            if (!intervalFailed)
            {
                _7695 = intervalFailed;
            }
            else
            {
                _7695 = false;
            }
            bool _7700;
            if (_7695)
            {
                _7700 = jetFailureSite == 0u;
            }
            else
            {
                _7700 = false;
            }
            if (_7700)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7677, _7678, _7686, _7686);
            }
            Interval _5611 = Interval{ _7679, _7680 };
            Interval _5612 = _7690;
            Interval _5613 = Interval{ 0.0, 0.0 };
            Interval _7705 = jet_mul_derivative(_5612, _5613, intervalFailed, optical_product_upper);
            Interval _5614 = Interval{ as_type<float>(as_type<uint>(_7705.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7705.lo) ^ 2147483648u) };
            Interval _7715 = jet_add_derivative(_5611, _5614, intervalFailed);
            Interval _5615 = _7715;
            Interval _5616 = Interval{ _7686, _7686 };
            Interval _7717 = jet_div_derivative(_5615, _5616, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5617 = Interval{ _7681, _7682 };
            Interval _5618 = _7690;
            Interval _5619 = Interval{ 0.0, 0.0 };
            Interval _7719 = jet_mul_derivative(_5618, _5619, intervalFailed, optical_product_upper);
            Interval _5620 = Interval{ as_type<float>(as_type<uint>(_7719.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7719.lo) ^ 2147483648u) };
            Interval _7729 = jet_add_derivative(_5617, _5620, intervalFailed);
            Interval _5621 = _7729;
            Interval _5622 = Interval{ _7686, _7686 };
            Interval _7731 = jet_div_derivative(_5621, _5622, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            OpticalJet param_var_a = OpticalJet{ _6914, _6915, _6916 };
            OpticalJet param_var_a_1 = jabs(param_var_a);
            float _5607 = 4.0;
            float _5608 = 100.0;
            Interval _7735 = iratio(_5607, _5608, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _7740;
            if (!intervalFailed)
            {
                _7740 = intervalFailed;
            }
            else
            {
                _7740 = false;
            }
            bool _7745;
            if (_7740)
            {
                _7745 = jetFailureSite == 0u;
            }
            else
            {
                _7745 = false;
            }
            if (_7745)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(4.0, 4.0, 100.0, 100.0);
            }
            OpticalJet param_var_b = OpticalJet{ _7735, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
            OpticalJet _7749 = jmax(param_var_a_1, param_var_b);
            Interval _7750 = _7749.v;
            Interval _5593 = _7690;
            Interval _5594 = _7750;
            Interval _7754 = idiv(_5593, _5594, intervalFailed, interval_divide_upper);
            bool _7763;
            if (!intervalFailed)
            {
                _7763 = intervalFailed;
            }
            else
            {
                _7763 = false;
            }
            bool _7768;
            if (_7763)
            {
                _7768 = jetFailureSite == 0u;
            }
            else
            {
                _7768 = false;
            }
            if (_7768)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(_7690.lo, _7690.hi, _7750.lo, _7750.hi);
            }
            Interval _5595 = _7717;
            Interval _5596 = _7754;
            Interval _5597 = _7749.dx;
            Interval _7772 = jet_mul_derivative(_5596, _5597, intervalFailed, optical_product_upper);
            Interval _5598 = Interval{ as_type<float>(as_type<uint>(_7772.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7772.lo) ^ 2147483648u) };
            Interval _7782 = jet_add_derivative(_5595, _5598, intervalFailed);
            Interval _5599 = _7782;
            Interval _5600 = _7750;
            Interval _7783 = jet_div_derivative(_5599, _5600, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _5601 = _7731;
            Interval _5602 = _7754;
            Interval _5603 = _7749.dy;
            Interval _7784 = jet_mul_derivative(_5602, _5603, intervalFailed, optical_product_upper);
            Interval _5604 = Interval{ as_type<float>(as_type<uint>(_7784.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_7784.lo) ^ 2147483648u) };
            Interval _7794 = jet_add_derivative(_5601, _5604, intervalFailed);
            Interval _5605 = _7794;
            Interval _5606 = _7750;
            Interval _7795 = jet_div_derivative(_5605, _5606, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            ReflectionLiquidFrame param_var_f = liquid;
            OpticalJet3 param_var_direction_1 = OpticalJet3{ OpticalJet{ Interval{ _6690, _6692 }, Interval{ _6694, _6696 }, Interval{ _6698, _6700 } }, OpticalJet{ Interval{ _6702, _6704 }, Interval{ _6706, _6708 }, Interval{ _6710, _6712 } }, OpticalJet{ Interval{ _6714, _6716 }, Interval{ _6718, _6720 }, Interval{ _6722, _6724 } } };
            OpticalJet param_var_distance = OpticalJet{ Interval{ _7530, _7531 }, Interval{ _7532, _7533 }, Interval{ _7534, _7535 } };
            OpticalJet param_var_footprint = OpticalJet{ _7754, _7783, _7795 };
            OpticalJet3 _7815 = jet_liquid_normal(param_var_f, param_var_direction_1, param_var_distance, param_var_footprint, hit, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            _7850 = _7815.x.v.lo;
            _7851 = _7815.x.v.hi;
            _7852 = _7815.x.dx.lo;
            _7853 = _7815.x.dx.hi;
            _7854 = _7815.x.dy.lo;
            _7855 = _7815.x.dy.hi;
            _7856 = _7815.y.v.lo;
            _7857 = _7815.y.v.hi;
            _7858 = _7815.y.dx.lo;
            _7859 = _7815.y.dx.hi;
            _7860 = _7815.y.dy.lo;
            _7861 = _7815.y.dy.hi;
            _7862 = _7815.z.v.lo;
            _7863 = _7815.z.v.hi;
            _7864 = _7815.z.dx.lo;
            _7865 = _7815.z.dx.hi;
            _7866 = _7815.z.dy.lo;
            _7867 = _7815.z.dy.hi;
        }
        else
        {
            ReflectionSpecularPlane param_var_plane_1 = ReflectionSpecularPlane{ _6788, _6789, _6790 };
            OpticalJet3 param_var_hit = hit;
            bool _7848 = jet_inside_face(param_var_plane_1, param_var_hit, intervalFailed, optical_product_upper, interval_divide_upper);
            if (!_7848)
            {
                return false;
            }
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
            _7862 = _6854;
            _7863 = _6855;
            _7864 = _6856;
            _7865 = _6857;
            _7866 = _6858;
            _7867 = _6859;
        }
        bool _7869;
        if (_6766)
        {
            _7869 = !_6776;
        }
        else
        {
            _7869 = false;
        }
        bool _7883;
        if (_7869)
        {
            bool _7876;
            if (_6788.w == 2.0)
            {
                _7876 = _6789.w == 2.0;
            }
            else
            {
                _7876 = false;
            }
            bool _7881;
            if (_7876)
            {
                _7881 = _6790.w == 2.0;
            }
            else
            {
                _7881 = false;
            }
            _7883 = !_7881;
        }
        else
        {
            _7883 = false;
        }
        int _7884;
        if (_7883)
        {
            _7884 = 1;
        }
        else
        {
            _7884 = 5;
        }
        float _7885 = float(_7884);
        float _5591 = _7885;
        float _5592 = 100.0;
        Interval _7887 = iratio(_5591, _5592, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _7892;
        if (!intervalFailed)
        {
            _7892 = intervalFailed;
        }
        else
        {
            _7892 = false;
        }
        bool _7897;
        if (_7892)
        {
            _7897 = jetFailureSite == 0u;
        }
        else
        {
            _7897 = false;
        }
        if (_7897)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(_7885, _7885, 100.0, 100.0);
        }
        OpticalJet param_var_a_2 = OpticalJet{ _7887, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
        float _5589 = 1.0;
        float _5590 = 100000.0;
        Interval _7903 = iratio(_5589, _5590, intervalFailed, optical_product_upper, interval_divide_upper);
        bool _7908;
        if (!intervalFailed)
        {
            _7908 = intervalFailed;
        }
        else
        {
            _7908 = false;
        }
        bool _7913;
        if (_7908)
        {
            _7913 = jetFailureSite == 0u;
        }
        else
        {
            _7913 = false;
        }
        if (_7913)
        {
            jetFailureSite = 6u;
            jetFailureArguments = float4(1.0, 1.0, 100000.0, 100000.0);
        }
        Interval _5575 = Interval{ _7530, _7531 };
        Interval _5576 = _7903;
        Interval _7917 = imul(_5575, _5576, intervalFailed, optical_product_upper);
        Interval _5577 = Interval{ _7532, _7533 };
        Interval _5578 = _7903;
        Interval _7919 = jet_mul_derivative(_5577, _5578, intervalFailed, optical_product_upper);
        Interval _5579 = _7919;
        Interval _5580 = Interval{ _7530, _7531 };
        Interval _5581 = Interval{ 0.0, 0.0 };
        Interval _7921 = jet_mul_derivative(_5580, _5581, intervalFailed, optical_product_upper);
        Interval _5582 = _7921;
        Interval _7922 = jet_add_derivative(_5579, _5582, intervalFailed);
        Interval _5583 = Interval{ _7534, _7535 };
        Interval _5584 = _7903;
        Interval _7924 = jet_mul_derivative(_5583, _5584, intervalFailed, optical_product_upper);
        Interval _5585 = _7924;
        Interval _5586 = Interval{ _7530, _7531 };
        Interval _5587 = Interval{ 0.0, 0.0 };
        Interval _7926 = jet_mul_derivative(_5586, _5587, intervalFailed, optical_product_upper);
        Interval _5588 = _7926;
        Interval _7927 = jet_add_derivative(_5585, _5588, intervalFailed);
        OpticalJet param_var_b_1 = OpticalJet{ _7917, _7922, _7927 };
        OpticalJet _7929 = jmax(param_var_a_2, param_var_b_1);
        Interval _7930 = _7929.v;
        _6679 = _7930.lo;
        _6681 = _7930.hi;
        Interval _7931 = _7929.dx;
        _6683 = _7931.lo;
        _6685 = _7931.hi;
        Interval _7932 = _7929.dy;
        _6687 = _7932.lo;
        _6689 = _7932.hi;
        Interval _7937 = _7929.v;
        Interval _5561 = Interval{ _7850, _7851 };
        Interval _5562 = _7937;
        Interval _7941 = imul(_5561, _5562, intervalFailed, optical_product_upper);
        Interval _5563 = Interval{ _7852, _7853 };
        Interval _5564 = _7937;
        Interval _7943 = jet_mul_derivative(_5563, _5564, intervalFailed, optical_product_upper);
        Interval _5565 = _7943;
        Interval _5566 = Interval{ _7850, _7851 };
        Interval _5567 = _7929.dx;
        Interval _7945 = jet_mul_derivative(_5566, _5567, intervalFailed, optical_product_upper);
        Interval _5568 = _7945;
        Interval _7946 = jet_add_derivative(_5565, _5568, intervalFailed);
        Interval _5569 = Interval{ _7854, _7855 };
        Interval _5570 = _7937;
        Interval _7948 = jet_mul_derivative(_5569, _5570, intervalFailed, optical_product_upper);
        Interval _5571 = _7948;
        Interval _5572 = Interval{ _7850, _7851 };
        Interval _5573 = _7929.dy;
        Interval _7950 = jet_mul_derivative(_5572, _5573, intervalFailed, optical_product_upper);
        Interval _5574 = _7950;
        Interval _7951 = jet_add_derivative(_5571, _5574, intervalFailed);
        Interval _7952 = _7929.v;
        Interval _5547 = Interval{ _7856, _7857 };
        Interval _5548 = _7952;
        Interval _7956 = imul(_5547, _5548, intervalFailed, optical_product_upper);
        Interval _5549 = Interval{ _7858, _7859 };
        Interval _5550 = _7952;
        Interval _7958 = jet_mul_derivative(_5549, _5550, intervalFailed, optical_product_upper);
        Interval _5551 = _7958;
        Interval _5552 = Interval{ _7856, _7857 };
        Interval _5553 = _7929.dx;
        Interval _7960 = jet_mul_derivative(_5552, _5553, intervalFailed, optical_product_upper);
        Interval _5554 = _7960;
        Interval _7961 = jet_add_derivative(_5551, _5554, intervalFailed);
        Interval _5555 = Interval{ _7860, _7861 };
        Interval _5556 = _7952;
        Interval _7963 = jet_mul_derivative(_5555, _5556, intervalFailed, optical_product_upper);
        Interval _5557 = _7963;
        Interval _5558 = Interval{ _7856, _7857 };
        Interval _5559 = _7929.dy;
        Interval _7965 = jet_mul_derivative(_5558, _5559, intervalFailed, optical_product_upper);
        Interval _5560 = _7965;
        Interval _7966 = jet_add_derivative(_5557, _5560, intervalFailed);
        Interval _7967 = _7929.v;
        Interval _5533 = Interval{ _7862, _7863 };
        Interval _5534 = _7967;
        Interval _7971 = imul(_5533, _5534, intervalFailed, optical_product_upper);
        Interval _5535 = Interval{ _7864, _7865 };
        Interval _5536 = _7967;
        Interval _7973 = jet_mul_derivative(_5535, _5536, intervalFailed, optical_product_upper);
        Interval _5537 = _7973;
        Interval _5538 = Interval{ _7862, _7863 };
        Interval _5539 = _7929.dx;
        Interval _7975 = jet_mul_derivative(_5538, _5539, intervalFailed, optical_product_upper);
        Interval _5540 = _7975;
        Interval _7976 = jet_add_derivative(_5537, _5540, intervalFailed);
        Interval _5541 = Interval{ _7866, _7867 };
        Interval _5542 = _7967;
        Interval _7978 = jet_mul_derivative(_5541, _5542, intervalFailed, optical_product_upper);
        Interval _5543 = _7978;
        Interval _5544 = Interval{ _7862, _7863 };
        Interval _5545 = _7929.dy;
        Interval _7980 = jet_mul_derivative(_5544, _5545, intervalFailed, optical_product_upper);
        Interval _5546 = _7980;
        Interval _7981 = jet_add_derivative(_5543, _5546, intervalFailed);
        Interval _5527 = hit.x.v;
        Interval _5528 = _7941;
        Interval _7985 = iadd(_5527, _5528, intervalFailed);
        Interval _5529 = hit.x.dx;
        Interval _5530 = _7946;
        Interval _7986 = jet_add_derivative(_5529, _5530, intervalFailed);
        Interval _5531 = hit.x.dy;
        Interval _5532 = _7951;
        Interval _7987 = jet_add_derivative(_5531, _5532, intervalFailed);
        Interval _5521 = hit.y.v;
        Interval _5522 = _7956;
        Interval _7991 = iadd(_5521, _5522, intervalFailed);
        Interval _5523 = hit.y.dx;
        Interval _5524 = _7961;
        Interval _7992 = jet_add_derivative(_5523, _5524, intervalFailed);
        Interval _5525 = hit.y.dy;
        Interval _5526 = _7966;
        Interval _7993 = jet_add_derivative(_5525, _5526, intervalFailed);
        Interval _5515 = hit.z.v;
        Interval _5516 = _7971;
        Interval _7997 = iadd(_5515, _5516, intervalFailed);
        Interval _5517 = hit.z.dx;
        Interval _5518 = _7976;
        Interval _7998 = jet_add_derivative(_5517, _5518, intervalFailed);
        Interval _5519 = hit.z.dy;
        Interval _5520 = _7981;
        Interval _7999 = jet_add_derivative(_5519, _5520, intervalFailed);
        _6727 = _7985.lo;
        _6729 = _7985.hi;
        _6731 = _7986.lo;
        _6733 = _7986.hi;
        _6735 = _7987.lo;
        _6737 = _7987.hi;
        _6739 = _7991.lo;
        _6741 = _7991.hi;
        _6743 = _7992.lo;
        _6745 = _7992.hi;
        _6747 = _7993.lo;
        _6749 = _7993.hi;
        _6751 = _7997.lo;
        _6753 = _7997.hi;
        _6755 = _7998.lo;
        _6757 = _7998.hi;
        _6759 = _7999.lo;
        _6761 = _7999.hi;
        Interval _5501 = Interval{ _6690, _6692 };
        Interval _5502 = Interval{ _7850, _7851 };
        Interval _8002 = imul(_5501, _5502, intervalFailed, optical_product_upper);
        Interval _5503 = Interval{ _6694, _6696 };
        Interval _5504 = Interval{ _7850, _7851 };
        Interval _8005 = jet_mul_derivative(_5503, _5504, intervalFailed, optical_product_upper);
        Interval _5505 = _8005;
        Interval _5506 = Interval{ _6690, _6692 };
        Interval _5507 = Interval{ _7852, _7853 };
        Interval _8008 = jet_mul_derivative(_5506, _5507, intervalFailed, optical_product_upper);
        Interval _5508 = _8008;
        Interval _8009 = jet_add_derivative(_5505, _5508, intervalFailed);
        Interval _5509 = Interval{ _6698, _6700 };
        Interval _5510 = Interval{ _7850, _7851 };
        Interval _8012 = jet_mul_derivative(_5509, _5510, intervalFailed, optical_product_upper);
        Interval _5511 = _8012;
        Interval _5512 = Interval{ _6690, _6692 };
        Interval _5513 = Interval{ _7854, _7855 };
        Interval _8015 = jet_mul_derivative(_5512, _5513, intervalFailed, optical_product_upper);
        Interval _5514 = _8015;
        Interval _8016 = jet_add_derivative(_5511, _5514, intervalFailed);
        Interval _5487 = Interval{ _6702, _6704 };
        Interval _5488 = Interval{ _7856, _7857 };
        Interval _8019 = imul(_5487, _5488, intervalFailed, optical_product_upper);
        Interval _5489 = Interval{ _6706, _6708 };
        Interval _5490 = Interval{ _7856, _7857 };
        Interval _8022 = jet_mul_derivative(_5489, _5490, intervalFailed, optical_product_upper);
        Interval _5491 = _8022;
        Interval _5492 = Interval{ _6702, _6704 };
        Interval _5493 = Interval{ _7858, _7859 };
        Interval _8025 = jet_mul_derivative(_5492, _5493, intervalFailed, optical_product_upper);
        Interval _5494 = _8025;
        Interval _8026 = jet_add_derivative(_5491, _5494, intervalFailed);
        Interval _5495 = Interval{ _6710, _6712 };
        Interval _5496 = Interval{ _7856, _7857 };
        Interval _8029 = jet_mul_derivative(_5495, _5496, intervalFailed, optical_product_upper);
        Interval _5497 = _8029;
        Interval _5498 = Interval{ _6702, _6704 };
        Interval _5499 = Interval{ _7860, _7861 };
        Interval _8032 = jet_mul_derivative(_5498, _5499, intervalFailed, optical_product_upper);
        Interval _5500 = _8032;
        Interval _8033 = jet_add_derivative(_5497, _5500, intervalFailed);
        Interval _5481 = _8002;
        Interval _5482 = _8019;
        Interval _8034 = iadd(_5481, _5482, intervalFailed);
        Interval _5483 = _8009;
        Interval _5484 = _8026;
        Interval _8035 = jet_add_derivative(_5483, _5484, intervalFailed);
        Interval _5485 = _8016;
        Interval _5486 = _8033;
        Interval _8036 = jet_add_derivative(_5485, _5486, intervalFailed);
        Interval _5467 = Interval{ _6714, _6716 };
        Interval _5468 = Interval{ _7862, _7863 };
        Interval _8039 = imul(_5467, _5468, intervalFailed, optical_product_upper);
        Interval _5469 = Interval{ _6718, _6720 };
        Interval _5470 = Interval{ _7862, _7863 };
        Interval _8042 = jet_mul_derivative(_5469, _5470, intervalFailed, optical_product_upper);
        Interval _5471 = _8042;
        Interval _5472 = Interval{ _6714, _6716 };
        Interval _5473 = Interval{ _7864, _7865 };
        Interval _8045 = jet_mul_derivative(_5472, _5473, intervalFailed, optical_product_upper);
        Interval _5474 = _8045;
        Interval _8046 = jet_add_derivative(_5471, _5474, intervalFailed);
        Interval _5475 = Interval{ _6722, _6724 };
        Interval _5476 = Interval{ _7862, _7863 };
        Interval _8049 = jet_mul_derivative(_5475, _5476, intervalFailed, optical_product_upper);
        Interval _5477 = _8049;
        Interval _5478 = Interval{ _6714, _6716 };
        Interval _5479 = Interval{ _7866, _7867 };
        Interval _8052 = jet_mul_derivative(_5478, _5479, intervalFailed, optical_product_upper);
        Interval _5480 = _8052;
        Interval _8053 = jet_add_derivative(_5477, _5480, intervalFailed);
        Interval _5461 = _8034;
        Interval _5462 = _8039;
        Interval _8054 = iadd(_5461, _5462, intervalFailed);
        Interval _5463 = _8035;
        Interval _5464 = _8046;
        Interval _8055 = jet_add_derivative(_5463, _5464, intervalFailed);
        Interval _5465 = _8036;
        Interval _5466 = _8053;
        Interval _8056 = jet_add_derivative(_5465, _5466, intervalFailed);
        Interval _5447 = Interval{ 2.0, 2.0 };
        Interval _5448 = _8054;
        Interval _8057 = imul(_5447, _5448, intervalFailed, optical_product_upper);
        Interval _5449 = Interval{ 0.0, 0.0 };
        Interval _5450 = _8054;
        Interval _8058 = jet_mul_derivative(_5449, _5450, intervalFailed, optical_product_upper);
        Interval _5451 = _8058;
        Interval _5452 = Interval{ 2.0, 2.0 };
        Interval _5453 = _8055;
        Interval _8059 = jet_mul_derivative(_5452, _5453, intervalFailed, optical_product_upper);
        Interval _5454 = _8059;
        Interval _8060 = jet_add_derivative(_5451, _5454, intervalFailed);
        Interval _5455 = Interval{ 0.0, 0.0 };
        Interval _5456 = _8054;
        Interval _8061 = jet_mul_derivative(_5455, _5456, intervalFailed, optical_product_upper);
        Interval _5457 = _8061;
        Interval _5458 = Interval{ 2.0, 2.0 };
        Interval _5459 = _8056;
        Interval _8062 = jet_mul_derivative(_5458, _5459, intervalFailed, optical_product_upper);
        Interval _5460 = _8062;
        Interval _8063 = jet_add_derivative(_5457, _5460, intervalFailed);
        Interval _5433 = Interval{ _7850, _7851 };
        Interval _5434 = _8057;
        Interval _8065 = imul(_5433, _5434, intervalFailed, optical_product_upper);
        Interval _5435 = Interval{ _7852, _7853 };
        Interval _5436 = _8057;
        Interval _8067 = jet_mul_derivative(_5435, _5436, intervalFailed, optical_product_upper);
        Interval _5437 = _8067;
        Interval _5438 = Interval{ _7850, _7851 };
        Interval _5439 = _8060;
        Interval _8069 = jet_mul_derivative(_5438, _5439, intervalFailed, optical_product_upper);
        Interval _5440 = _8069;
        Interval _8070 = jet_add_derivative(_5437, _5440, intervalFailed);
        Interval _5441 = Interval{ _7854, _7855 };
        Interval _5442 = _8057;
        Interval _8072 = jet_mul_derivative(_5441, _5442, intervalFailed, optical_product_upper);
        Interval _5443 = _8072;
        Interval _5444 = Interval{ _7850, _7851 };
        Interval _5445 = _8063;
        Interval _8074 = jet_mul_derivative(_5444, _5445, intervalFailed, optical_product_upper);
        Interval _5446 = _8074;
        Interval _8075 = jet_add_derivative(_5443, _5446, intervalFailed);
        Interval _5419 = Interval{ _7856, _7857 };
        Interval _5420 = _8057;
        Interval _8077 = imul(_5419, _5420, intervalFailed, optical_product_upper);
        Interval _5421 = Interval{ _7858, _7859 };
        Interval _5422 = _8057;
        Interval _8079 = jet_mul_derivative(_5421, _5422, intervalFailed, optical_product_upper);
        Interval _5423 = _8079;
        Interval _5424 = Interval{ _7856, _7857 };
        Interval _5425 = _8060;
        Interval _8081 = jet_mul_derivative(_5424, _5425, intervalFailed, optical_product_upper);
        Interval _5426 = _8081;
        Interval _8082 = jet_add_derivative(_5423, _5426, intervalFailed);
        Interval _5427 = Interval{ _7860, _7861 };
        Interval _5428 = _8057;
        Interval _8084 = jet_mul_derivative(_5427, _5428, intervalFailed, optical_product_upper);
        Interval _5429 = _8084;
        Interval _5430 = Interval{ _7856, _7857 };
        Interval _5431 = _8063;
        Interval _8086 = jet_mul_derivative(_5430, _5431, intervalFailed, optical_product_upper);
        Interval _5432 = _8086;
        Interval _8087 = jet_add_derivative(_5429, _5432, intervalFailed);
        Interval _5405 = Interval{ _7862, _7863 };
        Interval _5406 = _8057;
        Interval _8089 = imul(_5405, _5406, intervalFailed, optical_product_upper);
        Interval _5407 = Interval{ _7864, _7865 };
        Interval _5408 = _8057;
        Interval _8091 = jet_mul_derivative(_5407, _5408, intervalFailed, optical_product_upper);
        Interval _5409 = _8091;
        Interval _5410 = Interval{ _7862, _7863 };
        Interval _5411 = _8060;
        Interval _8093 = jet_mul_derivative(_5410, _5411, intervalFailed, optical_product_upper);
        Interval _5412 = _8093;
        Interval _8094 = jet_add_derivative(_5409, _5412, intervalFailed);
        Interval _5413 = Interval{ _7866, _7867 };
        Interval _5414 = _8057;
        Interval _8096 = jet_mul_derivative(_5413, _5414, intervalFailed, optical_product_upper);
        Interval _5415 = _8096;
        Interval _5416 = Interval{ _7862, _7863 };
        Interval _5417 = _8063;
        Interval _8098 = jet_mul_derivative(_5416, _5417, intervalFailed, optical_product_upper);
        Interval _5418 = _8098;
        Interval _8099 = jet_add_derivative(_5415, _5418, intervalFailed);
        Interval _5399 = Interval{ _6690, _6692 };
        Interval _5400 = Interval{ as_type<float>(as_type<uint>(_8065.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8065.lo) ^ 2147483648u) };
        Interval _8174 = iadd(_5399, _5400, intervalFailed);
        Interval _5401 = Interval{ _6694, _6696 };
        Interval _5402 = Interval{ as_type<float>(as_type<uint>(_8070.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8070.lo) ^ 2147483648u) };
        Interval _8177 = jet_add_derivative(_5401, _5402, intervalFailed);
        Interval _5403 = Interval{ _6698, _6700 };
        Interval _5404 = Interval{ as_type<float>(as_type<uint>(_8075.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8075.lo) ^ 2147483648u) };
        Interval _8180 = jet_add_derivative(_5403, _5404, intervalFailed);
        Interval _5393 = Interval{ _6702, _6704 };
        Interval _5394 = Interval{ as_type<float>(as_type<uint>(_8077.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8077.lo) ^ 2147483648u) };
        Interval _8183 = iadd(_5393, _5394, intervalFailed);
        Interval _5395 = Interval{ _6706, _6708 };
        Interval _5396 = Interval{ as_type<float>(as_type<uint>(_8082.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8082.lo) ^ 2147483648u) };
        Interval _8186 = jet_add_derivative(_5395, _5396, intervalFailed);
        Interval _5397 = Interval{ _6710, _6712 };
        Interval _5398 = Interval{ as_type<float>(as_type<uint>(_8087.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8087.lo) ^ 2147483648u) };
        Interval _8189 = jet_add_derivative(_5397, _5398, intervalFailed);
        Interval _5387 = Interval{ _6714, _6716 };
        Interval _5388 = Interval{ as_type<float>(as_type<uint>(_8089.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8089.lo) ^ 2147483648u) };
        Interval _8192 = iadd(_5387, _5388, intervalFailed);
        Interval _5389 = Interval{ _6718, _6720 };
        Interval _5390 = Interval{ as_type<float>(as_type<uint>(_8094.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8094.lo) ^ 2147483648u) };
        Interval _8195 = jet_add_derivative(_5389, _5390, intervalFailed);
        Interval _5391 = Interval{ _6722, _6724 };
        Interval _5392 = Interval{ as_type<float>(as_type<uint>(_8099.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8099.lo) ^ 2147483648u) };
        Interval _8198 = jet_add_derivative(_5391, _5392, intervalFailed);
        bool _8218;
        if (_6766)
        {
            _8218 = !_6776;
        }
        else
        {
            _8218 = false;
        }
        bool _8223;
        if (_8218)
        {
            _8223 = receiver.settings.x != 0.0;
        }
        else
        {
            _8223 = false;
        }
        if (_8223)
        {
            bool _8230;
            if (_8183.lo <= 0.0)
            {
                _8230 = _8183.hi >= 0.0;
            }
            else
            {
                _8230 = false;
            }
            float _8237;
            if (_8230)
            {
                _8237 = 0.0;
            }
            else
            {
                _8237 = precise::min(abs(_8183.lo), abs(_8183.hi));
            }
            float _5385 = 0.949999988079071044921875;
            float _8241 = interval_down(_5385, intervalFailed);
            float _5386 = 0.949999988079071044921875;
            __attribute__((unused)) float _8242 = interval_up(_5386, intervalFailed);
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
            float _8927;
            float _8928;
            float _8929;
            float _8930;
            float _8931;
            float _8932;
            if (precise::max(abs(_8183.lo), abs(_8183.hi)) < _8241)
            {
                Interval _5371 = _8183;
                Interval _5372 = Interval{ 0.0, 0.0 };
                Interval _8244 = imul(_5371, _5372, intervalFailed, optical_product_upper);
                Interval _5373 = _8186;
                Interval _5374 = Interval{ 0.0, 0.0 };
                Interval _8245 = jet_mul_derivative(_5373, _5374, intervalFailed, optical_product_upper);
                Interval _5375 = _8245;
                Interval _5376 = _8183;
                Interval _5377 = Interval{ 0.0, 0.0 };
                Interval _8246 = jet_mul_derivative(_5376, _5377, intervalFailed, optical_product_upper);
                Interval _5378 = _8246;
                Interval _8247 = jet_add_derivative(_5375, _5378, intervalFailed);
                Interval _5379 = _8189;
                Interval _5380 = Interval{ 0.0, 0.0 };
                Interval _8248 = jet_mul_derivative(_5379, _5380, intervalFailed, optical_product_upper);
                Interval _5381 = _8248;
                Interval _5382 = _8183;
                Interval _5383 = Interval{ 0.0, 0.0 };
                Interval _8249 = jet_mul_derivative(_5382, _5383, intervalFailed, optical_product_upper);
                Interval _5384 = _8249;
                Interval _8250 = jet_add_derivative(_5381, _5384, intervalFailed);
                Interval _5357 = _8192;
                Interval _5358 = Interval{ 1.0, 1.0 };
                Interval _8251 = imul(_5357, _5358, intervalFailed, optical_product_upper);
                Interval _5359 = _8195;
                Interval _5360 = Interval{ 1.0, 1.0 };
                Interval _8252 = jet_mul_derivative(_5359, _5360, intervalFailed, optical_product_upper);
                Interval _5361 = _8252;
                Interval _5362 = _8192;
                Interval _5363 = Interval{ 0.0, 0.0 };
                Interval _8253 = jet_mul_derivative(_5362, _5363, intervalFailed, optical_product_upper);
                Interval _5364 = _8253;
                Interval _8254 = jet_add_derivative(_5361, _5364, intervalFailed);
                Interval _5365 = _8198;
                Interval _5366 = Interval{ 1.0, 1.0 };
                Interval _8255 = jet_mul_derivative(_5365, _5366, intervalFailed, optical_product_upper);
                Interval _5367 = _8255;
                Interval _5368 = _8192;
                Interval _5369 = Interval{ 0.0, 0.0 };
                Interval _8256 = jet_mul_derivative(_5368, _5369, intervalFailed, optical_product_upper);
                Interval _5370 = _8256;
                Interval _8257 = jet_add_derivative(_5367, _5370, intervalFailed);
                Interval _5351 = _8244;
                Interval _5352 = Interval{ as_type<float>(as_type<uint>(_8251.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8251.lo) ^ 2147483648u) };
                Interval _8283 = iadd(_5351, _5352, intervalFailed);
                Interval _5353 = _8247;
                Interval _5354 = Interval{ as_type<float>(as_type<uint>(_8254.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8254.lo) ^ 2147483648u) };
                Interval _8285 = jet_add_derivative(_5353, _5354, intervalFailed);
                Interval _5355 = _8250;
                Interval _5356 = Interval{ as_type<float>(as_type<uint>(_8257.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8257.lo) ^ 2147483648u) };
                Interval _8287 = jet_add_derivative(_5355, _5356, intervalFailed);
                Interval _5337 = _8192;
                Interval _5338 = Interval{ 0.0, 0.0 };
                Interval _8288 = imul(_5337, _5338, intervalFailed, optical_product_upper);
                Interval _5339 = _8195;
                Interval _5340 = Interval{ 0.0, 0.0 };
                Interval _8289 = jet_mul_derivative(_5339, _5340, intervalFailed, optical_product_upper);
                Interval _5341 = _8289;
                Interval _5342 = _8192;
                Interval _5343 = Interval{ 0.0, 0.0 };
                Interval _8290 = jet_mul_derivative(_5342, _5343, intervalFailed, optical_product_upper);
                Interval _5344 = _8290;
                Interval _8291 = jet_add_derivative(_5341, _5344, intervalFailed);
                Interval _5345 = _8198;
                Interval _5346 = Interval{ 0.0, 0.0 };
                Interval _8292 = jet_mul_derivative(_5345, _5346, intervalFailed, optical_product_upper);
                Interval _5347 = _8292;
                Interval _5348 = _8192;
                Interval _5349 = Interval{ 0.0, 0.0 };
                Interval _8293 = jet_mul_derivative(_5348, _5349, intervalFailed, optical_product_upper);
                Interval _5350 = _8293;
                Interval _8294 = jet_add_derivative(_5347, _5350, intervalFailed);
                Interval _5323 = _8174;
                Interval _5324 = Interval{ 0.0, 0.0 };
                Interval _8295 = imul(_5323, _5324, intervalFailed, optical_product_upper);
                Interval _5325 = _8177;
                Interval _5326 = Interval{ 0.0, 0.0 };
                Interval _8296 = jet_mul_derivative(_5325, _5326, intervalFailed, optical_product_upper);
                Interval _5327 = _8296;
                Interval _5328 = _8174;
                Interval _5329 = Interval{ 0.0, 0.0 };
                Interval _8297 = jet_mul_derivative(_5328, _5329, intervalFailed, optical_product_upper);
                Interval _5330 = _8297;
                Interval _8298 = jet_add_derivative(_5327, _5330, intervalFailed);
                Interval _5331 = _8180;
                Interval _5332 = Interval{ 0.0, 0.0 };
                Interval _8299 = jet_mul_derivative(_5331, _5332, intervalFailed, optical_product_upper);
                Interval _5333 = _8299;
                Interval _5334 = _8174;
                Interval _5335 = Interval{ 0.0, 0.0 };
                Interval _8300 = jet_mul_derivative(_5334, _5335, intervalFailed, optical_product_upper);
                Interval _5336 = _8300;
                Interval _8301 = jet_add_derivative(_5333, _5336, intervalFailed);
                Interval _5317 = _8288;
                Interval _5318 = Interval{ as_type<float>(as_type<uint>(_8295.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8295.lo) ^ 2147483648u) };
                Interval _8327 = iadd(_5317, _5318, intervalFailed);
                Interval _5319 = _8291;
                Interval _5320 = Interval{ as_type<float>(as_type<uint>(_8298.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8298.lo) ^ 2147483648u) };
                Interval _8329 = jet_add_derivative(_5319, _5320, intervalFailed);
                Interval _5321 = _8294;
                Interval _5322 = Interval{ as_type<float>(as_type<uint>(_8301.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8301.lo) ^ 2147483648u) };
                Interval _8331 = jet_add_derivative(_5321, _5322, intervalFailed);
                Interval _5303 = _8174;
                Interval _5304 = Interval{ 1.0, 1.0 };
                Interval _8332 = imul(_5303, _5304, intervalFailed, optical_product_upper);
                Interval _5305 = _8177;
                Interval _5306 = Interval{ 1.0, 1.0 };
                Interval _8333 = jet_mul_derivative(_5305, _5306, intervalFailed, optical_product_upper);
                Interval _5307 = _8333;
                Interval _5308 = _8174;
                Interval _5309 = Interval{ 0.0, 0.0 };
                Interval _8334 = jet_mul_derivative(_5308, _5309, intervalFailed, optical_product_upper);
                Interval _5310 = _8334;
                Interval _8335 = jet_add_derivative(_5307, _5310, intervalFailed);
                Interval _5311 = _8180;
                Interval _5312 = Interval{ 1.0, 1.0 };
                Interval _8336 = jet_mul_derivative(_5311, _5312, intervalFailed, optical_product_upper);
                Interval _5313 = _8336;
                Interval _5314 = _8174;
                Interval _5315 = Interval{ 0.0, 0.0 };
                Interval _8337 = jet_mul_derivative(_5314, _5315, intervalFailed, optical_product_upper);
                Interval _5316 = _8337;
                Interval _8338 = jet_add_derivative(_5313, _5316, intervalFailed);
                Interval _5289 = _8183;
                Interval _5290 = Interval{ 0.0, 0.0 };
                Interval _8339 = imul(_5289, _5290, intervalFailed, optical_product_upper);
                Interval _5291 = _8186;
                Interval _5292 = Interval{ 0.0, 0.0 };
                Interval _8340 = jet_mul_derivative(_5291, _5292, intervalFailed, optical_product_upper);
                Interval _5293 = _8340;
                Interval _5294 = _8183;
                Interval _5295 = Interval{ 0.0, 0.0 };
                Interval _8341 = jet_mul_derivative(_5294, _5295, intervalFailed, optical_product_upper);
                Interval _5296 = _8341;
                Interval _8342 = jet_add_derivative(_5293, _5296, intervalFailed);
                Interval _5297 = _8189;
                Interval _5298 = Interval{ 0.0, 0.0 };
                Interval _8343 = jet_mul_derivative(_5297, _5298, intervalFailed, optical_product_upper);
                Interval _5299 = _8343;
                Interval _5300 = _8183;
                Interval _5301 = Interval{ 0.0, 0.0 };
                Interval _8344 = jet_mul_derivative(_5300, _5301, intervalFailed, optical_product_upper);
                Interval _5302 = _8344;
                Interval _8345 = jet_add_derivative(_5299, _5302, intervalFailed);
                Interval _5283 = _8332;
                Interval _5284 = Interval{ as_type<float>(as_type<uint>(_8339.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8339.lo) ^ 2147483648u) };
                Interval _8371 = iadd(_5283, _5284, intervalFailed);
                Interval _5285 = _8335;
                Interval _5286 = Interval{ as_type<float>(as_type<uint>(_8342.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8342.lo) ^ 2147483648u) };
                Interval _8373 = jet_add_derivative(_5285, _5286, intervalFailed);
                Interval _5287 = _8338;
                Interval _5288 = Interval{ as_type<float>(as_type<uint>(_8345.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8345.lo) ^ 2147483648u) };
                Interval _8375 = jet_add_derivative(_5287, _5288, intervalFailed);
                bool _8382;
                if (_8283.lo <= 0.0)
                {
                    _8382 = _8283.hi >= 0.0;
                }
                else
                {
                    _8382 = false;
                }
                float _8389;
                if (_8382)
                {
                    _8389 = 0.0;
                }
                else
                {
                    _8389 = precise::min(abs(_8283.lo), abs(_8283.hi));
                }
                float _8392 = precise::max(abs(_8283.lo), abs(_8283.hi));
                float _5273 = spvFMul(_8389, _8389);
                float _8393 = interval_down(_5273, intervalFailed);
                float _5274 = spvFMul(_8392, _8392);
                float _8395 = interval_up(_5274, intervalFailed);
                Interval _5275 = Interval{ 2.0, 2.0 };
                Interval _5276 = _8283;
                Interval _8396 = imul(_5275, _5276, intervalFailed, optical_product_upper);
                Interval _5277 = _8396;
                Interval _5278 = _8285;
                Interval _8397 = jet_mul_derivative(_5277, _5278, intervalFailed, optical_product_upper);
                Interval _5279 = Interval{ 2.0, 2.0 };
                Interval _5280 = _8283;
                Interval _8398 = imul(_5279, _5280, intervalFailed, optical_product_upper);
                Interval _5281 = _8398;
                Interval _5282 = _8287;
                Interval _8399 = jet_mul_derivative(_5281, _5282, intervalFailed, optical_product_upper);
                bool _8406;
                if (_8327.lo <= 0.0)
                {
                    _8406 = _8327.hi >= 0.0;
                }
                else
                {
                    _8406 = false;
                }
                float _8413;
                if (_8406)
                {
                    _8413 = 0.0;
                }
                else
                {
                    _8413 = precise::min(abs(_8327.lo), abs(_8327.hi));
                }
                float _8416 = precise::max(abs(_8327.lo), abs(_8327.hi));
                float _5263 = spvFMul(_8413, _8413);
                float _8417 = interval_down(_5263, intervalFailed);
                float _5264 = spvFMul(_8416, _8416);
                float _8419 = interval_up(_5264, intervalFailed);
                Interval _5265 = Interval{ 2.0, 2.0 };
                Interval _5266 = _8327;
                Interval _8420 = imul(_5265, _5266, intervalFailed, optical_product_upper);
                Interval _5267 = _8420;
                Interval _5268 = _8329;
                Interval _8421 = jet_mul_derivative(_5267, _5268, intervalFailed, optical_product_upper);
                Interval _5269 = Interval{ 2.0, 2.0 };
                Interval _5270 = _8327;
                Interval _8422 = imul(_5269, _5270, intervalFailed, optical_product_upper);
                Interval _5271 = _8422;
                Interval _5272 = _8331;
                Interval _8423 = jet_mul_derivative(_5271, _5272, intervalFailed, optical_product_upper);
                Interval _5257 = Interval{ precise::max(0.0, _8393), _8395 };
                Interval _5258 = Interval{ precise::max(0.0, _8417), _8419 };
                Interval _8426 = iadd(_5257, _5258, intervalFailed);
                Interval _5259 = _8397;
                Interval _5260 = _8421;
                Interval _8427 = jet_add_derivative(_5259, _5260, intervalFailed);
                Interval _5261 = _8399;
                Interval _5262 = _8423;
                Interval _8428 = jet_add_derivative(_5261, _5262, intervalFailed);
                bool _8435;
                if (_8371.lo <= 0.0)
                {
                    _8435 = _8371.hi >= 0.0;
                }
                else
                {
                    _8435 = false;
                }
                float _8442;
                if (_8435)
                {
                    _8442 = 0.0;
                }
                else
                {
                    _8442 = precise::min(abs(_8371.lo), abs(_8371.hi));
                }
                float _8445 = precise::max(abs(_8371.lo), abs(_8371.hi));
                float _5247 = spvFMul(_8442, _8442);
                float _8446 = interval_down(_5247, intervalFailed);
                float _5248 = spvFMul(_8445, _8445);
                float _8448 = interval_up(_5248, intervalFailed);
                Interval _5249 = Interval{ 2.0, 2.0 };
                Interval _5250 = _8371;
                Interval _8449 = imul(_5249, _5250, intervalFailed, optical_product_upper);
                Interval _5251 = _8449;
                Interval _5252 = _8373;
                Interval _8450 = jet_mul_derivative(_5251, _5252, intervalFailed, optical_product_upper);
                Interval _5253 = Interval{ 2.0, 2.0 };
                Interval _5254 = _8371;
                Interval _8451 = imul(_5253, _5254, intervalFailed, optical_product_upper);
                Interval _5255 = _8451;
                Interval _5256 = _8375;
                Interval _8452 = jet_mul_derivative(_5255, _5256, intervalFailed, optical_product_upper);
                Interval _5241 = _8426;
                Interval _5242 = Interval{ precise::max(0.0, _8446), _8448 };
                Interval _8454 = iadd(_5241, _5242, intervalFailed);
                Interval _5243 = _8427;
                Interval _5244 = _8450;
                Interval _8455 = jet_add_derivative(_5243, _5244, intervalFailed);
                Interval _5245 = _8428;
                Interval _5246 = _8452;
                Interval _8456 = jet_add_derivative(_5245, _5246, intervalFailed);
                Interval _5232 = _8454;
                Interval _8458 = isqrt(_5232, intervalFailed);
                bool _8466;
                if (!intervalFailed)
                {
                    _8466 = intervalFailed;
                }
                else
                {
                    _8466 = false;
                }
                bool _8471;
                if (_8466)
                {
                    _8471 = jetFailureSite == 0u;
                }
                else
                {
                    _8471 = false;
                }
                if (_8471)
                {
                    jetFailureSite = 3u;
                    jetFailureArguments = float4(_8454.lo, _8454.hi, 0.0, 0.0);
                }
                if (_8458.lo <= 0.0)
                {
                    jetBranchKnown = false;
                }
                Interval _5233 = Interval{ 2.0, 2.0 };
                Interval _5234 = _8458;
                Interval _8478 = imul(_5233, _5234, intervalFailed, optical_product_upper);
                Interval _5235 = Interval{ 1.0, 1.0 };
                Interval _5236 = _8478;
                Interval _8480 = idiv(_5235, _5236, intervalFailed, interval_divide_upper);
                bool _8487;
                if (!intervalFailed)
                {
                    _8487 = intervalFailed;
                }
                else
                {
                    _8487 = false;
                }
                bool _8492;
                if (_8487)
                {
                    _8492 = jetFailureSite == 0u;
                }
                else
                {
                    _8492 = false;
                }
                if (_8492)
                {
                    jetFailureSite = 4u;
                    jetFailureArguments = float4(1.0, 1.0, _8478.lo, _8478.hi);
                }
                Interval _5237 = _8480;
                Interval _5238 = _8455;
                Interval _8496 = jet_mul_derivative(_5237, _5238, intervalFailed, optical_product_upper);
                Interval _5239 = _8480;
                Interval _5240 = _8456;
                Interval _8497 = jet_mul_derivative(_5239, _5240, intervalFailed, optical_product_upper);
                Interval _5218 = Interval{ 1.0, 1.0 };
                Interval _5219 = _8458;
                Interval _8499 = idiv(_5218, _5219, intervalFailed, interval_divide_upper);
                bool _8506;
                if (!intervalFailed)
                {
                    _8506 = intervalFailed;
                }
                else
                {
                    _8506 = false;
                }
                bool _8511;
                if (_8506)
                {
                    _8511 = jetFailureSite == 0u;
                }
                else
                {
                    _8511 = false;
                }
                if (_8511)
                {
                    jetFailureSite = 1u;
                    jetFailureArguments = float4(1.0, 1.0, _8458.lo, _8458.hi);
                }
                Interval _5220 = Interval{ 0.0, 0.0 };
                Interval _5221 = _8499;
                Interval _5222 = _8496;
                Interval _8515 = jet_mul_derivative(_5221, _5222, intervalFailed, optical_product_upper);
                Interval _5223 = Interval{ as_type<float>(as_type<uint>(_8515.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8515.lo) ^ 2147483648u) };
                Interval _8525 = jet_add_derivative(_5220, _5223, intervalFailed);
                Interval _5224 = _8525;
                Interval _5225 = _8458;
                Interval _8526 = jet_div_derivative(_5224, _5225, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _5226 = Interval{ 0.0, 0.0 };
                Interval _5227 = _8499;
                Interval _5228 = _8497;
                Interval _8527 = jet_mul_derivative(_5227, _5228, intervalFailed, optical_product_upper);
                Interval _5229 = Interval{ as_type<float>(as_type<uint>(_8527.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8527.lo) ^ 2147483648u) };
                Interval _8537 = jet_add_derivative(_5226, _5229, intervalFailed);
                Interval _5230 = _8537;
                Interval _5231 = _8458;
                Interval _8538 = jet_div_derivative(_5230, _5231, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                Interval _5204 = _8283;
                Interval _5205 = _8499;
                Interval _8539 = imul(_5204, _5205, intervalFailed, optical_product_upper);
                Interval _5206 = _8285;
                Interval _5207 = _8499;
                Interval _8540 = jet_mul_derivative(_5206, _5207, intervalFailed, optical_product_upper);
                Interval _5208 = _8540;
                Interval _5209 = _8283;
                Interval _5210 = _8526;
                Interval _8541 = jet_mul_derivative(_5209, _5210, intervalFailed, optical_product_upper);
                Interval _5211 = _8541;
                Interval _8542 = jet_add_derivative(_5208, _5211, intervalFailed);
                Interval _5212 = _8287;
                Interval _5213 = _8499;
                Interval _8543 = jet_mul_derivative(_5212, _5213, intervalFailed, optical_product_upper);
                Interval _5214 = _8543;
                Interval _5215 = _8283;
                Interval _5216 = _8538;
                Interval _8544 = jet_mul_derivative(_5215, _5216, intervalFailed, optical_product_upper);
                Interval _5217 = _8544;
                Interval _8545 = jet_add_derivative(_5214, _5217, intervalFailed);
                Interval _5190 = _8327;
                Interval _5191 = _8499;
                Interval _8546 = imul(_5190, _5191, intervalFailed, optical_product_upper);
                Interval _5192 = _8329;
                Interval _5193 = _8499;
                Interval _8547 = jet_mul_derivative(_5192, _5193, intervalFailed, optical_product_upper);
                Interval _5194 = _8547;
                Interval _5195 = _8327;
                Interval _5196 = _8526;
                Interval _8548 = jet_mul_derivative(_5195, _5196, intervalFailed, optical_product_upper);
                Interval _5197 = _8548;
                Interval _8549 = jet_add_derivative(_5194, _5197, intervalFailed);
                Interval _5198 = _8331;
                Interval _5199 = _8499;
                Interval _8550 = jet_mul_derivative(_5198, _5199, intervalFailed, optical_product_upper);
                Interval _5200 = _8550;
                Interval _5201 = _8327;
                Interval _5202 = _8538;
                Interval _8551 = jet_mul_derivative(_5201, _5202, intervalFailed, optical_product_upper);
                Interval _5203 = _8551;
                Interval _8552 = jet_add_derivative(_5200, _5203, intervalFailed);
                Interval _5176 = _8371;
                Interval _5177 = _8499;
                Interval _8553 = imul(_5176, _5177, intervalFailed, optical_product_upper);
                Interval _5178 = _8373;
                Interval _5179 = _8499;
                Interval _8554 = jet_mul_derivative(_5178, _5179, intervalFailed, optical_product_upper);
                Interval _5180 = _8554;
                Interval _5181 = _8371;
                Interval _5182 = _8526;
                Interval _8555 = jet_mul_derivative(_5181, _5182, intervalFailed, optical_product_upper);
                Interval _5183 = _8555;
                Interval _8556 = jet_add_derivative(_5180, _5183, intervalFailed);
                Interval _5184 = _8375;
                Interval _5185 = _8499;
                Interval _8557 = jet_mul_derivative(_5184, _5185, intervalFailed, optical_product_upper);
                Interval _5186 = _8557;
                Interval _5187 = _8371;
                Interval _5188 = _8538;
                Interval _8558 = jet_mul_derivative(_5187, _5188, intervalFailed, optical_product_upper);
                Interval _5189 = _8558;
                Interval _8559 = jet_add_derivative(_5186, _5189, intervalFailed);
                _8915 = _8539.lo;
                _8916 = _8539.hi;
                _8917 = _8542.lo;
                _8918 = _8542.hi;
                _8919 = _8545.lo;
                _8920 = _8545.hi;
                _8921 = _8546.lo;
                _8922 = _8546.hi;
                _8923 = _8549.lo;
                _8924 = _8549.hi;
                _8925 = _8552.lo;
                _8926 = _8552.hi;
                _8927 = _8553.lo;
                _8928 = _8553.hi;
                _8929 = _8556.lo;
                _8930 = _8556.hi;
                _8931 = _8559.lo;
                _8932 = _8559.hi;
            }
            else
            {
                float _8897;
                float _8898;
                float _8899;
                float _8900;
                float _8901;
                float _8902;
                float _8903;
                float _8904;
                float _8905;
                float _8906;
                float _8907;
                float _8908;
                float _8909;
                float _8910;
                float _8911;
                float _8912;
                float _8913;
                float _8914;
                float _5174 = 0.949999988079071044921875;
                __attribute__((unused)) float _8578 = interval_down(_5174, intervalFailed);
                float _5175 = 0.949999988079071044921875;
                float _8579 = interval_up(_5175, intervalFailed);
                if (_8237 > _8579)
                {
                    Interval _5160 = _8183;
                    Interval _5161 = Interval{ 0.0, 0.0 };
                    Interval _8581 = imul(_5160, _5161, intervalFailed, optical_product_upper);
                    Interval _5162 = _8186;
                    Interval _5163 = Interval{ 0.0, 0.0 };
                    Interval _8582 = jet_mul_derivative(_5162, _5163, intervalFailed, optical_product_upper);
                    Interval _5164 = _8582;
                    Interval _5165 = _8183;
                    Interval _5166 = Interval{ 0.0, 0.0 };
                    Interval _8583 = jet_mul_derivative(_5165, _5166, intervalFailed, optical_product_upper);
                    Interval _5167 = _8583;
                    Interval _8584 = jet_add_derivative(_5164, _5167, intervalFailed);
                    Interval _5168 = _8189;
                    Interval _5169 = Interval{ 0.0, 0.0 };
                    Interval _8585 = jet_mul_derivative(_5168, _5169, intervalFailed, optical_product_upper);
                    Interval _5170 = _8585;
                    Interval _5171 = _8183;
                    Interval _5172 = Interval{ 0.0, 0.0 };
                    Interval _8586 = jet_mul_derivative(_5171, _5172, intervalFailed, optical_product_upper);
                    Interval _5173 = _8586;
                    Interval _8587 = jet_add_derivative(_5170, _5173, intervalFailed);
                    Interval _5146 = _8192;
                    Interval _5147 = Interval{ 0.0, 0.0 };
                    Interval _8588 = imul(_5146, _5147, intervalFailed, optical_product_upper);
                    Interval _5148 = _8195;
                    Interval _5149 = Interval{ 0.0, 0.0 };
                    Interval _8589 = jet_mul_derivative(_5148, _5149, intervalFailed, optical_product_upper);
                    Interval _5150 = _8589;
                    Interval _5151 = _8192;
                    Interval _5152 = Interval{ 0.0, 0.0 };
                    Interval _8590 = jet_mul_derivative(_5151, _5152, intervalFailed, optical_product_upper);
                    Interval _5153 = _8590;
                    Interval _8591 = jet_add_derivative(_5150, _5153, intervalFailed);
                    Interval _5154 = _8198;
                    Interval _5155 = Interval{ 0.0, 0.0 };
                    Interval _8592 = jet_mul_derivative(_5154, _5155, intervalFailed, optical_product_upper);
                    Interval _5156 = _8592;
                    Interval _5157 = _8192;
                    Interval _5158 = Interval{ 0.0, 0.0 };
                    Interval _8593 = jet_mul_derivative(_5157, _5158, intervalFailed, optical_product_upper);
                    Interval _5159 = _8593;
                    Interval _8594 = jet_add_derivative(_5156, _5159, intervalFailed);
                    Interval _5140 = _8581;
                    Interval _5141 = Interval{ as_type<float>(as_type<uint>(_8588.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8588.lo) ^ 2147483648u) };
                    Interval _8620 = iadd(_5140, _5141, intervalFailed);
                    Interval _5142 = _8584;
                    Interval _5143 = Interval{ as_type<float>(as_type<uint>(_8591.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8591.lo) ^ 2147483648u) };
                    Interval _8622 = jet_add_derivative(_5142, _5143, intervalFailed);
                    Interval _5144 = _8587;
                    Interval _5145 = Interval{ as_type<float>(as_type<uint>(_8594.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8594.lo) ^ 2147483648u) };
                    Interval _8624 = jet_add_derivative(_5144, _5145, intervalFailed);
                    Interval _5126 = _8192;
                    Interval _5127 = Interval{ 1.0, 1.0 };
                    Interval _8625 = imul(_5126, _5127, intervalFailed, optical_product_upper);
                    Interval _5128 = _8195;
                    Interval _5129 = Interval{ 1.0, 1.0 };
                    Interval _8626 = jet_mul_derivative(_5128, _5129, intervalFailed, optical_product_upper);
                    Interval _5130 = _8626;
                    Interval _5131 = _8192;
                    Interval _5132 = Interval{ 0.0, 0.0 };
                    Interval _8627 = jet_mul_derivative(_5131, _5132, intervalFailed, optical_product_upper);
                    Interval _5133 = _8627;
                    Interval _8628 = jet_add_derivative(_5130, _5133, intervalFailed);
                    Interval _5134 = _8198;
                    Interval _5135 = Interval{ 1.0, 1.0 };
                    Interval _8629 = jet_mul_derivative(_5134, _5135, intervalFailed, optical_product_upper);
                    Interval _5136 = _8629;
                    Interval _5137 = _8192;
                    Interval _5138 = Interval{ 0.0, 0.0 };
                    Interval _8630 = jet_mul_derivative(_5137, _5138, intervalFailed, optical_product_upper);
                    Interval _5139 = _8630;
                    Interval _8631 = jet_add_derivative(_5136, _5139, intervalFailed);
                    Interval _5112 = _8174;
                    Interval _5113 = Interval{ 0.0, 0.0 };
                    Interval _8632 = imul(_5112, _5113, intervalFailed, optical_product_upper);
                    Interval _5114 = _8177;
                    Interval _5115 = Interval{ 0.0, 0.0 };
                    Interval _8633 = jet_mul_derivative(_5114, _5115, intervalFailed, optical_product_upper);
                    Interval _5116 = _8633;
                    Interval _5117 = _8174;
                    Interval _5118 = Interval{ 0.0, 0.0 };
                    Interval _8634 = jet_mul_derivative(_5117, _5118, intervalFailed, optical_product_upper);
                    Interval _5119 = _8634;
                    Interval _8635 = jet_add_derivative(_5116, _5119, intervalFailed);
                    Interval _5120 = _8180;
                    Interval _5121 = Interval{ 0.0, 0.0 };
                    Interval _8636 = jet_mul_derivative(_5120, _5121, intervalFailed, optical_product_upper);
                    Interval _5122 = _8636;
                    Interval _5123 = _8174;
                    Interval _5124 = Interval{ 0.0, 0.0 };
                    Interval _8637 = jet_mul_derivative(_5123, _5124, intervalFailed, optical_product_upper);
                    Interval _5125 = _8637;
                    Interval _8638 = jet_add_derivative(_5122, _5125, intervalFailed);
                    Interval _5106 = _8625;
                    Interval _5107 = Interval{ as_type<float>(as_type<uint>(_8632.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8632.lo) ^ 2147483648u) };
                    Interval _8664 = iadd(_5106, _5107, intervalFailed);
                    Interval _5108 = _8628;
                    Interval _5109 = Interval{ as_type<float>(as_type<uint>(_8635.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8635.lo) ^ 2147483648u) };
                    Interval _8666 = jet_add_derivative(_5108, _5109, intervalFailed);
                    Interval _5110 = _8631;
                    Interval _5111 = Interval{ as_type<float>(as_type<uint>(_8638.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8638.lo) ^ 2147483648u) };
                    Interval _8668 = jet_add_derivative(_5110, _5111, intervalFailed);
                    Interval _5092 = _8174;
                    Interval _5093 = Interval{ 0.0, 0.0 };
                    Interval _8669 = imul(_5092, _5093, intervalFailed, optical_product_upper);
                    Interval _5094 = _8177;
                    Interval _5095 = Interval{ 0.0, 0.0 };
                    Interval _8670 = jet_mul_derivative(_5094, _5095, intervalFailed, optical_product_upper);
                    Interval _5096 = _8670;
                    Interval _5097 = _8174;
                    Interval _5098 = Interval{ 0.0, 0.0 };
                    Interval _8671 = jet_mul_derivative(_5097, _5098, intervalFailed, optical_product_upper);
                    Interval _5099 = _8671;
                    Interval _8672 = jet_add_derivative(_5096, _5099, intervalFailed);
                    Interval _5100 = _8180;
                    Interval _5101 = Interval{ 0.0, 0.0 };
                    Interval _8673 = jet_mul_derivative(_5100, _5101, intervalFailed, optical_product_upper);
                    Interval _5102 = _8673;
                    Interval _5103 = _8174;
                    Interval _5104 = Interval{ 0.0, 0.0 };
                    Interval _8674 = jet_mul_derivative(_5103, _5104, intervalFailed, optical_product_upper);
                    Interval _5105 = _8674;
                    Interval _8675 = jet_add_derivative(_5102, _5105, intervalFailed);
                    Interval _5078 = _8183;
                    Interval _5079 = Interval{ 1.0, 1.0 };
                    Interval _8676 = imul(_5078, _5079, intervalFailed, optical_product_upper);
                    Interval _5080 = _8186;
                    Interval _5081 = Interval{ 1.0, 1.0 };
                    Interval _8677 = jet_mul_derivative(_5080, _5081, intervalFailed, optical_product_upper);
                    Interval _5082 = _8677;
                    Interval _5083 = _8183;
                    Interval _5084 = Interval{ 0.0, 0.0 };
                    Interval _8678 = jet_mul_derivative(_5083, _5084, intervalFailed, optical_product_upper);
                    Interval _5085 = _8678;
                    Interval _8679 = jet_add_derivative(_5082, _5085, intervalFailed);
                    Interval _5086 = _8189;
                    Interval _5087 = Interval{ 1.0, 1.0 };
                    Interval _8680 = jet_mul_derivative(_5086, _5087, intervalFailed, optical_product_upper);
                    Interval _5088 = _8680;
                    Interval _5089 = _8183;
                    Interval _5090 = Interval{ 0.0, 0.0 };
                    Interval _8681 = jet_mul_derivative(_5089, _5090, intervalFailed, optical_product_upper);
                    Interval _5091 = _8681;
                    Interval _8682 = jet_add_derivative(_5088, _5091, intervalFailed);
                    Interval _5072 = _8669;
                    Interval _5073 = Interval{ as_type<float>(as_type<uint>(_8676.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8676.lo) ^ 2147483648u) };
                    Interval _8708 = iadd(_5072, _5073, intervalFailed);
                    Interval _5074 = _8672;
                    Interval _5075 = Interval{ as_type<float>(as_type<uint>(_8679.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8679.lo) ^ 2147483648u) };
                    Interval _8710 = jet_add_derivative(_5074, _5075, intervalFailed);
                    Interval _5076 = _8675;
                    Interval _5077 = Interval{ as_type<float>(as_type<uint>(_8682.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8682.lo) ^ 2147483648u) };
                    Interval _8712 = jet_add_derivative(_5076, _5077, intervalFailed);
                    bool _8719;
                    if (_8620.lo <= 0.0)
                    {
                        _8719 = _8620.hi >= 0.0;
                    }
                    else
                    {
                        _8719 = false;
                    }
                    float _8726;
                    if (_8719)
                    {
                        _8726 = 0.0;
                    }
                    else
                    {
                        _8726 = precise::min(abs(_8620.lo), abs(_8620.hi));
                    }
                    float _8729 = precise::max(abs(_8620.lo), abs(_8620.hi));
                    float _5062 = spvFMul(_8726, _8726);
                    float _8730 = interval_down(_5062, intervalFailed);
                    float _5063 = spvFMul(_8729, _8729);
                    float _8732 = interval_up(_5063, intervalFailed);
                    Interval _5064 = Interval{ 2.0, 2.0 };
                    Interval _5065 = _8620;
                    Interval _8733 = imul(_5064, _5065, intervalFailed, optical_product_upper);
                    Interval _5066 = _8733;
                    Interval _5067 = _8622;
                    Interval _8734 = jet_mul_derivative(_5066, _5067, intervalFailed, optical_product_upper);
                    Interval _5068 = Interval{ 2.0, 2.0 };
                    Interval _5069 = _8620;
                    Interval _8735 = imul(_5068, _5069, intervalFailed, optical_product_upper);
                    Interval _5070 = _8735;
                    Interval _5071 = _8624;
                    Interval _8736 = jet_mul_derivative(_5070, _5071, intervalFailed, optical_product_upper);
                    bool _8743;
                    if (_8664.lo <= 0.0)
                    {
                        _8743 = _8664.hi >= 0.0;
                    }
                    else
                    {
                        _8743 = false;
                    }
                    float _8750;
                    if (_8743)
                    {
                        _8750 = 0.0;
                    }
                    else
                    {
                        _8750 = precise::min(abs(_8664.lo), abs(_8664.hi));
                    }
                    float _8753 = precise::max(abs(_8664.lo), abs(_8664.hi));
                    float _5052 = spvFMul(_8750, _8750);
                    float _8754 = interval_down(_5052, intervalFailed);
                    float _5053 = spvFMul(_8753, _8753);
                    float _8756 = interval_up(_5053, intervalFailed);
                    Interval _5054 = Interval{ 2.0, 2.0 };
                    Interval _5055 = _8664;
                    Interval _8757 = imul(_5054, _5055, intervalFailed, optical_product_upper);
                    Interval _5056 = _8757;
                    Interval _5057 = _8666;
                    Interval _8758 = jet_mul_derivative(_5056, _5057, intervalFailed, optical_product_upper);
                    Interval _5058 = Interval{ 2.0, 2.0 };
                    Interval _5059 = _8664;
                    Interval _8759 = imul(_5058, _5059, intervalFailed, optical_product_upper);
                    Interval _5060 = _8759;
                    Interval _5061 = _8668;
                    Interval _8760 = jet_mul_derivative(_5060, _5061, intervalFailed, optical_product_upper);
                    Interval _5046 = Interval{ precise::max(0.0, _8730), _8732 };
                    Interval _5047 = Interval{ precise::max(0.0, _8754), _8756 };
                    Interval _8763 = iadd(_5046, _5047, intervalFailed);
                    Interval _5048 = _8734;
                    Interval _5049 = _8758;
                    Interval _8764 = jet_add_derivative(_5048, _5049, intervalFailed);
                    Interval _5050 = _8736;
                    Interval _5051 = _8760;
                    Interval _8765 = jet_add_derivative(_5050, _5051, intervalFailed);
                    bool _8772;
                    if (_8708.lo <= 0.0)
                    {
                        _8772 = _8708.hi >= 0.0;
                    }
                    else
                    {
                        _8772 = false;
                    }
                    float _8779;
                    if (_8772)
                    {
                        _8779 = 0.0;
                    }
                    else
                    {
                        _8779 = precise::min(abs(_8708.lo), abs(_8708.hi));
                    }
                    float _8782 = precise::max(abs(_8708.lo), abs(_8708.hi));
                    float _5036 = spvFMul(_8779, _8779);
                    float _8783 = interval_down(_5036, intervalFailed);
                    float _5037 = spvFMul(_8782, _8782);
                    float _8785 = interval_up(_5037, intervalFailed);
                    Interval _5038 = Interval{ 2.0, 2.0 };
                    Interval _5039 = _8708;
                    Interval _8786 = imul(_5038, _5039, intervalFailed, optical_product_upper);
                    Interval _5040 = _8786;
                    Interval _5041 = _8710;
                    Interval _8787 = jet_mul_derivative(_5040, _5041, intervalFailed, optical_product_upper);
                    Interval _5042 = Interval{ 2.0, 2.0 };
                    Interval _5043 = _8708;
                    Interval _8788 = imul(_5042, _5043, intervalFailed, optical_product_upper);
                    Interval _5044 = _8788;
                    Interval _5045 = _8712;
                    Interval _8789 = jet_mul_derivative(_5044, _5045, intervalFailed, optical_product_upper);
                    Interval _5030 = _8763;
                    Interval _5031 = Interval{ precise::max(0.0, _8783), _8785 };
                    Interval _8791 = iadd(_5030, _5031, intervalFailed);
                    Interval _5032 = _8764;
                    Interval _5033 = _8787;
                    Interval _8792 = jet_add_derivative(_5032, _5033, intervalFailed);
                    Interval _5034 = _8765;
                    Interval _5035 = _8789;
                    Interval _8793 = jet_add_derivative(_5034, _5035, intervalFailed);
                    Interval _5021 = _8791;
                    Interval _8795 = isqrt(_5021, intervalFailed);
                    bool _8803;
                    if (!intervalFailed)
                    {
                        _8803 = intervalFailed;
                    }
                    else
                    {
                        _8803 = false;
                    }
                    bool _8808;
                    if (_8803)
                    {
                        _8808 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8808 = false;
                    }
                    if (_8808)
                    {
                        jetFailureSite = 3u;
                        jetFailureArguments = float4(_8791.lo, _8791.hi, 0.0, 0.0);
                    }
                    if (_8795.lo <= 0.0)
                    {
                        jetBranchKnown = false;
                    }
                    Interval _5022 = Interval{ 2.0, 2.0 };
                    Interval _5023 = _8795;
                    Interval _8815 = imul(_5022, _5023, intervalFailed, optical_product_upper);
                    Interval _5024 = Interval{ 1.0, 1.0 };
                    Interval _5025 = _8815;
                    Interval _8817 = idiv(_5024, _5025, intervalFailed, interval_divide_upper);
                    bool _8824;
                    if (!intervalFailed)
                    {
                        _8824 = intervalFailed;
                    }
                    else
                    {
                        _8824 = false;
                    }
                    bool _8829;
                    if (_8824)
                    {
                        _8829 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8829 = false;
                    }
                    if (_8829)
                    {
                        jetFailureSite = 4u;
                        jetFailureArguments = float4(1.0, 1.0, _8815.lo, _8815.hi);
                    }
                    Interval _5026 = _8817;
                    Interval _5027 = _8792;
                    Interval _8833 = jet_mul_derivative(_5026, _5027, intervalFailed, optical_product_upper);
                    Interval _5028 = _8817;
                    Interval _5029 = _8793;
                    Interval _8834 = jet_mul_derivative(_5028, _5029, intervalFailed, optical_product_upper);
                    Interval _5007 = Interval{ 1.0, 1.0 };
                    Interval _5008 = _8795;
                    Interval _8836 = idiv(_5007, _5008, intervalFailed, interval_divide_upper);
                    bool _8843;
                    if (!intervalFailed)
                    {
                        _8843 = intervalFailed;
                    }
                    else
                    {
                        _8843 = false;
                    }
                    bool _8848;
                    if (_8843)
                    {
                        _8848 = jetFailureSite == 0u;
                    }
                    else
                    {
                        _8848 = false;
                    }
                    if (_8848)
                    {
                        jetFailureSite = 1u;
                        jetFailureArguments = float4(1.0, 1.0, _8795.lo, _8795.hi);
                    }
                    Interval _5009 = Interval{ 0.0, 0.0 };
                    Interval _5010 = _8836;
                    Interval _5011 = _8833;
                    Interval _8852 = jet_mul_derivative(_5010, _5011, intervalFailed, optical_product_upper);
                    Interval _5012 = Interval{ as_type<float>(as_type<uint>(_8852.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8852.lo) ^ 2147483648u) };
                    Interval _8862 = jet_add_derivative(_5009, _5012, intervalFailed);
                    Interval _5013 = _8862;
                    Interval _5014 = _8795;
                    Interval _8863 = jet_div_derivative(_5013, _5014, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                    Interval _5015 = Interval{ 0.0, 0.0 };
                    Interval _5016 = _8836;
                    Interval _5017 = _8834;
                    Interval _8864 = jet_mul_derivative(_5016, _5017, intervalFailed, optical_product_upper);
                    Interval _5018 = Interval{ as_type<float>(as_type<uint>(_8864.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_8864.lo) ^ 2147483648u) };
                    Interval _8874 = jet_add_derivative(_5015, _5018, intervalFailed);
                    Interval _5019 = _8874;
                    Interval _5020 = _8795;
                    Interval _8875 = jet_div_derivative(_5019, _5020, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
                    Interval _4993 = _8620;
                    Interval _4994 = _8836;
                    Interval _8876 = imul(_4993, _4994, intervalFailed, optical_product_upper);
                    Interval _4995 = _8622;
                    Interval _4996 = _8836;
                    Interval _8877 = jet_mul_derivative(_4995, _4996, intervalFailed, optical_product_upper);
                    Interval _4997 = _8877;
                    Interval _4998 = _8620;
                    Interval _4999 = _8863;
                    Interval _8878 = jet_mul_derivative(_4998, _4999, intervalFailed, optical_product_upper);
                    Interval _5000 = _8878;
                    Interval _8879 = jet_add_derivative(_4997, _5000, intervalFailed);
                    Interval _5001 = _8624;
                    Interval _5002 = _8836;
                    Interval _8880 = jet_mul_derivative(_5001, _5002, intervalFailed, optical_product_upper);
                    Interval _5003 = _8880;
                    Interval _5004 = _8620;
                    Interval _5005 = _8875;
                    Interval _8881 = jet_mul_derivative(_5004, _5005, intervalFailed, optical_product_upper);
                    Interval _5006 = _8881;
                    Interval _8882 = jet_add_derivative(_5003, _5006, intervalFailed);
                    Interval _4979 = _8664;
                    Interval _4980 = _8836;
                    Interval _8883 = imul(_4979, _4980, intervalFailed, optical_product_upper);
                    Interval _4981 = _8666;
                    Interval _4982 = _8836;
                    Interval _8884 = jet_mul_derivative(_4981, _4982, intervalFailed, optical_product_upper);
                    Interval _4983 = _8884;
                    Interval _4984 = _8664;
                    Interval _4985 = _8863;
                    Interval _8885 = jet_mul_derivative(_4984, _4985, intervalFailed, optical_product_upper);
                    Interval _4986 = _8885;
                    Interval _8886 = jet_add_derivative(_4983, _4986, intervalFailed);
                    Interval _4987 = _8668;
                    Interval _4988 = _8836;
                    Interval _8887 = jet_mul_derivative(_4987, _4988, intervalFailed, optical_product_upper);
                    Interval _4989 = _8887;
                    Interval _4990 = _8664;
                    Interval _4991 = _8875;
                    Interval _8888 = jet_mul_derivative(_4990, _4991, intervalFailed, optical_product_upper);
                    Interval _4992 = _8888;
                    Interval _8889 = jet_add_derivative(_4989, _4992, intervalFailed);
                    Interval _4965 = _8708;
                    Interval _4966 = _8836;
                    Interval _8890 = imul(_4965, _4966, intervalFailed, optical_product_upper);
                    Interval _4967 = _8710;
                    Interval _4968 = _8836;
                    Interval _8891 = jet_mul_derivative(_4967, _4968, intervalFailed, optical_product_upper);
                    Interval _4969 = _8891;
                    Interval _4970 = _8708;
                    Interval _4971 = _8863;
                    Interval _8892 = jet_mul_derivative(_4970, _4971, intervalFailed, optical_product_upper);
                    Interval _4972 = _8892;
                    Interval _8893 = jet_add_derivative(_4969, _4972, intervalFailed);
                    Interval _4973 = _8712;
                    Interval _4974 = _8836;
                    Interval _8894 = jet_mul_derivative(_4973, _4974, intervalFailed, optical_product_upper);
                    Interval _4975 = _8894;
                    Interval _4976 = _8708;
                    Interval _4977 = _8875;
                    Interval _8895 = jet_mul_derivative(_4976, _4977, intervalFailed, optical_product_upper);
                    Interval _4978 = _8895;
                    Interval _8896 = jet_add_derivative(_4975, _4978, intervalFailed);
                    _8897 = _8876.lo;
                    _8898 = _8876.hi;
                    _8899 = _8879.lo;
                    _8900 = _8879.hi;
                    _8901 = _8882.lo;
                    _8902 = _8882.hi;
                    _8903 = _8883.lo;
                    _8904 = _8883.hi;
                    _8905 = _8886.lo;
                    _8906 = _8886.hi;
                    _8907 = _8889.lo;
                    _8908 = _8889.hi;
                    _8909 = _8890.lo;
                    _8910 = _8890.hi;
                    _8911 = _8893.lo;
                    _8912 = _8893.hi;
                    _8913 = _8896.lo;
                    _8914 = _8896.hi;
                }
                else
                {
                    return false;
                }
                _8915 = _8897;
                _8916 = _8898;
                _8917 = _8899;
                _8918 = _8900;
                _8919 = _8901;
                _8920 = _8902;
                _8921 = _8903;
                _8922 = _8904;
                _8923 = _8905;
                _8924 = _8906;
                _8925 = _8907;
                _8926 = _8908;
                _8927 = _8909;
                _8928 = _8910;
                _8929 = _8911;
                _8930 = _8912;
                _8931 = _8913;
                _8932 = _8914;
            }
            uint _8936 = uint(receiver.settings.y);
            float _8940 = float(_2234[_8936].x);
            float _4963 = _8940;
            float _4964 = 1000.0;
            Interval _8942 = iratio(_4963, _4964, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _8947;
            if (!intervalFailed)
            {
                _8947 = intervalFailed;
            }
            else
            {
                _8947 = false;
            }
            bool _8952;
            if (_8947)
            {
                _8952 = jetFailureSite == 0u;
            }
            else
            {
                _8952 = false;
            }
            if (_8952)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(_8940, _8940, 1000.0, 1000.0);
            }
            Interval _4949 = Interval{ _8915, _8916 };
            Interval _4950 = _8942;
            Interval _8957 = imul(_4949, _4950, intervalFailed, optical_product_upper);
            Interval _4951 = Interval{ _8917, _8918 };
            Interval _4952 = _8942;
            Interval _8959 = jet_mul_derivative(_4951, _4952, intervalFailed, optical_product_upper);
            Interval _4953 = _8959;
            Interval _4954 = Interval{ _8915, _8916 };
            Interval _4955 = Interval{ 0.0, 0.0 };
            Interval _8961 = jet_mul_derivative(_4954, _4955, intervalFailed, optical_product_upper);
            Interval _4956 = _8961;
            Interval _8962 = jet_add_derivative(_4953, _4956, intervalFailed);
            Interval _4957 = Interval{ _8919, _8920 };
            Interval _4958 = _8942;
            Interval _8964 = jet_mul_derivative(_4957, _4958, intervalFailed, optical_product_upper);
            Interval _4959 = _8964;
            Interval _4960 = Interval{ _8915, _8916 };
            Interval _4961 = Interval{ 0.0, 0.0 };
            Interval _8966 = jet_mul_derivative(_4960, _4961, intervalFailed, optical_product_upper);
            Interval _4962 = _8966;
            Interval _8967 = jet_add_derivative(_4959, _4962, intervalFailed);
            Interval _4935 = Interval{ _8921, _8922 };
            Interval _4936 = _8942;
            Interval _8969 = imul(_4935, _4936, intervalFailed, optical_product_upper);
            Interval _4937 = Interval{ _8923, _8924 };
            Interval _4938 = _8942;
            Interval _8971 = jet_mul_derivative(_4937, _4938, intervalFailed, optical_product_upper);
            Interval _4939 = _8971;
            Interval _4940 = Interval{ _8921, _8922 };
            Interval _4941 = Interval{ 0.0, 0.0 };
            Interval _8973 = jet_mul_derivative(_4940, _4941, intervalFailed, optical_product_upper);
            Interval _4942 = _8973;
            Interval _8974 = jet_add_derivative(_4939, _4942, intervalFailed);
            Interval _4943 = Interval{ _8925, _8926 };
            Interval _4944 = _8942;
            Interval _8976 = jet_mul_derivative(_4943, _4944, intervalFailed, optical_product_upper);
            Interval _4945 = _8976;
            Interval _4946 = Interval{ _8921, _8922 };
            Interval _4947 = Interval{ 0.0, 0.0 };
            Interval _8978 = jet_mul_derivative(_4946, _4947, intervalFailed, optical_product_upper);
            Interval _4948 = _8978;
            Interval _8979 = jet_add_derivative(_4945, _4948, intervalFailed);
            Interval _4921 = Interval{ _8927, _8928 };
            Interval _4922 = _8942;
            Interval _8981 = imul(_4921, _4922, intervalFailed, optical_product_upper);
            Interval _4923 = Interval{ _8929, _8930 };
            Interval _4924 = _8942;
            Interval _8983 = jet_mul_derivative(_4923, _4924, intervalFailed, optical_product_upper);
            Interval _4925 = _8983;
            Interval _4926 = Interval{ _8927, _8928 };
            Interval _4927 = Interval{ 0.0, 0.0 };
            Interval _8985 = jet_mul_derivative(_4926, _4927, intervalFailed, optical_product_upper);
            Interval _4928 = _8985;
            Interval _8986 = jet_add_derivative(_4925, _4928, intervalFailed);
            Interval _4929 = Interval{ _8931, _8932 };
            Interval _4930 = _8942;
            Interval _8988 = jet_mul_derivative(_4929, _4930, intervalFailed, optical_product_upper);
            Interval _4931 = _8988;
            Interval _4932 = Interval{ _8927, _8928 };
            Interval _4933 = Interval{ 0.0, 0.0 };
            Interval _8990 = jet_mul_derivative(_4932, _4933, intervalFailed, optical_product_upper);
            Interval _4934 = _8990;
            Interval _8991 = jet_add_derivative(_4931, _4934, intervalFailed);
            Interval _4907 = _8183;
            Interval _4908 = Interval{ _8927, _8928 };
            Interval _8993 = imul(_4907, _4908, intervalFailed, optical_product_upper);
            Interval _4909 = _8186;
            Interval _4910 = Interval{ _8927, _8928 };
            Interval _8995 = jet_mul_derivative(_4909, _4910, intervalFailed, optical_product_upper);
            Interval _4911 = _8995;
            Interval _4912 = _8183;
            Interval _4913 = Interval{ _8929, _8930 };
            Interval _8997 = jet_mul_derivative(_4912, _4913, intervalFailed, optical_product_upper);
            Interval _4914 = _8997;
            Interval _8998 = jet_add_derivative(_4911, _4914, intervalFailed);
            Interval _4915 = _8189;
            Interval _4916 = Interval{ _8927, _8928 };
            Interval _9000 = jet_mul_derivative(_4915, _4916, intervalFailed, optical_product_upper);
            Interval _4917 = _9000;
            Interval _4918 = _8183;
            Interval _4919 = Interval{ _8931, _8932 };
            Interval _9002 = jet_mul_derivative(_4918, _4919, intervalFailed, optical_product_upper);
            Interval _4920 = _9002;
            Interval _9003 = jet_add_derivative(_4917, _4920, intervalFailed);
            Interval _4893 = _8192;
            Interval _4894 = Interval{ _8921, _8922 };
            Interval _9005 = imul(_4893, _4894, intervalFailed, optical_product_upper);
            Interval _4895 = _8195;
            Interval _4896 = Interval{ _8921, _8922 };
            Interval _9007 = jet_mul_derivative(_4895, _4896, intervalFailed, optical_product_upper);
            Interval _4897 = _9007;
            Interval _4898 = _8192;
            Interval _4899 = Interval{ _8923, _8924 };
            Interval _9009 = jet_mul_derivative(_4898, _4899, intervalFailed, optical_product_upper);
            Interval _4900 = _9009;
            Interval _9010 = jet_add_derivative(_4897, _4900, intervalFailed);
            Interval _4901 = _8198;
            Interval _4902 = Interval{ _8921, _8922 };
            Interval _9012 = jet_mul_derivative(_4901, _4902, intervalFailed, optical_product_upper);
            Interval _4903 = _9012;
            Interval _4904 = _8192;
            Interval _4905 = Interval{ _8925, _8926 };
            Interval _9014 = jet_mul_derivative(_4904, _4905, intervalFailed, optical_product_upper);
            Interval _4906 = _9014;
            Interval _9015 = jet_add_derivative(_4903, _4906, intervalFailed);
            Interval _4887 = _8993;
            Interval _4888 = Interval{ as_type<float>(as_type<uint>(_9005.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9005.lo) ^ 2147483648u) };
            Interval _9041 = iadd(_4887, _4888, intervalFailed);
            Interval _4889 = _8998;
            Interval _4890 = Interval{ as_type<float>(as_type<uint>(_9010.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9010.lo) ^ 2147483648u) };
            Interval _9043 = jet_add_derivative(_4889, _4890, intervalFailed);
            Interval _4891 = _9003;
            Interval _4892 = Interval{ as_type<float>(as_type<uint>(_9015.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9015.lo) ^ 2147483648u) };
            Interval _9045 = jet_add_derivative(_4891, _4892, intervalFailed);
            Interval _4873 = _8192;
            Interval _4874 = Interval{ _8915, _8916 };
            Interval _9047 = imul(_4873, _4874, intervalFailed, optical_product_upper);
            Interval _4875 = _8195;
            Interval _4876 = Interval{ _8915, _8916 };
            Interval _9049 = jet_mul_derivative(_4875, _4876, intervalFailed, optical_product_upper);
            Interval _4877 = _9049;
            Interval _4878 = _8192;
            Interval _4879 = Interval{ _8917, _8918 };
            Interval _9051 = jet_mul_derivative(_4878, _4879, intervalFailed, optical_product_upper);
            Interval _4880 = _9051;
            Interval _9052 = jet_add_derivative(_4877, _4880, intervalFailed);
            Interval _4881 = _8198;
            Interval _4882 = Interval{ _8915, _8916 };
            Interval _9054 = jet_mul_derivative(_4881, _4882, intervalFailed, optical_product_upper);
            Interval _4883 = _9054;
            Interval _4884 = _8192;
            Interval _4885 = Interval{ _8919, _8920 };
            Interval _9056 = jet_mul_derivative(_4884, _4885, intervalFailed, optical_product_upper);
            Interval _4886 = _9056;
            Interval _9057 = jet_add_derivative(_4883, _4886, intervalFailed);
            Interval _4859 = _8174;
            Interval _4860 = Interval{ _8927, _8928 };
            Interval _9059 = imul(_4859, _4860, intervalFailed, optical_product_upper);
            Interval _4861 = _8177;
            Interval _4862 = Interval{ _8927, _8928 };
            Interval _9061 = jet_mul_derivative(_4861, _4862, intervalFailed, optical_product_upper);
            Interval _4863 = _9061;
            Interval _4864 = _8174;
            Interval _4865 = Interval{ _8929, _8930 };
            Interval _9063 = jet_mul_derivative(_4864, _4865, intervalFailed, optical_product_upper);
            Interval _4866 = _9063;
            Interval _9064 = jet_add_derivative(_4863, _4866, intervalFailed);
            Interval _4867 = _8180;
            Interval _4868 = Interval{ _8927, _8928 };
            Interval _9066 = jet_mul_derivative(_4867, _4868, intervalFailed, optical_product_upper);
            Interval _4869 = _9066;
            Interval _4870 = _8174;
            Interval _4871 = Interval{ _8931, _8932 };
            Interval _9068 = jet_mul_derivative(_4870, _4871, intervalFailed, optical_product_upper);
            Interval _4872 = _9068;
            Interval _9069 = jet_add_derivative(_4869, _4872, intervalFailed);
            Interval _4853 = _9047;
            Interval _4854 = Interval{ as_type<float>(as_type<uint>(_9059.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9059.lo) ^ 2147483648u) };
            Interval _9095 = iadd(_4853, _4854, intervalFailed);
            Interval _4855 = _9052;
            Interval _4856 = Interval{ as_type<float>(as_type<uint>(_9064.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9064.lo) ^ 2147483648u) };
            Interval _9097 = jet_add_derivative(_4855, _4856, intervalFailed);
            Interval _4857 = _9057;
            Interval _4858 = Interval{ as_type<float>(as_type<uint>(_9069.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9069.lo) ^ 2147483648u) };
            Interval _9099 = jet_add_derivative(_4857, _4858, intervalFailed);
            Interval _4839 = _8174;
            Interval _4840 = Interval{ _8921, _8922 };
            Interval _9101 = imul(_4839, _4840, intervalFailed, optical_product_upper);
            Interval _4841 = _8177;
            Interval _4842 = Interval{ _8921, _8922 };
            Interval _9103 = jet_mul_derivative(_4841, _4842, intervalFailed, optical_product_upper);
            Interval _4843 = _9103;
            Interval _4844 = _8174;
            Interval _4845 = Interval{ _8923, _8924 };
            Interval _9105 = jet_mul_derivative(_4844, _4845, intervalFailed, optical_product_upper);
            Interval _4846 = _9105;
            Interval _9106 = jet_add_derivative(_4843, _4846, intervalFailed);
            Interval _4847 = _8180;
            Interval _4848 = Interval{ _8921, _8922 };
            Interval _9108 = jet_mul_derivative(_4847, _4848, intervalFailed, optical_product_upper);
            Interval _4849 = _9108;
            Interval _4850 = _8174;
            Interval _4851 = Interval{ _8925, _8926 };
            Interval _9110 = jet_mul_derivative(_4850, _4851, intervalFailed, optical_product_upper);
            Interval _4852 = _9110;
            Interval _9111 = jet_add_derivative(_4849, _4852, intervalFailed);
            Interval _4825 = _8183;
            Interval _4826 = Interval{ _8915, _8916 };
            Interval _9113 = imul(_4825, _4826, intervalFailed, optical_product_upper);
            Interval _4827 = _8186;
            Interval _4828 = Interval{ _8915, _8916 };
            Interval _9115 = jet_mul_derivative(_4827, _4828, intervalFailed, optical_product_upper);
            Interval _4829 = _9115;
            Interval _4830 = _8183;
            Interval _4831 = Interval{ _8917, _8918 };
            Interval _9117 = jet_mul_derivative(_4830, _4831, intervalFailed, optical_product_upper);
            Interval _4832 = _9117;
            Interval _9118 = jet_add_derivative(_4829, _4832, intervalFailed);
            Interval _4833 = _8189;
            Interval _4834 = Interval{ _8915, _8916 };
            Interval _9120 = jet_mul_derivative(_4833, _4834, intervalFailed, optical_product_upper);
            Interval _4835 = _9120;
            Interval _4836 = _8183;
            Interval _4837 = Interval{ _8919, _8920 };
            Interval _9122 = jet_mul_derivative(_4836, _4837, intervalFailed, optical_product_upper);
            Interval _4838 = _9122;
            Interval _9123 = jet_add_derivative(_4835, _4838, intervalFailed);
            Interval _4819 = _9101;
            Interval _4820 = Interval{ as_type<float>(as_type<uint>(_9113.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9113.lo) ^ 2147483648u) };
            Interval _9149 = iadd(_4819, _4820, intervalFailed);
            Interval _4821 = _9106;
            Interval _4822 = Interval{ as_type<float>(as_type<uint>(_9118.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9118.lo) ^ 2147483648u) };
            Interval _9151 = jet_add_derivative(_4821, _4822, intervalFailed);
            Interval _4823 = _9111;
            Interval _4824 = Interval{ as_type<float>(as_type<uint>(_9123.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9123.lo) ^ 2147483648u) };
            Interval _9153 = jet_add_derivative(_4823, _4824, intervalFailed);
            float _9155 = float(_2234[_8936].y);
            float _4817 = _9155;
            float _4818 = 1000.0;
            Interval _9157 = iratio(_4817, _4818, intervalFailed, optical_product_upper, interval_divide_upper);
            bool _9162;
            if (!intervalFailed)
            {
                _9162 = intervalFailed;
            }
            else
            {
                _9162 = false;
            }
            bool _9167;
            if (_9162)
            {
                _9167 = jetFailureSite == 0u;
            }
            else
            {
                _9167 = false;
            }
            if (_9167)
            {
                jetFailureSite = 6u;
                jetFailureArguments = float4(_9155, _9155, 1000.0, 1000.0);
            }
            Interval _4803 = _9041;
            Interval _4804 = _9157;
            Interval _9171 = imul(_4803, _4804, intervalFailed, optical_product_upper);
            Interval _4805 = _9043;
            Interval _4806 = _9157;
            Interval _9172 = jet_mul_derivative(_4805, _4806, intervalFailed, optical_product_upper);
            Interval _4807 = _9172;
            Interval _4808 = _9041;
            Interval _4809 = Interval{ 0.0, 0.0 };
            Interval _9173 = jet_mul_derivative(_4808, _4809, intervalFailed, optical_product_upper);
            Interval _4810 = _9173;
            Interval _9174 = jet_add_derivative(_4807, _4810, intervalFailed);
            Interval _4811 = _9045;
            Interval _4812 = _9157;
            Interval _9175 = jet_mul_derivative(_4811, _4812, intervalFailed, optical_product_upper);
            Interval _4813 = _9175;
            Interval _4814 = _9041;
            Interval _4815 = Interval{ 0.0, 0.0 };
            Interval _9176 = jet_mul_derivative(_4814, _4815, intervalFailed, optical_product_upper);
            Interval _4816 = _9176;
            Interval _9177 = jet_add_derivative(_4813, _4816, intervalFailed);
            Interval _4789 = _9095;
            Interval _4790 = _9157;
            Interval _9178 = imul(_4789, _4790, intervalFailed, optical_product_upper);
            Interval _4791 = _9097;
            Interval _4792 = _9157;
            Interval _9179 = jet_mul_derivative(_4791, _4792, intervalFailed, optical_product_upper);
            Interval _4793 = _9179;
            Interval _4794 = _9095;
            Interval _4795 = Interval{ 0.0, 0.0 };
            Interval _9180 = jet_mul_derivative(_4794, _4795, intervalFailed, optical_product_upper);
            Interval _4796 = _9180;
            Interval _9181 = jet_add_derivative(_4793, _4796, intervalFailed);
            Interval _4797 = _9099;
            Interval _4798 = _9157;
            Interval _9182 = jet_mul_derivative(_4797, _4798, intervalFailed, optical_product_upper);
            Interval _4799 = _9182;
            Interval _4800 = _9095;
            Interval _4801 = Interval{ 0.0, 0.0 };
            Interval _9183 = jet_mul_derivative(_4800, _4801, intervalFailed, optical_product_upper);
            Interval _4802 = _9183;
            Interval _9184 = jet_add_derivative(_4799, _4802, intervalFailed);
            Interval _4775 = _9149;
            Interval _4776 = _9157;
            Interval _9185 = imul(_4775, _4776, intervalFailed, optical_product_upper);
            Interval _4777 = _9151;
            Interval _4778 = _9157;
            Interval _9186 = jet_mul_derivative(_4777, _4778, intervalFailed, optical_product_upper);
            Interval _4779 = _9186;
            Interval _4780 = _9149;
            Interval _4781 = Interval{ 0.0, 0.0 };
            Interval _9187 = jet_mul_derivative(_4780, _4781, intervalFailed, optical_product_upper);
            Interval _4782 = _9187;
            Interval _9188 = jet_add_derivative(_4779, _4782, intervalFailed);
            Interval _4783 = _9153;
            Interval _4784 = _9157;
            Interval _9189 = jet_mul_derivative(_4783, _4784, intervalFailed, optical_product_upper);
            Interval _4785 = _9189;
            Interval _4786 = _9149;
            Interval _4787 = Interval{ 0.0, 0.0 };
            Interval _9190 = jet_mul_derivative(_4786, _4787, intervalFailed, optical_product_upper);
            Interval _4788 = _9190;
            Interval _9191 = jet_add_derivative(_4785, _4788, intervalFailed);
            Interval _4769 = _8957;
            Interval _4770 = _9171;
            Interval _9192 = iadd(_4769, _4770, intervalFailed);
            Interval _4771 = _8962;
            Interval _4772 = _9174;
            Interval _9193 = jet_add_derivative(_4771, _4772, intervalFailed);
            Interval _4773 = _8967;
            Interval _4774 = _9177;
            Interval _9194 = jet_add_derivative(_4773, _4774, intervalFailed);
            Interval _4763 = _8969;
            Interval _4764 = _9178;
            Interval _9195 = iadd(_4763, _4764, intervalFailed);
            Interval _4765 = _8974;
            Interval _4766 = _9181;
            Interval _9196 = jet_add_derivative(_4765, _4766, intervalFailed);
            Interval _4767 = _8979;
            Interval _4768 = _9184;
            Interval _9197 = jet_add_derivative(_4767, _4768, intervalFailed);
            Interval _4757 = _8981;
            Interval _4758 = _9185;
            Interval _9198 = iadd(_4757, _4758, intervalFailed);
            Interval _4759 = _8986;
            Interval _4760 = _9188;
            Interval _9199 = jet_add_derivative(_4759, _4760, intervalFailed);
            Interval _4761 = _8991;
            Interval _4762 = _9191;
            Interval _9200 = jet_add_derivative(_4761, _4762, intervalFailed);
            bool _9208;
            if (receiver.settings.x <= 0.0)
            {
                _9208 = receiver.settings.x >= 0.0;
            }
            else
            {
                _9208 = false;
            }
            float _9215;
            if (_9208)
            {
                _9215 = 0.0;
            }
            else
            {
                _9215 = precise::min(abs(receiver.settings.x), abs(receiver.settings.x));
            }
            float _9218 = precise::max(abs(receiver.settings.x), abs(receiver.settings.x));
            float _4747 = spvFMul(_9215, _9215);
            float _9219 = interval_down(_4747, intervalFailed);
            float _9220 = precise::max(0.0, _9219);
            float _4748 = spvFMul(_9218, _9218);
            float _9221 = interval_up(_4748, intervalFailed);
            Interval _4749 = Interval{ 2.0, 2.0 };
            Interval _4750 = Interval{ receiver.settings.x, receiver.settings.x };
            Interval _9223 = imul(_4749, _4750, intervalFailed, optical_product_upper);
            Interval _4751 = _9223;
            Interval _4752 = Interval{ 0.0, 0.0 };
            Interval _9224 = jet_mul_derivative(_4751, _4752, intervalFailed, optical_product_upper);
            Interval _4753 = Interval{ 2.0, 2.0 };
            Interval _4754 = Interval{ receiver.settings.x, receiver.settings.x };
            Interval _9226 = imul(_4753, _4754, intervalFailed, optical_product_upper);
            Interval _4755 = _9226;
            Interval _4756 = Interval{ 0.0, 0.0 };
            Interval _9227 = jet_mul_derivative(_4755, _4756, intervalFailed, optical_product_upper);
            Interval _4733 = _9192;
            Interval _4734 = Interval{ _9220, _9221 };
            Interval _9229 = imul(_4733, _4734, intervalFailed, optical_product_upper);
            Interval _4735 = _9193;
            Interval _4736 = Interval{ _9220, _9221 };
            Interval _9231 = jet_mul_derivative(_4735, _4736, intervalFailed, optical_product_upper);
            Interval _4737 = _9231;
            Interval _4738 = _9192;
            Interval _4739 = _9224;
            Interval _9232 = jet_mul_derivative(_4738, _4739, intervalFailed, optical_product_upper);
            Interval _4740 = _9232;
            Interval _9233 = jet_add_derivative(_4737, _4740, intervalFailed);
            Interval _4741 = _9194;
            Interval _4742 = Interval{ _9220, _9221 };
            Interval _9235 = jet_mul_derivative(_4741, _4742, intervalFailed, optical_product_upper);
            Interval _4743 = _9235;
            Interval _4744 = _9192;
            Interval _4745 = _9227;
            Interval _9236 = jet_mul_derivative(_4744, _4745, intervalFailed, optical_product_upper);
            Interval _4746 = _9236;
            Interval _9237 = jet_add_derivative(_4743, _4746, intervalFailed);
            Interval _4719 = _9195;
            Interval _4720 = Interval{ _9220, _9221 };
            Interval _9239 = imul(_4719, _4720, intervalFailed, optical_product_upper);
            Interval _4721 = _9196;
            Interval _4722 = Interval{ _9220, _9221 };
            Interval _9241 = jet_mul_derivative(_4721, _4722, intervalFailed, optical_product_upper);
            Interval _4723 = _9241;
            Interval _4724 = _9195;
            Interval _4725 = _9224;
            Interval _9242 = jet_mul_derivative(_4724, _4725, intervalFailed, optical_product_upper);
            Interval _4726 = _9242;
            Interval _9243 = jet_add_derivative(_4723, _4726, intervalFailed);
            Interval _4727 = _9197;
            Interval _4728 = Interval{ _9220, _9221 };
            Interval _9245 = jet_mul_derivative(_4727, _4728, intervalFailed, optical_product_upper);
            Interval _4729 = _9245;
            Interval _4730 = _9195;
            Interval _4731 = _9227;
            Interval _9246 = jet_mul_derivative(_4730, _4731, intervalFailed, optical_product_upper);
            Interval _4732 = _9246;
            Interval _9247 = jet_add_derivative(_4729, _4732, intervalFailed);
            Interval _4705 = _9198;
            Interval _4706 = Interval{ _9220, _9221 };
            Interval _9249 = imul(_4705, _4706, intervalFailed, optical_product_upper);
            Interval _4707 = _9199;
            Interval _4708 = Interval{ _9220, _9221 };
            Interval _9251 = jet_mul_derivative(_4707, _4708, intervalFailed, optical_product_upper);
            Interval _4709 = _9251;
            Interval _4710 = _9198;
            Interval _4711 = _9224;
            Interval _9252 = jet_mul_derivative(_4710, _4711, intervalFailed, optical_product_upper);
            Interval _4712 = _9252;
            Interval _9253 = jet_add_derivative(_4709, _4712, intervalFailed);
            Interval _4713 = _9200;
            Interval _4714 = Interval{ _9220, _9221 };
            Interval _9255 = jet_mul_derivative(_4713, _4714, intervalFailed, optical_product_upper);
            Interval _4715 = _9255;
            Interval _4716 = _9198;
            Interval _4717 = _9227;
            Interval _9256 = jet_mul_derivative(_4716, _4717, intervalFailed, optical_product_upper);
            Interval _4718 = _9256;
            Interval _9257 = jet_add_derivative(_4715, _4718, intervalFailed);
            Interval _4699 = _8174;
            Interval _4700 = _9229;
            Interval _9258 = iadd(_4699, _4700, intervalFailed);
            Interval _4701 = _8177;
            Interval _4702 = _9233;
            Interval _9259 = jet_add_derivative(_4701, _4702, intervalFailed);
            Interval _4703 = _8180;
            Interval _4704 = _9237;
            Interval _9260 = jet_add_derivative(_4703, _4704, intervalFailed);
            Interval _4693 = _8183;
            Interval _4694 = _9239;
            Interval _9261 = iadd(_4693, _4694, intervalFailed);
            Interval _4695 = _8186;
            Interval _4696 = _9243;
            Interval _9262 = jet_add_derivative(_4695, _4696, intervalFailed);
            Interval _4697 = _8189;
            Interval _4698 = _9247;
            Interval _9263 = jet_add_derivative(_4697, _4698, intervalFailed);
            Interval _4687 = _8192;
            Interval _4688 = _9249;
            Interval _9264 = iadd(_4687, _4688, intervalFailed);
            Interval _4689 = _8195;
            Interval _4690 = _9253;
            Interval _9265 = jet_add_derivative(_4689, _4690, intervalFailed);
            Interval _4691 = _8198;
            Interval _4692 = _9257;
            Interval _9266 = jet_add_derivative(_4691, _4692, intervalFailed);
            bool _9273;
            if (_9258.lo <= 0.0)
            {
                _9273 = _9258.hi >= 0.0;
            }
            else
            {
                _9273 = false;
            }
            float _9280;
            if (_9273)
            {
                _9280 = 0.0;
            }
            else
            {
                _9280 = precise::min(abs(_9258.lo), abs(_9258.hi));
            }
            float _9283 = precise::max(abs(_9258.lo), abs(_9258.hi));
            float _4677 = spvFMul(_9280, _9280);
            float _9284 = interval_down(_4677, intervalFailed);
            float _4678 = spvFMul(_9283, _9283);
            float _9286 = interval_up(_4678, intervalFailed);
            Interval _4679 = Interval{ 2.0, 2.0 };
            Interval _4680 = _9258;
            Interval _9287 = imul(_4679, _4680, intervalFailed, optical_product_upper);
            Interval _4681 = _9287;
            Interval _4682 = _9259;
            Interval _9288 = jet_mul_derivative(_4681, _4682, intervalFailed, optical_product_upper);
            Interval _4683 = Interval{ 2.0, 2.0 };
            Interval _4684 = _9258;
            Interval _9289 = imul(_4683, _4684, intervalFailed, optical_product_upper);
            Interval _4685 = _9289;
            Interval _4686 = _9260;
            Interval _9290 = jet_mul_derivative(_4685, _4686, intervalFailed, optical_product_upper);
            bool _9297;
            if (_9261.lo <= 0.0)
            {
                _9297 = _9261.hi >= 0.0;
            }
            else
            {
                _9297 = false;
            }
            float _9304;
            if (_9297)
            {
                _9304 = 0.0;
            }
            else
            {
                _9304 = precise::min(abs(_9261.lo), abs(_9261.hi));
            }
            float _9307 = precise::max(abs(_9261.lo), abs(_9261.hi));
            float _4667 = spvFMul(_9304, _9304);
            float _9308 = interval_down(_4667, intervalFailed);
            float _4668 = spvFMul(_9307, _9307);
            float _9310 = interval_up(_4668, intervalFailed);
            Interval _4669 = Interval{ 2.0, 2.0 };
            Interval _4670 = _9261;
            Interval _9311 = imul(_4669, _4670, intervalFailed, optical_product_upper);
            Interval _4671 = _9311;
            Interval _4672 = _9262;
            Interval _9312 = jet_mul_derivative(_4671, _4672, intervalFailed, optical_product_upper);
            Interval _4673 = Interval{ 2.0, 2.0 };
            Interval _4674 = _9261;
            Interval _9313 = imul(_4673, _4674, intervalFailed, optical_product_upper);
            Interval _4675 = _9313;
            Interval _4676 = _9263;
            Interval _9314 = jet_mul_derivative(_4675, _4676, intervalFailed, optical_product_upper);
            Interval _4661 = Interval{ precise::max(0.0, _9284), _9286 };
            Interval _4662 = Interval{ precise::max(0.0, _9308), _9310 };
            Interval _9317 = iadd(_4661, _4662, intervalFailed);
            Interval _4663 = _9288;
            Interval _4664 = _9312;
            Interval _9318 = jet_add_derivative(_4663, _4664, intervalFailed);
            Interval _4665 = _9290;
            Interval _4666 = _9314;
            Interval _9319 = jet_add_derivative(_4665, _4666, intervalFailed);
            bool _9326;
            if (_9264.lo <= 0.0)
            {
                _9326 = _9264.hi >= 0.0;
            }
            else
            {
                _9326 = false;
            }
            float _9333;
            if (_9326)
            {
                _9333 = 0.0;
            }
            else
            {
                _9333 = precise::min(abs(_9264.lo), abs(_9264.hi));
            }
            float _9336 = precise::max(abs(_9264.lo), abs(_9264.hi));
            float _4651 = spvFMul(_9333, _9333);
            float _9337 = interval_down(_4651, intervalFailed);
            float _4652 = spvFMul(_9336, _9336);
            float _9339 = interval_up(_4652, intervalFailed);
            Interval _4653 = Interval{ 2.0, 2.0 };
            Interval _4654 = _9264;
            Interval _9340 = imul(_4653, _4654, intervalFailed, optical_product_upper);
            Interval _4655 = _9340;
            Interval _4656 = _9265;
            Interval _9341 = jet_mul_derivative(_4655, _4656, intervalFailed, optical_product_upper);
            Interval _4657 = Interval{ 2.0, 2.0 };
            Interval _4658 = _9264;
            Interval _9342 = imul(_4657, _4658, intervalFailed, optical_product_upper);
            Interval _4659 = _9342;
            Interval _4660 = _9266;
            Interval _9343 = jet_mul_derivative(_4659, _4660, intervalFailed, optical_product_upper);
            Interval _4645 = _9317;
            Interval _4646 = Interval{ precise::max(0.0, _9337), _9339 };
            Interval _9345 = iadd(_4645, _4646, intervalFailed);
            Interval _4647 = _9318;
            Interval _4648 = _9341;
            Interval _9346 = jet_add_derivative(_4647, _4648, intervalFailed);
            Interval _4649 = _9319;
            Interval _4650 = _9343;
            Interval _9347 = jet_add_derivative(_4649, _4650, intervalFailed);
            Interval _4636 = _9345;
            Interval _9349 = isqrt(_4636, intervalFailed);
            bool _9357;
            if (!intervalFailed)
            {
                _9357 = intervalFailed;
            }
            else
            {
                _9357 = false;
            }
            bool _9362;
            if (_9357)
            {
                _9362 = jetFailureSite == 0u;
            }
            else
            {
                _9362 = false;
            }
            if (_9362)
            {
                jetFailureSite = 3u;
                jetFailureArguments = float4(_9345.lo, _9345.hi, 0.0, 0.0);
            }
            if (_9349.lo <= 0.0)
            {
                jetBranchKnown = false;
            }
            Interval _4637 = Interval{ 2.0, 2.0 };
            Interval _4638 = _9349;
            Interval _9369 = imul(_4637, _4638, intervalFailed, optical_product_upper);
            Interval _4639 = Interval{ 1.0, 1.0 };
            Interval _4640 = _9369;
            Interval _9371 = idiv(_4639, _4640, intervalFailed, interval_divide_upper);
            bool _9378;
            if (!intervalFailed)
            {
                _9378 = intervalFailed;
            }
            else
            {
                _9378 = false;
            }
            bool _9383;
            if (_9378)
            {
                _9383 = jetFailureSite == 0u;
            }
            else
            {
                _9383 = false;
            }
            if (_9383)
            {
                jetFailureSite = 4u;
                jetFailureArguments = float4(1.0, 1.0, _9369.lo, _9369.hi);
            }
            Interval _4641 = _9371;
            Interval _4642 = _9346;
            Interval _9387 = jet_mul_derivative(_4641, _4642, intervalFailed, optical_product_upper);
            Interval _4643 = _9371;
            Interval _4644 = _9347;
            Interval _9388 = jet_mul_derivative(_4643, _4644, intervalFailed, optical_product_upper);
            Interval _4622 = Interval{ 1.0, 1.0 };
            Interval _4623 = _9349;
            Interval _9390 = idiv(_4622, _4623, intervalFailed, interval_divide_upper);
            bool _9397;
            if (!intervalFailed)
            {
                _9397 = intervalFailed;
            }
            else
            {
                _9397 = false;
            }
            bool _9402;
            if (_9397)
            {
                _9402 = jetFailureSite == 0u;
            }
            else
            {
                _9402 = false;
            }
            if (_9402)
            {
                jetFailureSite = 1u;
                jetFailureArguments = float4(1.0, 1.0, _9349.lo, _9349.hi);
            }
            Interval _4624 = Interval{ 0.0, 0.0 };
            Interval _4625 = _9390;
            Interval _4626 = _9387;
            Interval _9406 = jet_mul_derivative(_4625, _4626, intervalFailed, optical_product_upper);
            Interval _4627 = Interval{ as_type<float>(as_type<uint>(_9406.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9406.lo) ^ 2147483648u) };
            Interval _9416 = jet_add_derivative(_4624, _4627, intervalFailed);
            Interval _4628 = _9416;
            Interval _4629 = _9349;
            Interval _9417 = jet_div_derivative(_4628, _4629, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _4630 = Interval{ 0.0, 0.0 };
            Interval _4631 = _9390;
            Interval _4632 = _9388;
            Interval _9418 = jet_mul_derivative(_4631, _4632, intervalFailed, optical_product_upper);
            Interval _4633 = Interval{ as_type<float>(as_type<uint>(_9418.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_9418.lo) ^ 2147483648u) };
            Interval _9428 = jet_add_derivative(_4630, _4633, intervalFailed);
            Interval _4634 = _9428;
            Interval _4635 = _9349;
            Interval _9429 = jet_div_derivative(_4634, _4635, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
            Interval _4608 = _9258;
            Interval _4609 = _9390;
            Interval _9430 = imul(_4608, _4609, intervalFailed, optical_product_upper);
            Interval _4610 = _9259;
            Interval _4611 = _9390;
            Interval _9431 = jet_mul_derivative(_4610, _4611, intervalFailed, optical_product_upper);
            Interval _4612 = _9431;
            Interval _4613 = _9258;
            Interval _4614 = _9417;
            Interval _9432 = jet_mul_derivative(_4613, _4614, intervalFailed, optical_product_upper);
            Interval _4615 = _9432;
            Interval _9433 = jet_add_derivative(_4612, _4615, intervalFailed);
            Interval _4616 = _9260;
            Interval _4617 = _9390;
            Interval _9434 = jet_mul_derivative(_4616, _4617, intervalFailed, optical_product_upper);
            Interval _4618 = _9434;
            Interval _4619 = _9258;
            Interval _4620 = _9429;
            Interval _9435 = jet_mul_derivative(_4619, _4620, intervalFailed, optical_product_upper);
            Interval _4621 = _9435;
            Interval _9436 = jet_add_derivative(_4618, _4621, intervalFailed);
            Interval _4594 = _9261;
            Interval _4595 = _9390;
            Interval _9437 = imul(_4594, _4595, intervalFailed, optical_product_upper);
            Interval _4596 = _9262;
            Interval _4597 = _9390;
            Interval _9438 = jet_mul_derivative(_4596, _4597, intervalFailed, optical_product_upper);
            Interval _4598 = _9438;
            Interval _4599 = _9261;
            Interval _4600 = _9417;
            Interval _9439 = jet_mul_derivative(_4599, _4600, intervalFailed, optical_product_upper);
            Interval _4601 = _9439;
            Interval _9440 = jet_add_derivative(_4598, _4601, intervalFailed);
            Interval _4602 = _9263;
            Interval _4603 = _9390;
            Interval _9441 = jet_mul_derivative(_4602, _4603, intervalFailed, optical_product_upper);
            Interval _4604 = _9441;
            Interval _4605 = _9261;
            Interval _4606 = _9429;
            Interval _9442 = jet_mul_derivative(_4605, _4606, intervalFailed, optical_product_upper);
            Interval _4607 = _9442;
            Interval _9443 = jet_add_derivative(_4604, _4607, intervalFailed);
            Interval _4580 = _9264;
            Interval _4581 = _9390;
            Interval _9444 = imul(_4580, _4581, intervalFailed, optical_product_upper);
            Interval _4582 = _9265;
            Interval _4583 = _9390;
            Interval _9445 = jet_mul_derivative(_4582, _4583, intervalFailed, optical_product_upper);
            Interval _4584 = _9445;
            Interval _4585 = _9264;
            Interval _4586 = _9417;
            Interval _9446 = jet_mul_derivative(_4585, _4586, intervalFailed, optical_product_upper);
            Interval _4587 = _9446;
            Interval _9447 = jet_add_derivative(_4584, _4587, intervalFailed);
            Interval _4588 = _9266;
            Interval _4589 = _9390;
            Interval _9448 = jet_mul_derivative(_4588, _4589, intervalFailed, optical_product_upper);
            Interval _4590 = _9448;
            Interval _4591 = _9264;
            Interval _4592 = _9429;
            Interval _9449 = jet_mul_derivative(_4591, _4592, intervalFailed, optical_product_upper);
            Interval _4593 = _9449;
            Interval _9450 = jet_add_derivative(_4590, _4593, intervalFailed);
            Interval _4566 = _9430;
            Interval _4567 = Interval{ _7850, _7851 };
            Interval _9452 = imul(_4566, _4567, intervalFailed, optical_product_upper);
            Interval _4568 = _9433;
            Interval _4569 = Interval{ _7850, _7851 };
            Interval _9454 = jet_mul_derivative(_4568, _4569, intervalFailed, optical_product_upper);
            Interval _4570 = _9454;
            Interval _4571 = _9430;
            Interval _4572 = Interval{ _7852, _7853 };
            Interval _9456 = jet_mul_derivative(_4571, _4572, intervalFailed, optical_product_upper);
            Interval _4573 = _9456;
            Interval _9457 = jet_add_derivative(_4570, _4573, intervalFailed);
            Interval _4574 = _9436;
            Interval _4575 = Interval{ _7850, _7851 };
            Interval _9459 = jet_mul_derivative(_4574, _4575, intervalFailed, optical_product_upper);
            Interval _4576 = _9459;
            Interval _4577 = _9430;
            Interval _4578 = Interval{ _7854, _7855 };
            Interval _9461 = jet_mul_derivative(_4577, _4578, intervalFailed, optical_product_upper);
            Interval _4579 = _9461;
            Interval _9462 = jet_add_derivative(_4576, _4579, intervalFailed);
            Interval _4552 = _9437;
            Interval _4553 = Interval{ _7856, _7857 };
            Interval _9464 = imul(_4552, _4553, intervalFailed, optical_product_upper);
            Interval _4554 = _9440;
            Interval _4555 = Interval{ _7856, _7857 };
            Interval _9466 = jet_mul_derivative(_4554, _4555, intervalFailed, optical_product_upper);
            Interval _4556 = _9466;
            Interval _4557 = _9437;
            Interval _4558 = Interval{ _7858, _7859 };
            Interval _9468 = jet_mul_derivative(_4557, _4558, intervalFailed, optical_product_upper);
            Interval _4559 = _9468;
            Interval _9469 = jet_add_derivative(_4556, _4559, intervalFailed);
            Interval _4560 = _9443;
            Interval _4561 = Interval{ _7856, _7857 };
            Interval _9471 = jet_mul_derivative(_4560, _4561, intervalFailed, optical_product_upper);
            Interval _4562 = _9471;
            Interval _4563 = _9437;
            Interval _4564 = Interval{ _7860, _7861 };
            Interval _9473 = jet_mul_derivative(_4563, _4564, intervalFailed, optical_product_upper);
            Interval _4565 = _9473;
            Interval _9474 = jet_add_derivative(_4562, _4565, intervalFailed);
            Interval _4546 = _9452;
            Interval _4547 = _9464;
            Interval _9475 = iadd(_4546, _4547, intervalFailed);
            Interval _4548 = _9457;
            Interval _4549 = _9469;
            Interval _9476 = jet_add_derivative(_4548, _4549, intervalFailed);
            Interval _4550 = _9462;
            Interval _4551 = _9474;
            Interval _9477 = jet_add_derivative(_4550, _4551, intervalFailed);
            Interval _4532 = _9444;
            Interval _4533 = Interval{ _7862, _7863 };
            Interval _9479 = imul(_4532, _4533, intervalFailed, optical_product_upper);
            Interval _4534 = _9447;
            Interval _4535 = Interval{ _7862, _7863 };
            Interval _9481 = jet_mul_derivative(_4534, _4535, intervalFailed, optical_product_upper);
            Interval _4536 = _9481;
            Interval _4537 = _9444;
            Interval _4538 = Interval{ _7864, _7865 };
            Interval _9483 = jet_mul_derivative(_4537, _4538, intervalFailed, optical_product_upper);
            Interval _4539 = _9483;
            Interval _9484 = jet_add_derivative(_4536, _4539, intervalFailed);
            Interval _4540 = _9450;
            Interval _4541 = Interval{ _7862, _7863 };
            Interval _9486 = jet_mul_derivative(_4540, _4541, intervalFailed, optical_product_upper);
            Interval _4542 = _9486;
            Interval _4543 = _9444;
            Interval _4544 = Interval{ _7866, _7867 };
            Interval _9488 = jet_mul_derivative(_4543, _4544, intervalFailed, optical_product_upper);
            Interval _4545 = _9488;
            Interval _9489 = jet_add_derivative(_4542, _4545, intervalFailed);
            Interval _4526 = _9475;
            Interval _4527 = _9479;
            Interval _9490 = iadd(_4526, _4527, intervalFailed);
            Interval _4528 = _9476;
            Interval _4529 = _9484;
            __attribute__((unused)) Interval _9491 = jet_add_derivative(_4528, _4529, intervalFailed);
            Interval _4530 = _9477;
            Interval _4531 = _9489;
            __attribute__((unused)) Interval _9492 = jet_add_derivative(_4530, _4531, intervalFailed);
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
            float _9527;
            float _9528;
            float _9529;
            float _9530;
            float _9531;
            float _9532;
            if (_9490.lo > 0.0)
            {
                _9515 = _9430.lo;
                _9516 = _9430.hi;
                _9517 = _9433.lo;
                _9518 = _9433.hi;
                _9519 = _9436.lo;
                _9520 = _9436.hi;
                _9521 = _9437.lo;
                _9522 = _9437.hi;
                _9523 = _9440.lo;
                _9524 = _9440.hi;
                _9525 = _9443.lo;
                _9526 = _9443.hi;
                _9527 = _9444.lo;
                _9528 = _9444.hi;
                _9529 = _9447.lo;
                _9530 = _9447.hi;
                _9531 = _9450.lo;
                _9532 = _9450.hi;
            }
            else
            {
                if (_9490.hi > 0.0)
                {
                    return false;
                }
                _9515 = _8174.lo;
                _9516 = _8174.hi;
                _9517 = _8177.lo;
                _9518 = _8177.hi;
                _9519 = _8180.lo;
                _9520 = _8180.hi;
                _9521 = _8183.lo;
                _9522 = _8183.hi;
                _9523 = _8186.lo;
                _9524 = _8186.hi;
                _9525 = _8189.lo;
                _9526 = _8189.hi;
                _9527 = _8192.lo;
                _9528 = _8192.hi;
                _9529 = _8195.lo;
                _9530 = _8195.hi;
                _9531 = _8198.lo;
                _9532 = _8198.hi;
            }
            _6691 = _9515;
            _6693 = _9516;
            _6695 = _9517;
            _6697 = _9518;
            _6699 = _9519;
            _6701 = _9520;
            _6703 = _9521;
            _6705 = _9522;
            _6707 = _9523;
            _6709 = _9524;
            _6711 = _9525;
            _6713 = _9526;
            _6715 = _9527;
            _6717 = _9528;
            _6719 = _9529;
            _6721 = _9530;
            _6723 = _9531;
            _6725 = _9532;
        }
        else
        {
            _6691 = _8174.lo;
            _6693 = _8174.hi;
            _6695 = _8177.lo;
            _6697 = _8177.hi;
            _6699 = _8180.lo;
            _6701 = _8180.hi;
            _6703 = _8183.lo;
            _6705 = _8183.hi;
            _6707 = _8186.lo;
            _6709 = _8186.hi;
            _6711 = _8189.lo;
            _6713 = _8189.hi;
            _6715 = _8192.lo;
            _6717 = _8192.hi;
            _6719 = _8195.lo;
            _6721 = _8195.hi;
            _6723 = _8198.lo;
            _6725 = _8198.hi;
        }
        bool _9537;
        if (!intervalFailed)
        {
            _9537 = !jetBranchKnown;
        }
        else
        {
            _9537 = true;
        }
        if (_9537)
        {
            return false;
        }
    }
    ray.origin = OpticalJet3{ OpticalJet{ Interval{ _6726, _6728 }, Interval{ _6730, _6732 }, Interval{ _6734, _6736 } }, OpticalJet{ Interval{ _6738, _6740 }, Interval{ _6742, _6744 }, Interval{ _6746, _6748 } }, OpticalJet{ Interval{ _6750, _6752 }, Interval{ _6754, _6756 }, Interval{ _6758, _6760 } } };
    ray.outgoing = OpticalJet3{ OpticalJet{ Interval{ _6690, _6692 }, Interval{ _6694, _6696 }, Interval{ _6698, _6700 } }, OpticalJet{ Interval{ _6702, _6704 }, Interval{ _6706, _6708 }, Interval{ _6710, _6712 } }, OpticalJet{ Interval{ _6714, _6716 }, Interval{ _6718, _6720 }, Interval{ _6722, _6724 } } };
    ray.bias0 = OpticalJet{ Interval{ _6678, _6680 }, Interval{ _6682, _6684 }, Interval{ _6686, _6688 } };
    bool _9574;
    if (!intervalFailed)
    {
        _9574 = jetBranchKnown;
    }
    else
    {
        _9574 = false;
    }
    return _9574;
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
    float _10096;
    float _10097;
    float _10098;
    float _10099;
    float _10100;
    float _10101;
    if (finiteTerminal)
    {
        Interval _9943 = target.x;
        Interval _9944 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.v.lo) ^ 2147483648u) };
        Interval _10049 = iadd(_9943, _9944, intervalFailed);
        Interval _9945 = Interval{ 0.0, 0.0 };
        Interval _9946 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.dx.lo) ^ 2147483648u) };
        Interval _10051 = jet_add_derivative(_9945, _9946, intervalFailed);
        Interval _9947 = Interval{ 0.0, 0.0 };
        Interval _9948 = Interval{ as_type<float>(as_type<uint>(ray.origin.x.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.x.dy.lo) ^ 2147483648u) };
        Interval _10053 = jet_add_derivative(_9947, _9948, intervalFailed);
        Interval _9937 = target.y;
        Interval _9938 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.v.lo) ^ 2147483648u) };
        Interval _10055 = iadd(_9937, _9938, intervalFailed);
        Interval _9939 = Interval{ 0.0, 0.0 };
        Interval _9940 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.dx.lo) ^ 2147483648u) };
        Interval _10057 = jet_add_derivative(_9939, _9940, intervalFailed);
        Interval _9941 = Interval{ 0.0, 0.0 };
        Interval _9942 = Interval{ as_type<float>(as_type<uint>(ray.origin.y.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.y.dy.lo) ^ 2147483648u) };
        Interval _10059 = jet_add_derivative(_9941, _9942, intervalFailed);
        Interval _9931 = target.z;
        Interval _9932 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.v.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.v.lo) ^ 2147483648u) };
        Interval _10061 = iadd(_9931, _9932, intervalFailed);
        Interval _9933 = Interval{ 0.0, 0.0 };
        Interval _9934 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.dx.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.dx.lo) ^ 2147483648u) };
        Interval _10063 = jet_add_derivative(_9933, _9934, intervalFailed);
        Interval _9935 = Interval{ 0.0, 0.0 };
        Interval _9936 = Interval{ as_type<float>(as_type<uint>(ray.origin.z.dy.hi) ^ 2147483648u), as_type<float>(as_type<uint>(ray.origin.z.dy.lo) ^ 2147483648u) };
        Interval _10065 = jet_add_derivative(_9935, _9936, intervalFailed);
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
        _10096 = _10061.lo;
        _10097 = _10061.hi;
        _10098 = _10063.lo;
        _10099 = _10063.hi;
        _10100 = _10065.lo;
        _10101 = _10065.hi;
    }
    else
    {
        _10084 = target.x.lo;
        _10085 = target.x.hi;
        _10086 = 0.0;
        _10087 = 0.0;
        _10088 = 0.0;
        _10089 = 0.0;
        _10090 = target.y.lo;
        _10091 = target.y.hi;
        _10092 = 0.0;
        _10093 = 0.0;
        _10094 = 0.0;
        _10095 = 0.0;
        _10096 = target.z.lo;
        _10097 = target.z.hi;
        _10098 = 0.0;
        _10099 = 0.0;
        _10100 = 0.0;
        _10101 = 0.0;
    }
    bool _10106;
    if (_10084 <= 0.0)
    {
        _10106 = _10085 >= 0.0;
    }
    else
    {
        _10106 = false;
    }
    float _10113;
    if (_10106)
    {
        _10113 = 0.0;
    }
    else
    {
        _10113 = precise::min(abs(_10084), abs(_10085));
    }
    float _10116 = precise::max(abs(_10084), abs(_10085));
    float _9921 = spvFMul(_10113, _10113);
    float _10117 = interval_down(_9921, intervalFailed);
    float _9922 = spvFMul(_10116, _10116);
    float _10119 = interval_up(_9922, intervalFailed);
    Interval _9923 = Interval{ 2.0, 2.0 };
    Interval _9924 = Interval{ _10084, _10085 };
    Interval _10121 = imul(_9923, _9924, intervalFailed, optical_product_upper);
    Interval _9925 = _10121;
    Interval _9926 = Interval{ _10086, _10087 };
    Interval _10123 = jet_mul_derivative(_9925, _9926, intervalFailed, optical_product_upper);
    Interval _9927 = Interval{ 2.0, 2.0 };
    Interval _9928 = Interval{ _10084, _10085 };
    Interval _10125 = imul(_9927, _9928, intervalFailed, optical_product_upper);
    Interval _9929 = _10125;
    Interval _9930 = Interval{ _10088, _10089 };
    Interval _10127 = jet_mul_derivative(_9929, _9930, intervalFailed, optical_product_upper);
    bool _10132;
    if (_10090 <= 0.0)
    {
        _10132 = _10091 >= 0.0;
    }
    else
    {
        _10132 = false;
    }
    float _10139;
    if (_10132)
    {
        _10139 = 0.0;
    }
    else
    {
        _10139 = precise::min(abs(_10090), abs(_10091));
    }
    float _10142 = precise::max(abs(_10090), abs(_10091));
    float _9911 = spvFMul(_10139, _10139);
    float _10143 = interval_down(_9911, intervalFailed);
    float _9912 = spvFMul(_10142, _10142);
    float _10145 = interval_up(_9912, intervalFailed);
    Interval _9913 = Interval{ 2.0, 2.0 };
    Interval _9914 = Interval{ _10090, _10091 };
    Interval _10147 = imul(_9913, _9914, intervalFailed, optical_product_upper);
    Interval _9915 = _10147;
    Interval _9916 = Interval{ _10092, _10093 };
    Interval _10149 = jet_mul_derivative(_9915, _9916, intervalFailed, optical_product_upper);
    Interval _9917 = Interval{ 2.0, 2.0 };
    Interval _9918 = Interval{ _10090, _10091 };
    Interval _10151 = imul(_9917, _9918, intervalFailed, optical_product_upper);
    Interval _9919 = _10151;
    Interval _9920 = Interval{ _10094, _10095 };
    Interval _10153 = jet_mul_derivative(_9919, _9920, intervalFailed, optical_product_upper);
    Interval _9905 = Interval{ precise::max(0.0, _10117), _10119 };
    Interval _9906 = Interval{ precise::max(0.0, _10143), _10145 };
    Interval _10156 = iadd(_9905, _9906, intervalFailed);
    Interval _9907 = _10123;
    Interval _9908 = _10149;
    Interval _10157 = jet_add_derivative(_9907, _9908, intervalFailed);
    Interval _9909 = _10127;
    Interval _9910 = _10153;
    Interval _10158 = jet_add_derivative(_9909, _9910, intervalFailed);
    bool _10163;
    if (_10096 <= 0.0)
    {
        _10163 = _10097 >= 0.0;
    }
    else
    {
        _10163 = false;
    }
    float _10170;
    if (_10163)
    {
        _10170 = 0.0;
    }
    else
    {
        _10170 = precise::min(abs(_10096), abs(_10097));
    }
    float _10173 = precise::max(abs(_10096), abs(_10097));
    float _9895 = spvFMul(_10170, _10170);
    float _10174 = interval_down(_9895, intervalFailed);
    float _9896 = spvFMul(_10173, _10173);
    float _10176 = interval_up(_9896, intervalFailed);
    Interval _9897 = Interval{ 2.0, 2.0 };
    Interval _9898 = Interval{ _10096, _10097 };
    Interval _10178 = imul(_9897, _9898, intervalFailed, optical_product_upper);
    Interval _9899 = _10178;
    Interval _9900 = Interval{ _10098, _10099 };
    Interval _10180 = jet_mul_derivative(_9899, _9900, intervalFailed, optical_product_upper);
    Interval _9901 = Interval{ 2.0, 2.0 };
    Interval _9902 = Interval{ _10096, _10097 };
    Interval _10182 = imul(_9901, _9902, intervalFailed, optical_product_upper);
    Interval _9903 = _10182;
    Interval _9904 = Interval{ _10100, _10101 };
    Interval _10184 = jet_mul_derivative(_9903, _9904, intervalFailed, optical_product_upper);
    Interval _9889 = _10156;
    Interval _9890 = Interval{ precise::max(0.0, _10174), _10176 };
    Interval _10186 = iadd(_9889, _9890, intervalFailed);
    Interval _9891 = _10157;
    Interval _9892 = _10180;
    Interval _10187 = jet_add_derivative(_9891, _9892, intervalFailed);
    Interval _9893 = _10158;
    Interval _9894 = _10184;
    Interval _10188 = jet_add_derivative(_9893, _9894, intervalFailed);
    Interval _9880 = _10186;
    Interval _10190 = isqrt(_9880, intervalFailed);
    bool _10198;
    if (!intervalFailed)
    {
        _10198 = intervalFailed;
    }
    else
    {
        _10198 = false;
    }
    bool _10203;
    if (_10198)
    {
        _10203 = jetFailureSite == 0u;
    }
    else
    {
        _10203 = false;
    }
    if (_10203)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_10186.lo, _10186.hi, 0.0, 0.0);
    }
    if (_10190.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _9881 = Interval{ 2.0, 2.0 };
    Interval _9882 = _10190;
    Interval _10210 = imul(_9881, _9882, intervalFailed, optical_product_upper);
    Interval _9883 = Interval{ 1.0, 1.0 };
    Interval _9884 = _10210;
    Interval _10212 = idiv(_9883, _9884, intervalFailed, interval_divide_upper);
    bool _10219;
    if (!intervalFailed)
    {
        _10219 = intervalFailed;
    }
    else
    {
        _10219 = false;
    }
    bool _10224;
    if (_10219)
    {
        _10224 = jetFailureSite == 0u;
    }
    else
    {
        _10224 = false;
    }
    if (_10224)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _10210.lo, _10210.hi);
    }
    Interval _9885 = _10212;
    Interval _9886 = _10187;
    __attribute__((unused)) Interval _10228 = jet_mul_derivative(_9885, _9886, intervalFailed, optical_product_upper);
    Interval _9887 = _10212;
    Interval _9888 = _10188;
    __attribute__((unused)) Interval _10229 = jet_mul_derivative(_9887, _9888, intervalFailed, optical_product_upper);
    float _10234;
    if (finiteTerminal)
    {
        _10234 = ray.bias0.v.hi;
    }
    else
    {
        _10234 = 0.0;
    }
    bool _10294;
    if ((isunordered(_10190.lo, _10234) || _10190.lo > _10234))
    {
        Interval _9866 = Interval{ _10084, _10085 };
        Interval _9867 = ray.outgoing.x.v;
        Interval _10245 = imul(_9866, _9867, intervalFailed, optical_product_upper);
        Interval _9868 = Interval{ _10086, _10087 };
        Interval _9869 = ray.outgoing.x.v;
        Interval _10247 = jet_mul_derivative(_9868, _9869, intervalFailed, optical_product_upper);
        Interval _9870 = _10247;
        Interval _9871 = Interval{ _10084, _10085 };
        Interval _9872 = ray.outgoing.x.dx;
        Interval _10249 = jet_mul_derivative(_9871, _9872, intervalFailed, optical_product_upper);
        Interval _9873 = _10249;
        Interval _10250 = jet_add_derivative(_9870, _9873, intervalFailed);
        Interval _9874 = Interval{ _10088, _10089 };
        Interval _9875 = ray.outgoing.x.v;
        Interval _10252 = jet_mul_derivative(_9874, _9875, intervalFailed, optical_product_upper);
        Interval _9876 = _10252;
        Interval _9877 = Interval{ _10084, _10085 };
        Interval _9878 = ray.outgoing.x.dy;
        Interval _10254 = jet_mul_derivative(_9877, _9878, intervalFailed, optical_product_upper);
        Interval _9879 = _10254;
        Interval _10255 = jet_add_derivative(_9876, _9879, intervalFailed);
        Interval _9852 = Interval{ _10090, _10091 };
        Interval _9853 = ray.outgoing.y.v;
        Interval _10260 = imul(_9852, _9853, intervalFailed, optical_product_upper);
        Interval _9854 = Interval{ _10092, _10093 };
        Interval _9855 = ray.outgoing.y.v;
        Interval _10262 = jet_mul_derivative(_9854, _9855, intervalFailed, optical_product_upper);
        Interval _9856 = _10262;
        Interval _9857 = Interval{ _10090, _10091 };
        Interval _9858 = ray.outgoing.y.dx;
        Interval _10264 = jet_mul_derivative(_9857, _9858, intervalFailed, optical_product_upper);
        Interval _9859 = _10264;
        Interval _10265 = jet_add_derivative(_9856, _9859, intervalFailed);
        Interval _9860 = Interval{ _10094, _10095 };
        Interval _9861 = ray.outgoing.y.v;
        Interval _10267 = jet_mul_derivative(_9860, _9861, intervalFailed, optical_product_upper);
        Interval _9862 = _10267;
        Interval _9863 = Interval{ _10090, _10091 };
        Interval _9864 = ray.outgoing.y.dy;
        Interval _10269 = jet_mul_derivative(_9863, _9864, intervalFailed, optical_product_upper);
        Interval _9865 = _10269;
        Interval _10270 = jet_add_derivative(_9862, _9865, intervalFailed);
        Interval _9846 = _10245;
        Interval _9847 = _10260;
        Interval _10271 = iadd(_9846, _9847, intervalFailed);
        Interval _9848 = _10250;
        Interval _9849 = _10265;
        Interval _10272 = jet_add_derivative(_9848, _9849, intervalFailed);
        Interval _9850 = _10255;
        Interval _9851 = _10270;
        Interval _10273 = jet_add_derivative(_9850, _9851, intervalFailed);
        Interval _9832 = Interval{ _10096, _10097 };
        Interval _9833 = ray.outgoing.z.v;
        Interval _10278 = imul(_9832, _9833, intervalFailed, optical_product_upper);
        Interval _9834 = Interval{ _10098, _10099 };
        Interval _9835 = ray.outgoing.z.v;
        Interval _10280 = jet_mul_derivative(_9834, _9835, intervalFailed, optical_product_upper);
        Interval _9836 = _10280;
        Interval _9837 = Interval{ _10096, _10097 };
        Interval _9838 = ray.outgoing.z.dx;
        Interval _10282 = jet_mul_derivative(_9837, _9838, intervalFailed, optical_product_upper);
        Interval _9839 = _10282;
        Interval _10283 = jet_add_derivative(_9836, _9839, intervalFailed);
        Interval _9840 = Interval{ _10100, _10101 };
        Interval _9841 = ray.outgoing.z.v;
        Interval _10285 = jet_mul_derivative(_9840, _9841, intervalFailed, optical_product_upper);
        Interval _9842 = _10285;
        Interval _9843 = Interval{ _10096, _10097 };
        Interval _9844 = ray.outgoing.z.dy;
        Interval _10287 = jet_mul_derivative(_9843, _9844, intervalFailed, optical_product_upper);
        Interval _9845 = _10287;
        Interval _10288 = jet_add_derivative(_9842, _9845, intervalFailed);
        Interval _9826 = _10271;
        Interval _9827 = _10278;
        Interval _10289 = iadd(_9826, _9827, intervalFailed);
        Interval _9828 = _10272;
        Interval _9829 = _10283;
        __attribute__((unused)) Interval _10290 = jet_add_derivative(_9828, _9829, intervalFailed);
        Interval _9830 = _10273;
        Interval _9831 = _10288;
        __attribute__((unused)) Interval _10291 = jet_add_derivative(_9830, _9831, intervalFailed);
        _10294 = _10289.lo <= 0.0;
    }
    else
    {
        _10294 = true;
    }
    if (_10294)
    {
        return false;
    }
    float _10313;
    float _10314;
    if (omitted == 0u)
    {
        _10313 = ray.outgoing.x.v.lo;
        _10314 = ray.outgoing.x.v.hi;
    }
    else
    {
        float _10311;
        float _10312;
        if (omitted == 1u)
        {
            _10311 = ray.outgoing.y.v.lo;
            _10312 = ray.outgoing.y.v.hi;
        }
        else
        {
            _10311 = ray.outgoing.z.v.lo;
            _10312 = ray.outgoing.z.v.hi;
        }
        _10313 = _10311;
        _10314 = _10312;
    }
    bool _10319;
    if (_10313 <= 0.0)
    {
        _10319 = _10314 >= 0.0;
    }
    else
    {
        _10319 = false;
    }
    if (_10319)
    {
        return false;
    }
    bool _10324;
    if (_10084 <= 0.0)
    {
        _10324 = _10085 >= 0.0;
    }
    else
    {
        _10324 = false;
    }
    float _10331;
    if (_10324)
    {
        _10331 = 0.0;
    }
    else
    {
        _10331 = precise::min(abs(_10084), abs(_10085));
    }
    float _10334 = precise::max(abs(_10084), abs(_10085));
    float _9816 = spvFMul(_10331, _10331);
    float _10335 = interval_down(_9816, intervalFailed);
    float _9817 = spvFMul(_10334, _10334);
    float _10337 = interval_up(_9817, intervalFailed);
    Interval _9818 = Interval{ 2.0, 2.0 };
    Interval _9819 = Interval{ _10084, _10085 };
    Interval _10339 = imul(_9818, _9819, intervalFailed, optical_product_upper);
    Interval _9820 = _10339;
    Interval _9821 = Interval{ _10086, _10087 };
    Interval _10341 = jet_mul_derivative(_9820, _9821, intervalFailed, optical_product_upper);
    Interval _9822 = Interval{ 2.0, 2.0 };
    Interval _9823 = Interval{ _10084, _10085 };
    Interval _10343 = imul(_9822, _9823, intervalFailed, optical_product_upper);
    Interval _9824 = _10343;
    Interval _9825 = Interval{ _10088, _10089 };
    Interval _10345 = jet_mul_derivative(_9824, _9825, intervalFailed, optical_product_upper);
    bool _10350;
    if (_10090 <= 0.0)
    {
        _10350 = _10091 >= 0.0;
    }
    else
    {
        _10350 = false;
    }
    float _10357;
    if (_10350)
    {
        _10357 = 0.0;
    }
    else
    {
        _10357 = precise::min(abs(_10090), abs(_10091));
    }
    float _10360 = precise::max(abs(_10090), abs(_10091));
    float _9806 = spvFMul(_10357, _10357);
    float _10361 = interval_down(_9806, intervalFailed);
    float _9807 = spvFMul(_10360, _10360);
    float _10363 = interval_up(_9807, intervalFailed);
    Interval _9808 = Interval{ 2.0, 2.0 };
    Interval _9809 = Interval{ _10090, _10091 };
    Interval _10365 = imul(_9808, _9809, intervalFailed, optical_product_upper);
    Interval _9810 = _10365;
    Interval _9811 = Interval{ _10092, _10093 };
    Interval _10367 = jet_mul_derivative(_9810, _9811, intervalFailed, optical_product_upper);
    Interval _9812 = Interval{ 2.0, 2.0 };
    Interval _9813 = Interval{ _10090, _10091 };
    Interval _10369 = imul(_9812, _9813, intervalFailed, optical_product_upper);
    Interval _9814 = _10369;
    Interval _9815 = Interval{ _10094, _10095 };
    Interval _10371 = jet_mul_derivative(_9814, _9815, intervalFailed, optical_product_upper);
    Interval _9800 = Interval{ precise::max(0.0, _10335), _10337 };
    Interval _9801 = Interval{ precise::max(0.0, _10361), _10363 };
    Interval _10374 = iadd(_9800, _9801, intervalFailed);
    Interval _9802 = _10341;
    Interval _9803 = _10367;
    Interval _10375 = jet_add_derivative(_9802, _9803, intervalFailed);
    Interval _9804 = _10345;
    Interval _9805 = _10371;
    Interval _10376 = jet_add_derivative(_9804, _9805, intervalFailed);
    bool _10381;
    if (_10096 <= 0.0)
    {
        _10381 = _10097 >= 0.0;
    }
    else
    {
        _10381 = false;
    }
    float _10388;
    if (_10381)
    {
        _10388 = 0.0;
    }
    else
    {
        _10388 = precise::min(abs(_10096), abs(_10097));
    }
    float _10391 = precise::max(abs(_10096), abs(_10097));
    float _9790 = spvFMul(_10388, _10388);
    float _10392 = interval_down(_9790, intervalFailed);
    float _9791 = spvFMul(_10391, _10391);
    float _10394 = interval_up(_9791, intervalFailed);
    Interval _9792 = Interval{ 2.0, 2.0 };
    Interval _9793 = Interval{ _10096, _10097 };
    Interval _10396 = imul(_9792, _9793, intervalFailed, optical_product_upper);
    Interval _9794 = _10396;
    Interval _9795 = Interval{ _10098, _10099 };
    Interval _10398 = jet_mul_derivative(_9794, _9795, intervalFailed, optical_product_upper);
    Interval _9796 = Interval{ 2.0, 2.0 };
    Interval _9797 = Interval{ _10096, _10097 };
    Interval _10400 = imul(_9796, _9797, intervalFailed, optical_product_upper);
    Interval _9798 = _10400;
    Interval _9799 = Interval{ _10100, _10101 };
    Interval _10402 = jet_mul_derivative(_9798, _9799, intervalFailed, optical_product_upper);
    Interval _9784 = _10374;
    Interval _9785 = Interval{ precise::max(0.0, _10392), _10394 };
    Interval _10404 = iadd(_9784, _9785, intervalFailed);
    Interval _9786 = _10375;
    Interval _9787 = _10398;
    Interval _10405 = jet_add_derivative(_9786, _9787, intervalFailed);
    Interval _9788 = _10376;
    Interval _9789 = _10402;
    Interval _10406 = jet_add_derivative(_9788, _9789, intervalFailed);
    Interval _9775 = _10404;
    Interval _10408 = isqrt(_9775, intervalFailed);
    bool _10416;
    if (!intervalFailed)
    {
        _10416 = intervalFailed;
    }
    else
    {
        _10416 = false;
    }
    bool _10421;
    if (_10416)
    {
        _10421 = jetFailureSite == 0u;
    }
    else
    {
        _10421 = false;
    }
    if (_10421)
    {
        jetFailureSite = 3u;
        jetFailureArguments = float4(_10404.lo, _10404.hi, 0.0, 0.0);
    }
    if (_10408.lo <= 0.0)
    {
        jetBranchKnown = false;
    }
    Interval _9776 = Interval{ 2.0, 2.0 };
    Interval _9777 = _10408;
    Interval _10428 = imul(_9776, _9777, intervalFailed, optical_product_upper);
    Interval _9778 = Interval{ 1.0, 1.0 };
    Interval _9779 = _10428;
    Interval _10430 = idiv(_9778, _9779, intervalFailed, interval_divide_upper);
    bool _10437;
    if (!intervalFailed)
    {
        _10437 = intervalFailed;
    }
    else
    {
        _10437 = false;
    }
    bool _10442;
    if (_10437)
    {
        _10442 = jetFailureSite == 0u;
    }
    else
    {
        _10442 = false;
    }
    if (_10442)
    {
        jetFailureSite = 4u;
        jetFailureArguments = float4(1.0, 1.0, _10428.lo, _10428.hi);
    }
    Interval _9780 = _10430;
    Interval _9781 = _10405;
    Interval _10446 = jet_mul_derivative(_9780, _9781, intervalFailed, optical_product_upper);
    Interval _9782 = _10430;
    Interval _9783 = _10406;
    Interval _10447 = jet_mul_derivative(_9782, _9783, intervalFailed, optical_product_upper);
    Interval _9761 = Interval{ 1.0, 1.0 };
    Interval _9762 = _10408;
    Interval _10449 = idiv(_9761, _9762, intervalFailed, interval_divide_upper);
    bool _10456;
    if (!intervalFailed)
    {
        _10456 = intervalFailed;
    }
    else
    {
        _10456 = false;
    }
    bool _10461;
    if (_10456)
    {
        _10461 = jetFailureSite == 0u;
    }
    else
    {
        _10461 = false;
    }
    if (_10461)
    {
        jetFailureSite = 1u;
        jetFailureArguments = float4(1.0, 1.0, _10408.lo, _10408.hi);
    }
    Interval _9763 = Interval{ 0.0, 0.0 };
    Interval _9764 = _10449;
    Interval _9765 = _10446;
    Interval _10465 = jet_mul_derivative(_9764, _9765, intervalFailed, optical_product_upper);
    Interval _9766 = Interval{ as_type<float>(as_type<uint>(_10465.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10465.lo) ^ 2147483648u) };
    Interval _10475 = jet_add_derivative(_9763, _9766, intervalFailed);
    Interval _9767 = _10475;
    Interval _9768 = _10408;
    Interval _10476 = jet_div_derivative(_9767, _9768, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _9769 = Interval{ 0.0, 0.0 };
    Interval _9770 = _10449;
    Interval _9771 = _10447;
    Interval _10477 = jet_mul_derivative(_9770, _9771, intervalFailed, optical_product_upper);
    Interval _9772 = Interval{ as_type<float>(as_type<uint>(_10477.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10477.lo) ^ 2147483648u) };
    Interval _10487 = jet_add_derivative(_9769, _9772, intervalFailed);
    Interval _9773 = _10487;
    Interval _9774 = _10408;
    Interval _10488 = jet_div_derivative(_9773, _9774, intervalFailed, interval_divide_upper, jetFailureSite, jetFailureArguments);
    Interval _9747 = Interval{ _10084, _10085 };
    Interval _9748 = _10449;
    Interval _10490 = imul(_9747, _9748, intervalFailed, optical_product_upper);
    Interval _9749 = Interval{ _10086, _10087 };
    Interval _9750 = _10449;
    Interval _10492 = jet_mul_derivative(_9749, _9750, intervalFailed, optical_product_upper);
    Interval _9751 = _10492;
    Interval _9752 = Interval{ _10084, _10085 };
    Interval _9753 = _10476;
    Interval _10494 = jet_mul_derivative(_9752, _9753, intervalFailed, optical_product_upper);
    Interval _9754 = _10494;
    Interval _10495 = jet_add_derivative(_9751, _9754, intervalFailed);
    Interval _9755 = Interval{ _10088, _10089 };
    Interval _9756 = _10449;
    Interval _10497 = jet_mul_derivative(_9755, _9756, intervalFailed, optical_product_upper);
    Interval _9757 = _10497;
    Interval _9758 = Interval{ _10084, _10085 };
    Interval _9759 = _10488;
    Interval _10499 = jet_mul_derivative(_9758, _9759, intervalFailed, optical_product_upper);
    Interval _9760 = _10499;
    Interval _10500 = jet_add_derivative(_9757, _9760, intervalFailed);
    Interval _9733 = Interval{ _10090, _10091 };
    Interval _9734 = _10449;
    Interval _10502 = imul(_9733, _9734, intervalFailed, optical_product_upper);
    Interval _9735 = Interval{ _10092, _10093 };
    Interval _9736 = _10449;
    Interval _10504 = jet_mul_derivative(_9735, _9736, intervalFailed, optical_product_upper);
    Interval _9737 = _10504;
    Interval _9738 = Interval{ _10090, _10091 };
    Interval _9739 = _10476;
    Interval _10506 = jet_mul_derivative(_9738, _9739, intervalFailed, optical_product_upper);
    Interval _9740 = _10506;
    Interval _10507 = jet_add_derivative(_9737, _9740, intervalFailed);
    Interval _9741 = Interval{ _10094, _10095 };
    Interval _9742 = _10449;
    Interval _10509 = jet_mul_derivative(_9741, _9742, intervalFailed, optical_product_upper);
    Interval _9743 = _10509;
    Interval _9744 = Interval{ _10090, _10091 };
    Interval _9745 = _10488;
    Interval _10511 = jet_mul_derivative(_9744, _9745, intervalFailed, optical_product_upper);
    Interval _9746 = _10511;
    Interval _10512 = jet_add_derivative(_9743, _9746, intervalFailed);
    Interval _9719 = Interval{ _10096, _10097 };
    Interval _9720 = _10449;
    Interval _10514 = imul(_9719, _9720, intervalFailed, optical_product_upper);
    Interval _9721 = Interval{ _10098, _10099 };
    Interval _9722 = _10449;
    Interval _10516 = jet_mul_derivative(_9721, _9722, intervalFailed, optical_product_upper);
    Interval _9723 = _10516;
    Interval _9724 = Interval{ _10096, _10097 };
    Interval _9725 = _10476;
    Interval _10518 = jet_mul_derivative(_9724, _9725, intervalFailed, optical_product_upper);
    Interval _9726 = _10518;
    Interval _10519 = jet_add_derivative(_9723, _9726, intervalFailed);
    Interval _9727 = Interval{ _10100, _10101 };
    Interval _9728 = _10449;
    Interval _10521 = jet_mul_derivative(_9727, _9728, intervalFailed, optical_product_upper);
    Interval _9729 = _10521;
    Interval _9730 = Interval{ _10096, _10097 };
    Interval _9731 = _10488;
    Interval _10523 = jet_mul_derivative(_9730, _9731, intervalFailed, optical_product_upper);
    Interval _9732 = _10523;
    Interval _10524 = jet_add_derivative(_9729, _9732, intervalFailed);
    Interval _9705 = _10502;
    Interval _9706 = ray.outgoing.z.v;
    Interval _10533 = imul(_9705, _9706, intervalFailed, optical_product_upper);
    Interval _9707 = _10507;
    Interval _9708 = ray.outgoing.z.v;
    Interval _10534 = jet_mul_derivative(_9707, _9708, intervalFailed, optical_product_upper);
    Interval _9709 = _10534;
    Interval _9710 = _10502;
    Interval _9711 = ray.outgoing.z.dx;
    Interval _10535 = jet_mul_derivative(_9710, _9711, intervalFailed, optical_product_upper);
    Interval _9712 = _10535;
    Interval _10536 = jet_add_derivative(_9709, _9712, intervalFailed);
    Interval _9713 = _10512;
    Interval _9714 = ray.outgoing.z.v;
    Interval _10537 = jet_mul_derivative(_9713, _9714, intervalFailed, optical_product_upper);
    Interval _9715 = _10537;
    Interval _9716 = _10502;
    Interval _9717 = ray.outgoing.z.dy;
    Interval _10538 = jet_mul_derivative(_9716, _9717, intervalFailed, optical_product_upper);
    Interval _9718 = _10538;
    Interval _10539 = jet_add_derivative(_9715, _9718, intervalFailed);
    Interval _9691 = _10514;
    Interval _9692 = ray.outgoing.y.v;
    Interval _10543 = imul(_9691, _9692, intervalFailed, optical_product_upper);
    Interval _9693 = _10519;
    Interval _9694 = ray.outgoing.y.v;
    Interval _10544 = jet_mul_derivative(_9693, _9694, intervalFailed, optical_product_upper);
    Interval _9695 = _10544;
    Interval _9696 = _10514;
    Interval _9697 = ray.outgoing.y.dx;
    Interval _10545 = jet_mul_derivative(_9696, _9697, intervalFailed, optical_product_upper);
    Interval _9698 = _10545;
    Interval _10546 = jet_add_derivative(_9695, _9698, intervalFailed);
    Interval _9699 = _10524;
    Interval _9700 = ray.outgoing.y.v;
    Interval _10547 = jet_mul_derivative(_9699, _9700, intervalFailed, optical_product_upper);
    Interval _9701 = _10547;
    Interval _9702 = _10514;
    Interval _9703 = ray.outgoing.y.dy;
    Interval _10548 = jet_mul_derivative(_9702, _9703, intervalFailed, optical_product_upper);
    Interval _9704 = _10548;
    Interval _10549 = jet_add_derivative(_9701, _9704, intervalFailed);
    Interval _9685 = _10533;
    Interval _9686 = Interval{ as_type<float>(as_type<uint>(_10543.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10543.lo) ^ 2147483648u) };
    Interval _10575 = iadd(_9685, _9686, intervalFailed);
    Interval _9687 = _10536;
    Interval _9688 = Interval{ as_type<float>(as_type<uint>(_10546.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10546.lo) ^ 2147483648u) };
    Interval _10577 = jet_add_derivative(_9687, _9688, intervalFailed);
    Interval _9689 = _10539;
    Interval _9690 = Interval{ as_type<float>(as_type<uint>(_10549.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10549.lo) ^ 2147483648u) };
    Interval _10579 = jet_add_derivative(_9689, _9690, intervalFailed);
    Interval _9671 = _10514;
    Interval _9672 = ray.outgoing.x.v;
    Interval _10583 = imul(_9671, _9672, intervalFailed, optical_product_upper);
    Interval _9673 = _10519;
    Interval _9674 = ray.outgoing.x.v;
    Interval _10584 = jet_mul_derivative(_9673, _9674, intervalFailed, optical_product_upper);
    Interval _9675 = _10584;
    Interval _9676 = _10514;
    Interval _9677 = ray.outgoing.x.dx;
    Interval _10585 = jet_mul_derivative(_9676, _9677, intervalFailed, optical_product_upper);
    Interval _9678 = _10585;
    Interval _10586 = jet_add_derivative(_9675, _9678, intervalFailed);
    Interval _9679 = _10524;
    Interval _9680 = ray.outgoing.x.v;
    Interval _10587 = jet_mul_derivative(_9679, _9680, intervalFailed, optical_product_upper);
    Interval _9681 = _10587;
    Interval _9682 = _10514;
    Interval _9683 = ray.outgoing.x.dy;
    Interval _10588 = jet_mul_derivative(_9682, _9683, intervalFailed, optical_product_upper);
    Interval _9684 = _10588;
    Interval _10589 = jet_add_derivative(_9681, _9684, intervalFailed);
    Interval _9657 = _10490;
    Interval _9658 = ray.outgoing.z.v;
    Interval _10593 = imul(_9657, _9658, intervalFailed, optical_product_upper);
    Interval _9659 = _10495;
    Interval _9660 = ray.outgoing.z.v;
    Interval _10594 = jet_mul_derivative(_9659, _9660, intervalFailed, optical_product_upper);
    Interval _9661 = _10594;
    Interval _9662 = _10490;
    Interval _9663 = ray.outgoing.z.dx;
    Interval _10595 = jet_mul_derivative(_9662, _9663, intervalFailed, optical_product_upper);
    Interval _9664 = _10595;
    Interval _10596 = jet_add_derivative(_9661, _9664, intervalFailed);
    Interval _9665 = _10500;
    Interval _9666 = ray.outgoing.z.v;
    Interval _10597 = jet_mul_derivative(_9665, _9666, intervalFailed, optical_product_upper);
    Interval _9667 = _10597;
    Interval _9668 = _10490;
    Interval _9669 = ray.outgoing.z.dy;
    Interval _10598 = jet_mul_derivative(_9668, _9669, intervalFailed, optical_product_upper);
    Interval _9670 = _10598;
    Interval _10599 = jet_add_derivative(_9667, _9670, intervalFailed);
    Interval _9651 = _10583;
    Interval _9652 = Interval{ as_type<float>(as_type<uint>(_10593.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10593.lo) ^ 2147483648u) };
    Interval _10625 = iadd(_9651, _9652, intervalFailed);
    Interval _9653 = _10586;
    Interval _9654 = Interval{ as_type<float>(as_type<uint>(_10596.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10596.lo) ^ 2147483648u) };
    Interval _10627 = jet_add_derivative(_9653, _9654, intervalFailed);
    Interval _9655 = _10589;
    Interval _9656 = Interval{ as_type<float>(as_type<uint>(_10599.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10599.lo) ^ 2147483648u) };
    Interval _10629 = jet_add_derivative(_9655, _9656, intervalFailed);
    Interval _9637 = _10490;
    Interval _9638 = ray.outgoing.y.v;
    Interval _10633 = imul(_9637, _9638, intervalFailed, optical_product_upper);
    Interval _9639 = _10495;
    Interval _9640 = ray.outgoing.y.v;
    Interval _10634 = jet_mul_derivative(_9639, _9640, intervalFailed, optical_product_upper);
    Interval _9641 = _10634;
    Interval _9642 = _10490;
    Interval _9643 = ray.outgoing.y.dx;
    Interval _10635 = jet_mul_derivative(_9642, _9643, intervalFailed, optical_product_upper);
    Interval _9644 = _10635;
    Interval _10636 = jet_add_derivative(_9641, _9644, intervalFailed);
    Interval _9645 = _10500;
    Interval _9646 = ray.outgoing.y.v;
    Interval _10637 = jet_mul_derivative(_9645, _9646, intervalFailed, optical_product_upper);
    Interval _9647 = _10637;
    Interval _9648 = _10490;
    Interval _9649 = ray.outgoing.y.dy;
    Interval _10638 = jet_mul_derivative(_9648, _9649, intervalFailed, optical_product_upper);
    Interval _9650 = _10638;
    Interval _10639 = jet_add_derivative(_9647, _9650, intervalFailed);
    Interval _9623 = _10502;
    Interval _9624 = ray.outgoing.x.v;
    Interval _10643 = imul(_9623, _9624, intervalFailed, optical_product_upper);
    Interval _9625 = _10507;
    Interval _9626 = ray.outgoing.x.v;
    Interval _10644 = jet_mul_derivative(_9625, _9626, intervalFailed, optical_product_upper);
    Interval _9627 = _10644;
    Interval _9628 = _10502;
    Interval _9629 = ray.outgoing.x.dx;
    Interval _10645 = jet_mul_derivative(_9628, _9629, intervalFailed, optical_product_upper);
    Interval _9630 = _10645;
    Interval _10646 = jet_add_derivative(_9627, _9630, intervalFailed);
    Interval _9631 = _10512;
    Interval _9632 = ray.outgoing.x.v;
    Interval _10647 = jet_mul_derivative(_9631, _9632, intervalFailed, optical_product_upper);
    Interval _9633 = _10647;
    Interval _9634 = _10502;
    Interval _9635 = ray.outgoing.x.dy;
    Interval _10648 = jet_mul_derivative(_9634, _9635, intervalFailed, optical_product_upper);
    Interval _9636 = _10648;
    Interval _10649 = jet_add_derivative(_9633, _9636, intervalFailed);
    Interval _9617 = _10633;
    Interval _9618 = Interval{ as_type<float>(as_type<uint>(_10643.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10643.lo) ^ 2147483648u) };
    Interval _10675 = iadd(_9617, _9618, intervalFailed);
    Interval _9619 = _10636;
    Interval _9620 = Interval{ as_type<float>(as_type<uint>(_10646.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10646.lo) ^ 2147483648u) };
    Interval _10677 = jet_add_derivative(_9619, _9620, intervalFailed);
    Interval _9621 = _10639;
    Interval _9622 = Interval{ as_type<float>(as_type<uint>(_10649.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10649.lo) ^ 2147483648u) };
    Interval _10679 = jet_add_derivative(_9621, _9622, intervalFailed);
    Interval _9603 = _10575;
    Interval _9604 = Interval{ focal, focal };
    Interval _10682 = imul(_9603, _9604, intervalFailed, optical_product_upper);
    Interval _9605 = _10577;
    Interval _9606 = Interval{ focal, focal };
    Interval _10684 = jet_mul_derivative(_9605, _9606, intervalFailed, optical_product_upper);
    Interval _9607 = _10684;
    Interval _9608 = _10575;
    Interval _9609 = Interval{ 0.0, 0.0 };
    Interval _10685 = jet_mul_derivative(_9608, _9609, intervalFailed, optical_product_upper);
    Interval _9610 = _10685;
    Interval _10686 = jet_add_derivative(_9607, _9610, intervalFailed);
    Interval _9611 = _10579;
    Interval _9612 = Interval{ focal, focal };
    Interval _10688 = jet_mul_derivative(_9611, _9612, intervalFailed, optical_product_upper);
    Interval _9613 = _10688;
    Interval _9614 = _10575;
    Interval _9615 = Interval{ 0.0, 0.0 };
    Interval _10689 = jet_mul_derivative(_9614, _9615, intervalFailed, optical_product_upper);
    Interval _9616 = _10689;
    Interval _10690 = jet_add_derivative(_9613, _9616, intervalFailed);
    Interval _9589 = _10625;
    Interval _9590 = Interval{ focal, focal };
    Interval _10692 = imul(_9589, _9590, intervalFailed, optical_product_upper);
    Interval _9591 = _10627;
    Interval _9592 = Interval{ focal, focal };
    Interval _10694 = jet_mul_derivative(_9591, _9592, intervalFailed, optical_product_upper);
    Interval _9593 = _10694;
    Interval _9594 = _10625;
    Interval _9595 = Interval{ 0.0, 0.0 };
    Interval _10695 = jet_mul_derivative(_9594, _9595, intervalFailed, optical_product_upper);
    Interval _9596 = _10695;
    Interval _10696 = jet_add_derivative(_9593, _9596, intervalFailed);
    Interval _9597 = _10629;
    Interval _9598 = Interval{ focal, focal };
    Interval _10698 = jet_mul_derivative(_9597, _9598, intervalFailed, optical_product_upper);
    Interval _9599 = _10698;
    Interval _9600 = _10625;
    Interval _9601 = Interval{ 0.0, 0.0 };
    Interval _10699 = jet_mul_derivative(_9600, _9601, intervalFailed, optical_product_upper);
    Interval _9602 = _10699;
    Interval _10700 = jet_add_derivative(_9599, _9602, intervalFailed);
    Interval _9575 = _10675;
    Interval _9576 = Interval{ focal, focal };
    Interval _10702 = imul(_9575, _9576, intervalFailed, optical_product_upper);
    Interval _9577 = _10677;
    Interval _9578 = Interval{ focal, focal };
    Interval _10704 = jet_mul_derivative(_9577, _9578, intervalFailed, optical_product_upper);
    Interval _9579 = _10704;
    Interval _9580 = _10675;
    Interval _9581 = Interval{ 0.0, 0.0 };
    Interval _10705 = jet_mul_derivative(_9580, _9581, intervalFailed, optical_product_upper);
    Interval _9582 = _10705;
    Interval _10706 = jet_add_derivative(_9579, _9582, intervalFailed);
    Interval _9583 = _10679;
    Interval _9584 = Interval{ focal, focal };
    Interval _10708 = jet_mul_derivative(_9583, _9584, intervalFailed, optical_product_upper);
    Interval _9585 = _10708;
    Interval _9586 = _10675;
    Interval _9587 = Interval{ 0.0, 0.0 };
    Interval _10709 = jet_mul_derivative(_9586, _9587, intervalFailed, optical_product_upper);
    Interval _9588 = _10709;
    Interval _10710 = jet_add_derivative(_9585, _9588, intervalFailed);
    if (omitted == 0u)
    {
        e0 = OpticalJet{ _10692, _10696, _10700 };
        e1 = OpticalJet{ _10702, _10706, _10710 };
    }
    else
    {
        if (omitted == 1u)
        {
            e0 = OpticalJet{ _10702, _10706, _10710 };
            e1 = OpticalJet{ _10682, _10686, _10690 };
        }
        else
        {
            e0 = OpticalJet{ _10682, _10686, _10690 };
            e1 = OpticalJet{ _10692, _10696, _10700 };
        }
    }
    bool _10724;
    if (!intervalFailed)
    {
        _10724 = jetBranchKnown;
    }
    else
    {
        _10724 = false;
    }
    return _10724;
}

static inline __attribute__((always_inline))
OpticalLocalRoot optical_local_root(thread const float4& box, thread const float2& centre, thread const Interval& f0, thread const Interval& f1, thread const Interval& j00, thread const Interval& j01, thread const Interval& j10, thread const Interval& j11, thread bool& intervalFailed, thread float& optical_product_upper, thread bool& jetBranchKnown)
{
    bool _10743;
    if (!intervalFailed)
    {
        _10743 = !jetBranchKnown;
    }
    else
    {
        _10743 = true;
    }
    bool _10752;
    if (!_10743)
    {
        bool4 _10746 = isnan(box);
        bool4 _10747 = isinf(box);
        _10752 = !all(not(bool4(_10746.x || _10747.x, _10746.y || _10747.y, _10746.z || _10747.z, _10746.w || _10747.w)));
    }
    else
    {
        _10752 = true;
    }
    bool _10761;
    if (!_10752)
    {
        bool2 _10755 = isnan(centre);
        bool2 _10756 = isinf(centre);
        _10761 = !all(not(bool2(_10755.x || _10756.x, _10755.y || _10756.y)));
    }
    else
    {
        _10761 = true;
    }
    bool _10769;
    if (!_10761)
    {
        _10769 = any(box.xy >= box.zw);
    }
    else
    {
        _10769 = true;
    }
    bool _10776;
    if (!_10769)
    {
        _10776 = any(centre <= box.xy);
    }
    else
    {
        _10776 = true;
    }
    bool _10783;
    if (!_10776)
    {
        _10783 = any(centre >= box.zw);
    }
    else
    {
        _10783 = true;
    }
    bool _10804;
    if (!_10783)
    {
        bool _10798;
        if (!(isnan(f0.lo) || isinf(f0.lo)))
        {
            _10798 = !(isnan(f0.hi) || isinf(f0.hi));
        }
        else
        {
            _10798 = false;
        }
        bool _10802;
        if (_10798)
        {
            _10802 = f0.lo <= f0.hi;
        }
        else
        {
            _10802 = false;
        }
        _10804 = !_10802;
    }
    else
    {
        _10804 = true;
    }
    bool _10825;
    if (!_10804)
    {
        bool _10819;
        if (!(isnan(f1.lo) || isinf(f1.lo)))
        {
            _10819 = !(isnan(f1.hi) || isinf(f1.hi));
        }
        else
        {
            _10819 = false;
        }
        bool _10823;
        if (_10819)
        {
            _10823 = f1.lo <= f1.hi;
        }
        else
        {
            _10823 = false;
        }
        _10825 = !_10823;
    }
    else
    {
        _10825 = true;
    }
    bool _10846;
    if (!_10825)
    {
        bool _10840;
        if (!(isnan(j00.lo) || isinf(j00.lo)))
        {
            _10840 = !(isnan(j00.hi) || isinf(j00.hi));
        }
        else
        {
            _10840 = false;
        }
        bool _10844;
        if (_10840)
        {
            _10844 = j00.lo <= j00.hi;
        }
        else
        {
            _10844 = false;
        }
        _10846 = !_10844;
    }
    else
    {
        _10846 = true;
    }
    bool _10867;
    if (!_10846)
    {
        bool _10861;
        if (!(isnan(j01.lo) || isinf(j01.lo)))
        {
            _10861 = !(isnan(j01.hi) || isinf(j01.hi));
        }
        else
        {
            _10861 = false;
        }
        bool _10865;
        if (_10861)
        {
            _10865 = j01.lo <= j01.hi;
        }
        else
        {
            _10865 = false;
        }
        _10867 = !_10865;
    }
    else
    {
        _10867 = true;
    }
    bool _10888;
    if (!_10867)
    {
        bool _10882;
        if (!(isnan(j10.lo) || isinf(j10.lo)))
        {
            _10882 = !(isnan(j10.hi) || isinf(j10.hi));
        }
        else
        {
            _10882 = false;
        }
        bool _10886;
        if (_10882)
        {
            _10886 = j10.lo <= j10.hi;
        }
        else
        {
            _10886 = false;
        }
        _10888 = !_10886;
    }
    else
    {
        _10888 = true;
    }
    bool _10909;
    if (!_10888)
    {
        bool _10903;
        if (!(isnan(j11.lo) || isinf(j11.lo)))
        {
            _10903 = !(isnan(j11.hi) || isinf(j11.hi));
        }
        else
        {
            _10903 = false;
        }
        bool _10907;
        if (_10903)
        {
            _10907 = j11.lo <= j11.hi;
        }
        else
        {
            _10907 = false;
        }
        _10909 = !_10907;
    }
    else
    {
        _10909 = true;
    }
    if (_10909)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float _1692 = spvFMul(spvFAdd(j00.lo, j00.hi), 0.5);
    float _1694 = spvFMul(spvFAdd(j01.lo, j01.hi), 0.5);
    float _1696 = spvFMul(spvFAdd(j10.lo, j10.hi), 0.5);
    float _1698 = spvFMul(spvFAdd(j11.lo, j11.hi), 0.5);
    float4 _10926 = float4(_1692, _1694, _1696, _1698);
    float _1701 = spvFSub(spvFMul(_1692, _1698), spvFMul(_1694, _1696));
    bool4 _10927 = isnan(_10926);
    bool4 _10928 = isinf(_10926);
    bool _10935;
    if (all(not(bool4(_10927.x || _10928.x, _10927.y || _10928.y, _10927.z || _10928.z, _10927.w || _10928.w))))
    {
        _10935 = isnan(_1701) || isinf(_1701);
    }
    else
    {
        _10935 = true;
    }
    bool _10938;
    if (!_10935)
    {
        _10938 = _1701 == 0.0;
    }
    else
    {
        _10938 = true;
    }
    if (_10938)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float4 _1704 = float4(_1698, -_1694, -_1696, _1692) / float4(_1701);
    bool4 _10941 = isnan(_1704);
    bool4 _10942 = isinf(_1704);
    bool _10973;
    if (all(not(bool4(_10941.x || _10942.x, _10941.y || _10942.y, _10941.z || _10942.z, _10941.w || _10942.w))))
    {
        float _10946 = _1704.x;
        Interval param_var_a = Interval{ _10946, _10946 };
        float _10948 = _1704.w;
        Interval param_var_b = Interval{ _10948, _10948 };
        Interval _10950 = imul(param_var_a, param_var_b, intervalFailed, optical_product_upper);
        float _10951 = _1704.y;
        Interval param_var_a_1 = Interval{ _10951, _10951 };
        float _10953 = _1704.z;
        Interval param_var_b_1 = Interval{ _10953, _10953 };
        Interval _10955 = imul(param_var_a_1, param_var_b_1, intervalFailed, optical_product_upper);
        Interval _10737 = _10950;
        Interval _10738 = Interval{ as_type<float>(as_type<uint>(_10955.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10955.lo) ^ 2147483648u) };
        Interval _10965 = iadd(_10737, _10738, intervalFailed);
        bool _10972;
        if (_10965.lo <= 0.0)
        {
            _10972 = _10965.hi >= 0.0;
        }
        else
        {
            _10972 = false;
        }
        _10973 = _10972;
    }
    else
    {
        _10973 = true;
    }
    if (_10973)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    float _10974 = _1704.x;
    Interval param_var_a_2 = Interval{ _10974, _10974 };
    Interval param_var_b_2 = j00;
    Interval _10977 = imul(param_var_a_2, param_var_b_2, intervalFailed, optical_product_upper);
    Interval param_var_a_3 = _10977;
    float _10978 = _1704.y;
    Interval param_var_a_4 = Interval{ _10978, _10978 };
    Interval param_var_b_3 = j10;
    Interval _10981 = imul(param_var_a_4, param_var_b_3, intervalFailed, optical_product_upper);
    Interval param_var_b_4 = _10981;
    Interval _10982 = iadd(param_var_a_3, param_var_b_4, intervalFailed);
    Interval _10735 = Interval{ 1.0, 1.0 };
    Interval _10736 = Interval{ as_type<float>(as_type<uint>(_10982.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_10982.lo) ^ 2147483648u) };
    Interval _10992 = iadd(_10735, _10736, intervalFailed);
    float _10993 = _1704.x;
    Interval param_var_a_5 = Interval{ _10993, _10993 };
    Interval param_var_b_5 = j01;
    Interval _10996 = imul(param_var_a_5, param_var_b_5, intervalFailed, optical_product_upper);
    Interval param_var_a_6 = _10996;
    float _10997 = _1704.y;
    Interval param_var_a_7 = Interval{ _10997, _10997 };
    Interval param_var_b_6 = j11;
    Interval _11000 = imul(param_var_a_7, param_var_b_6, intervalFailed, optical_product_upper);
    Interval param_var_b_7 = _11000;
    Interval _11001 = iadd(param_var_a_6, param_var_b_7, intervalFailed);
    float _11006 = as_type<float>(as_type<uint>(_11001.hi) ^ 2147483648u);
    float _11009 = as_type<float>(as_type<uint>(_11001.lo) ^ 2147483648u);
    float _11010 = _1704.z;
    Interval param_var_a_8 = Interval{ _11010, _11010 };
    Interval param_var_b_8 = j00;
    Interval _11013 = imul(param_var_a_8, param_var_b_8, intervalFailed, optical_product_upper);
    Interval param_var_a_9 = _11013;
    float _11014 = _1704.w;
    Interval param_var_a_10 = Interval{ _11014, _11014 };
    Interval param_var_b_9 = j10;
    Interval _11017 = imul(param_var_a_10, param_var_b_9, intervalFailed, optical_product_upper);
    Interval param_var_b_10 = _11017;
    Interval _11018 = iadd(param_var_a_9, param_var_b_10, intervalFailed);
    float _11023 = as_type<float>(as_type<uint>(_11018.hi) ^ 2147483648u);
    float _11026 = as_type<float>(as_type<uint>(_11018.lo) ^ 2147483648u);
    float _11027 = _1704.z;
    Interval param_var_a_11 = Interval{ _11027, _11027 };
    Interval param_var_b_11 = j01;
    Interval _11030 = imul(param_var_a_11, param_var_b_11, intervalFailed, optical_product_upper);
    Interval param_var_a_12 = _11030;
    float _11031 = _1704.w;
    Interval param_var_a_13 = Interval{ _11031, _11031 };
    Interval param_var_b_12 = j11;
    Interval _11034 = imul(param_var_a_13, param_var_b_12, intervalFailed, optical_product_upper);
    Interval param_var_b_13 = _11034;
    Interval _11035 = iadd(param_var_a_12, param_var_b_13, intervalFailed);
    Interval _10733 = Interval{ 1.0, 1.0 };
    Interval _10734 = Interval{ as_type<float>(as_type<uint>(_11035.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11035.lo) ^ 2147483648u) };
    Interval _11045 = iadd(_10733, _10734, intervalFailed);
    Interval _10731 = Interval{ box.x, box.z };
    Interval _10732 = Interval{ as_type<float>(as_type<uint>(centre.x) ^ 2147483648u), as_type<float>(as_type<uint>(centre.x) ^ 2147483648u) };
    Interval _11060 = iadd(_10731, _10732, intervalFailed);
    Interval _10729 = Interval{ box.y, box.w };
    Interval _10730 = Interval{ as_type<float>(as_type<uint>(centre.y) ^ 2147483648u), as_type<float>(as_type<uint>(centre.y) ^ 2147483648u) };
    Interval _11075 = iadd(_10729, _10730, intervalFailed);
    float _11078 = _1704.x;
    Interval param_var_a_14 = Interval{ _11078, _11078 };
    Interval param_var_b_14 = f0;
    Interval _11081 = imul(param_var_a_14, param_var_b_14, intervalFailed, optical_product_upper);
    Interval param_var_a_15 = _11081;
    float _11082 = _1704.y;
    Interval param_var_a_16 = Interval{ _11082, _11082 };
    Interval param_var_b_15 = f1;
    Interval _11085 = imul(param_var_a_16, param_var_b_15, intervalFailed, optical_product_upper);
    Interval param_var_b_16 = _11085;
    Interval _11086 = iadd(param_var_a_15, param_var_b_16, intervalFailed);
    Interval _10727 = Interval{ centre.x, centre.x };
    Interval _10728 = Interval{ as_type<float>(as_type<uint>(_11086.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11086.lo) ^ 2147483648u) };
    Interval _11097 = iadd(_10727, _10728, intervalFailed);
    float _11100 = _1704.z;
    Interval param_var_a_17 = Interval{ _11100, _11100 };
    Interval param_var_b_17 = f0;
    Interval _11103 = imul(param_var_a_17, param_var_b_17, intervalFailed, optical_product_upper);
    Interval param_var_a_18 = _11103;
    float _11104 = _1704.w;
    Interval param_var_a_19 = Interval{ _11104, _11104 };
    Interval param_var_b_18 = f1;
    Interval _11107 = imul(param_var_a_19, param_var_b_18, intervalFailed, optical_product_upper);
    Interval param_var_b_19 = _11107;
    Interval _11108 = iadd(param_var_a_18, param_var_b_19, intervalFailed);
    Interval _10725 = Interval{ centre.y, centre.y };
    Interval _10726 = Interval{ as_type<float>(as_type<uint>(_11108.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_11108.lo) ^ 2147483648u) };
    Interval _11119 = iadd(_10725, _10726, intervalFailed);
    Interval param_var_a_20 = _11097;
    Interval param_var_a_21 = _10992;
    Interval param_var_b_20 = _11060;
    Interval _11120 = imul(param_var_a_21, param_var_b_20, intervalFailed, optical_product_upper);
    Interval param_var_a_22 = _11120;
    Interval param_var_a_23 = Interval{ _11006, _11009 };
    Interval param_var_b_21 = _11075;
    Interval _11122 = imul(param_var_a_23, param_var_b_21, intervalFailed, optical_product_upper);
    Interval param_var_b_22 = _11122;
    Interval _11123 = iadd(param_var_a_22, param_var_b_22, intervalFailed);
    Interval param_var_b_23 = _11123;
    Interval _11124 = iadd(param_var_a_20, param_var_b_23, intervalFailed);
    Interval param_var_a_24 = _11119;
    Interval param_var_a_25 = Interval{ _11023, _11026 };
    Interval param_var_b_24 = _11060;
    Interval _11128 = imul(param_var_a_25, param_var_b_24, intervalFailed, optical_product_upper);
    Interval param_var_a_26 = _11128;
    Interval param_var_a_27 = _11045;
    Interval param_var_b_25 = _11075;
    Interval _11129 = imul(param_var_a_27, param_var_b_25, intervalFailed, optical_product_upper);
    Interval param_var_b_26 = _11129;
    Interval _11130 = iadd(param_var_a_26, param_var_b_26, intervalFailed);
    Interval param_var_b_27 = _11130;
    Interval _11131 = iadd(param_var_a_24, param_var_b_27, intervalFailed);
    bool _11140;
    if (_10992.lo <= 0.0)
    {
        _11140 = _10992.hi >= 0.0;
    }
    else
    {
        _11140 = false;
    }
    float _11147;
    if (_11140)
    {
        _11147 = 0.0;
    }
    else
    {
        _11147 = precise::min(abs(_10992.lo), abs(_10992.hi));
    }
    Interval param_var_a_28 = Interval{ _11147, precise::max(abs(_10992.lo), abs(_10992.hi)) };
    bool _11156;
    if (_11006 <= 0.0)
    {
        _11156 = _11009 >= 0.0;
    }
    else
    {
        _11156 = false;
    }
    float _11163;
    if (_11156)
    {
        _11163 = 0.0;
    }
    else
    {
        _11163 = precise::min(abs(_11006), abs(_11009));
    }
    Interval param_var_b_28 = Interval{ _11163, precise::max(abs(_11006), abs(_11009)) };
    Interval _11168 = iadd(param_var_a_28, param_var_b_28, intervalFailed);
    bool _11175;
    if (_11023 <= 0.0)
    {
        _11175 = _11026 >= 0.0;
    }
    else
    {
        _11175 = false;
    }
    float _11182;
    if (_11175)
    {
        _11182 = 0.0;
    }
    else
    {
        _11182 = precise::min(abs(_11023), abs(_11026));
    }
    Interval param_var_a_29 = Interval{ _11182, precise::max(abs(_11023), abs(_11026)) };
    bool _11193;
    if (_11045.lo <= 0.0)
    {
        _11193 = _11045.hi >= 0.0;
    }
    else
    {
        _11193 = false;
    }
    float _11200;
    if (_11193)
    {
        _11200 = 0.0;
    }
    else
    {
        _11200 = precise::min(abs(_11045.lo), abs(_11045.hi));
    }
    Interval param_var_b_29 = Interval{ _11200, precise::max(abs(_11045.lo), abs(_11045.hi)) };
    Interval _11205 = iadd(param_var_a_29, param_var_b_29, intervalFailed);
    float _11208 = precise::max(_11168.lo, _11205.lo);
    float _11209 = precise::max(_11168.hi, _11205.hi);
    bool _11214;
    if (!intervalFailed)
    {
        _11214 = !jetBranchKnown;
    }
    else
    {
        _11214 = true;
    }
    bool _11234;
    if (!_11214)
    {
        bool _11228;
        if (!(isnan(_11124.lo) || isinf(_11124.lo)))
        {
            _11228 = !(isnan(_11124.hi) || isinf(_11124.hi));
        }
        else
        {
            _11228 = false;
        }
        bool _11232;
        if (_11228)
        {
            _11232 = _11124.lo <= _11124.hi;
        }
        else
        {
            _11232 = false;
        }
        _11234 = !_11232;
    }
    else
    {
        _11234 = true;
    }
    bool _11254;
    if (!_11234)
    {
        bool _11248;
        if (!(isnan(_11131.lo) || isinf(_11131.lo)))
        {
            _11248 = !(isnan(_11131.hi) || isinf(_11131.hi));
        }
        else
        {
            _11248 = false;
        }
        bool _11252;
        if (_11248)
        {
            _11252 = _11131.lo <= _11131.hi;
        }
        else
        {
            _11252 = false;
        }
        _11254 = !_11252;
    }
    else
    {
        _11254 = true;
    }
    bool _11272;
    if (!_11254)
    {
        bool _11266;
        if (!(isnan(_11208) || isinf(_11208)))
        {
            _11266 = !(isnan(_11209) || isinf(_11209));
        }
        else
        {
            _11266 = false;
        }
        bool _11270;
        if (_11266)
        {
            _11270 = _11208 <= _11209;
        }
        else
        {
            _11270 = false;
        }
        _11272 = !_11270;
    }
    else
    {
        _11272 = true;
    }
    if (_11272)
    {
        return OpticalLocalRoot{ 0u, float4(0.0), 1000000015047466219876688855040.0, short(false) };
    }
    bool _11280;
    if ((isunordered(_11124.hi, box.x) || _11124.hi >= box.x))
    {
        _11280 = _11124.lo > box.z;
    }
    else
    {
        _11280 = true;
    }
    bool _11285;
    if (!_11280)
    {
        _11285 = _11131.hi < box.y;
    }
    else
    {
        _11285 = true;
    }
    bool _11290;
    if (!_11285)
    {
        _11290 = _11131.lo > box.w;
    }
    else
    {
        _11290 = true;
    }
    uint _11309;
    if (_11290)
    {
        _11309 = 2u;
    }
    else
    {
        bool _11295;
        if (_11209 < 1.0)
        {
            _11295 = _11124.lo > box.x;
        }
        else
        {
            _11295 = false;
        }
        bool _11299;
        if (_11295)
        {
            _11299 = _11124.hi < box.z;
        }
        else
        {
            _11299 = false;
        }
        bool _11303;
        if (_11299)
        {
            _11303 = _11131.lo > box.y;
        }
        else
        {
            _11303 = false;
        }
        bool _11307;
        if (_11303)
        {
            _11307 = _11131.hi < box.w;
        }
        else
        {
            _11307 = false;
        }
        uint _11308;
        if (_11307)
        {
            _11308 = 1u;
        }
        else
        {
            _11308 = 0u;
        }
        _11309 = _11308;
    }
    return OpticalLocalRoot{ _11309, float4(_11124.lo, _11131.lo, _11124.hi, _11131.hi), _11209, short(true) };
}

static inline __attribute__((always_inline))
bool optical_intersect_inherited_root(thread OpticalLocalRoot& proved, thread const OpticalLocalRoot& next, thread bool& intervalFailed, thread bool& jetBranchKnown)
{
    bool _11317;
    if (proved.status == 1u)
    {
        _11317 = !bool(proved.evaluated);
    }
    else
    {
        _11317 = true;
    }
    bool _11322;
    if (!_11317)
    {
        _11322 = !bool(next.evaluated);
    }
    else
    {
        _11322 = true;
    }
    bool _11327;
    if (!_11322)
    {
        _11327 = next.status == 2u;
    }
    else
    {
        _11327 = true;
    }
    bool _11330;
    if (!_11327)
    {
        _11330 = intervalFailed;
    }
    else
    {
        _11330 = true;
    }
    bool _11334;
    if (!_11330)
    {
        _11334 = !jetBranchKnown;
    }
    else
    {
        _11334 = true;
    }
    if (_11334)
    {
        return false;
    }
    float4 _11353 = float4(precise::max(proved.enclosure.xy, next.enclosure.xy), precise::min(proved.enclosure.zw, next.enclosure.zw));
    bool4 _11354 = isnan(_11353);
    bool4 _11355 = isinf(_11353);
    bool _11363;
    if (all(not(bool4(_11354.x || _11355.x, _11354.y || _11355.y, _11354.z || _11355.z, _11354.w || _11355.w))))
    {
        _11363 = any(_11353.xy > _11353.zw);
    }
    else
    {
        _11363 = true;
    }
    if (_11363)
    {
        return false;
    }
    proved.enclosure = _11353;
    return true;
}

static inline __attribute__((always_inline))
void write_optical_local_root(thread const uint& outAt, thread const float4& box, thread const ReflectionRoughFrame& receiver, thread const ReflectionLiquidFrame& liquid, thread const spvUnsafeArray<ReflectionSpecularPlane, 4>& planes, thread const uint4& control, thread const Interval3& target, thread const bool& finiteTarget, thread const uint& refinements, device type_RWByteAddressBuffer& results, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments)
{
    float2 _1638 = spvFAdd(box.xy, box.zw) * 0.5;
    OpticalJet a = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    OpticalJet b = OpticalJet{ Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 }, Interval{ 0.0, 0.0 } };
    OpticalLocalRoot root;
    root.status = 0u;
    root.enclosure = float4(0.0);
    root.contraction = 1000000015047466219876688855040.0;
    root.evaluated = short(false);
    bool _4025 = min(refinements, 2u) == 2u;
    float _4061;
    float _4063;
    float _4065;
    float _4067;
    _4061 = 0.0;
    _4063 = 0.0;
    _4065 = 0.0;
    _4067 = 0.0;
    OpticalJetRay ray;
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
    float _4053;
    float _4055;
    float _4057;
    uint _4059;
    float _4062;
    float _4064;
    float _4066;
    float _4068;
    uint _4406;
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
    float _4052 = 0.0;
    float _4054 = 0.0;
    float _4056 = 0.0;
    uint _4058 = 0u;
    uint _4060 = 0u;
    for (;;)
    {
        if (_4060 < (2u + min(refinements, 2u)))
        {
            bool _4076;
            if (_4060 >= 2u)
            {
                _4076 = root.status != 1u;
            }
            else
            {
                _4076 = false;
            }
            if (_4076)
            {
                _4406 = _4058;
                break;
            }
            float4 _4078 = root.enclosure;
            float2 _4082;
            if (_4060 >= 2u)
            {
                _4082 = spvFAdd(_4078.xy, _4078.zw) * 0.5;
            }
            else
            {
                _4082 = _1638;
            }
            intervalFailed = false;
            jetBranchKnown = true;
            float4 _4093;
            if (_4060 == 0u)
            {
                _4093 = box;
            }
            else
            {
                bool _4086;
                if (_4060 == 2u)
                {
                    _4086 = _4025;
                }
                else
                {
                    _4086 = false;
                }
                float4 _4092;
                if (_4086)
                {
                    _4092 = _4078;
                }
                else
                {
                    _4092 = float4(_4082.xyxy);
                }
                _4093 = _4092;
            }
            float4 param_var_box = _4093;
            ReflectionRoughFrame param_var_receiver = receiver;
            ReflectionLiquidFrame param_var_liquid = liquid;
            spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
            uint4 param_var_control = control;
            bool _4098 = optical_jet_forward(param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, ray, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (!_4098)
            {
                if (_4060 >= 2u)
                {
                    _4406 = _4058;
                    break;
                }
                results._m0[(outAt + 12u) >> 2u] = (intervalFailed ? 9u : 8u) + ((!jetBranchKnown) ? 2u : 0u);
                return;
            }
            if (_4060 == 0u)
            {
                float3 midpoint = float3(spvFMul(spvFAdd(ray.outgoing.x.v.lo, ray.outgoing.x.v.hi), 0.5), spvFMul(spvFAdd(ray.outgoing.y.v.lo, ray.outgoing.y.v.hi), 0.5), spvFMul(spvFAdd(ray.outgoing.z.v.lo, ray.outgoing.z.v.hi), 0.5));
                int _4128;
                if (abs(midpoint.x) > abs(midpoint.y))
                {
                    _4128 = 0;
                }
                else
                {
                    _4128 = 1;
                }
                uint _4129 = uint(_4128);
                uint _4137;
                if (abs(midpoint.z) > abs(midpoint[_4129]))
                {
                    _4137 = 2u;
                }
                else
                {
                    _4137 = _4129;
                }
                uint _4143 = (outAt + 32u) >> 2u;
                uint2 _4145 = as_type<uint2>(float2(ray.origin.x.v.lo, ray.origin.x.v.hi));
                results._m0[_4143] = _4145.x;
                results._m0[_4143 + 1u] = _4145.y;
                uint _4155 = (outAt + 40u) >> 2u;
                uint2 _4157 = as_type<uint2>(float2(ray.origin.y.v.lo, ray.origin.y.v.hi));
                results._m0[_4155] = _4157.x;
                results._m0[_4155 + 1u] = _4157.y;
                uint _4167 = (outAt + 48u) >> 2u;
                uint2 _4169 = as_type<uint2>(float2(ray.origin.z.v.lo, ray.origin.z.v.hi));
                results._m0[_4167] = _4169.x;
                results._m0[_4167 + 1u] = _4169.y;
                uint _4179 = (outAt + 56u) >> 2u;
                uint2 _4181 = as_type<uint2>(float2(ray.outgoing.x.v.lo, ray.outgoing.x.v.hi));
                results._m0[_4179] = _4181.x;
                results._m0[_4179 + 1u] = _4181.y;
                uint _4191 = (outAt + 64u) >> 2u;
                uint2 _4193 = as_type<uint2>(float2(ray.outgoing.y.v.lo, ray.outgoing.y.v.hi));
                results._m0[_4191] = _4193.x;
                results._m0[_4191 + 1u] = _4193.y;
                uint _4203 = (outAt + 72u) >> 2u;
                uint2 _4205 = as_type<uint2>(float2(ray.outgoing.z.v.lo, ray.outgoing.z.v.hi));
                results._m0[_4203] = _4205.x;
                results._m0[_4203 + 1u] = _4205.y;
                uint _4215 = (outAt + 80u) >> 2u;
                uint2 _4217 = as_type<uint2>(float2(ray.depth.v.lo, ray.depth.v.hi));
                results._m0[_4215] = _4217.x;
                results._m0[_4215 + 1u] = _4217.y;
                uint _4227 = (outAt + 88u) >> 2u;
                uint2 _4229 = as_type<uint2>(float2(ray.bias0.v.lo, ray.bias0.v.hi));
                results._m0[_4227] = _4229.x;
                results._m0[_4227 + 1u] = _4229.y;
                _4059 = _4137;
            }
            else
            {
                _4059 = _4058;
            }
            OpticalJetRay param_var_ray = ray;
            Interval3 param_var_target = target;
            bool param_var_finiteTerminal = finiteTarget;
            float param_var_focal = precise::max(receiver.projection.x, receiver.projection.y);
            uint param_var_omitted = _4059;
            bool _4244 = optical_jet_residual(param_var_ray, param_var_target, param_var_finiteTerminal, param_var_focal, param_var_omitted, a, b, intervalFailed, optical_product_upper, interval_divide_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
            if (!_4244)
            {
                if (_4060 >= 2u)
                {
                    _4406 = _4059;
                    break;
                }
                results._m0[(outAt + 12u) >> 2u] = (intervalFailed ? 17u : 16u) + ((!jetBranchKnown) ? 2u : 0u);
                return;
            }
            if (_4060 == 0u)
            {
                uint _4259 = (outAt + 96u) >> 2u;
                uint2 _4261 = as_type<uint2>(float2(a.v.lo, a.v.hi));
                results._m0[_4259] = _4261.x;
                results._m0[_4259 + 1u] = _4261.y;
                uint _4271 = (outAt + 104u) >> 2u;
                uint2 _4273 = as_type<uint2>(float2(b.v.lo, b.v.hi));
                results._m0[_4271] = _4273.x;
                results._m0[_4271 + 1u] = _4273.y;
                uint _4297 = (outAt + 112u) >> 2u;
                uint2 _4299 = as_type<uint2>(float2(a.dx.lo, a.dx.hi));
                results._m0[_4297] = _4299.x;
                results._m0[_4297 + 1u] = _4299.y;
                uint _4307 = (outAt + 120u) >> 2u;
                uint2 _4309 = as_type<uint2>(float2(a.dy.lo, a.dy.hi));
                results._m0[_4307] = _4309.x;
                results._m0[_4307 + 1u] = _4309.y;
                uint _4317 = (outAt + 128u) >> 2u;
                uint2 _4319 = as_type<uint2>(float2(b.dx.lo, b.dx.hi));
                results._m0[_4317] = _4319.x;
                results._m0[_4317 + 1u] = _4319.y;
                uint _4327 = (outAt + 136u) >> 2u;
                uint2 _4329 = as_type<uint2>(float2(b.dy.lo, b.dy.hi));
                results._m0[_4327] = _4329.x;
                results._m0[_4327 + 1u] = _4329.y;
                _4027 = _4026;
                _4029 = _4028;
                _4031 = _4030;
                _4033 = _4032;
                _4035 = _4034;
                _4037 = _4036;
                _4039 = _4038;
                _4041 = _4040;
                _4043 = b.dy.lo;
                _4045 = b.dy.hi;
                _4047 = b.dx.lo;
                _4049 = b.dx.hi;
                _4051 = a.dy.lo;
                _4053 = a.dy.hi;
                _4055 = a.dx.lo;
                _4057 = a.dx.hi;
                _4062 = _4061;
                _4064 = _4063;
                _4066 = _4065;
                _4068 = _4067;
            }
            else
            {
                float _4394;
                float _4395;
                float _4396;
                float _4397;
                float _4398;
                float _4399;
                float _4400;
                float _4401;
                float _4402;
                float _4403;
                float _4404;
                float _4405;
                if (_4060 == 1u)
                {
                    float4 param_var_box_1 = box;
                    float2 param_var_centre = _1638;
                    Interval param_var_f0 = a.v;
                    Interval param_var_f1 = b.v;
                    Interval param_var_j00 = Interval{ _4054, _4056 };
                    Interval param_var_j01 = Interval{ _4050, _4052 };
                    Interval param_var_j10 = Interval{ _4046, _4048 };
                    Interval param_var_j11 = Interval{ _4042, _4044 };
                    OpticalLocalRoot _4348 = optical_local_root(param_var_box_1, param_var_centre, param_var_f0, param_var_f1, param_var_j00, param_var_j01, param_var_j10, param_var_j11, intervalFailed, optical_product_upper, jetBranchKnown);
                    root = _4348;
                    _4394 = _4026;
                    _4395 = _4028;
                    _4396 = _4030;
                    _4397 = _4032;
                    _4398 = _4034;
                    _4399 = _4036;
                    _4400 = _4038;
                    _4401 = _4040;
                    _4402 = b.v.lo;
                    _4403 = b.v.hi;
                    _4404 = a.v.lo;
                    _4405 = a.v.hi;
                }
                else
                {
                    bool _4350;
                    if (_4060 == 2u)
                    {
                        _4350 = _4025;
                    }
                    else
                    {
                        _4350 = false;
                    }
                    float _4386;
                    float _4387;
                    float _4388;
                    float _4389;
                    float _4390;
                    float _4391;
                    float _4392;
                    float _4393;
                    if (_4350)
                    {
                        _4386 = b.dy.lo;
                        _4387 = b.dy.hi;
                        _4388 = b.dx.lo;
                        _4389 = b.dx.hi;
                        _4390 = a.dy.lo;
                        _4391 = a.dy.hi;
                        _4392 = a.dx.lo;
                        _4393 = a.dx.hi;
                    }
                    else
                    {
                        float _4367;
                        float _4368;
                        float _4369;
                        float _4370;
                        float _4371;
                        float _4372;
                        float _4373;
                        float _4374;
                        if (_4025)
                        {
                            _4367 = _4026;
                            _4368 = _4028;
                            _4369 = _4030;
                            _4370 = _4032;
                            _4371 = _4034;
                            _4372 = _4036;
                            _4373 = _4038;
                            _4374 = _4040;
                        }
                        else
                        {
                            _4367 = _4042;
                            _4368 = _4044;
                            _4369 = _4046;
                            _4370 = _4048;
                            _4371 = _4050;
                            _4372 = _4052;
                            _4373 = _4054;
                            _4374 = _4056;
                        }
                        float4 param_var_box_2 = _4078;
                        float2 param_var_centre_1 = _4082;
                        Interval param_var_f0_1 = a.v;
                        Interval param_var_f1_1 = b.v;
                        Interval param_var_j00_1 = Interval{ _4373, _4374 };
                        Interval param_var_j01_1 = Interval{ _4371, _4372 };
                        Interval param_var_j10_1 = Interval{ _4369, _4370 };
                        Interval param_var_j11_1 = Interval{ _4367, _4368 };
                        OpticalLocalRoot _4383 = optical_local_root(param_var_box_2, param_var_centre_1, param_var_f0_1, param_var_f1_1, param_var_j00_1, param_var_j01_1, param_var_j10_1, param_var_j11_1, intervalFailed, optical_product_upper, jetBranchKnown);
                        OpticalLocalRoot param_var_next = _4383;
                        bool _4384 = optical_intersect_inherited_root(root, param_var_next, intervalFailed, jetBranchKnown);
                        if (!_4384)
                        {
                            _4406 = _4059;
                            break;
                        }
                        _4386 = _4026;
                        _4387 = _4028;
                        _4388 = _4030;
                        _4389 = _4032;
                        _4390 = _4034;
                        _4391 = _4036;
                        _4392 = _4038;
                        _4393 = _4040;
                    }
                    _4394 = _4386;
                    _4395 = _4387;
                    _4396 = _4388;
                    _4397 = _4389;
                    _4398 = _4390;
                    _4399 = _4391;
                    _4400 = _4392;
                    _4401 = _4393;
                    _4402 = _4061;
                    _4403 = _4063;
                    _4404 = _4065;
                    _4405 = _4067;
                }
                _4027 = _4394;
                _4029 = _4395;
                _4031 = _4396;
                _4033 = _4397;
                _4035 = _4398;
                _4037 = _4399;
                _4039 = _4400;
                _4041 = _4401;
                _4043 = _4042;
                _4045 = _4044;
                _4047 = _4046;
                _4049 = _4048;
                _4051 = _4050;
                _4053 = _4052;
                _4055 = _4054;
                _4057 = _4056;
                _4062 = _4402;
                _4064 = _4403;
                _4066 = _4404;
                _4068 = _4405;
            }
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
            _4054 = _4055;
            _4056 = _4057;
            _4058 = _4059;
            _4060++;
            _4061 = _4062;
            _4063 = _4064;
            _4065 = _4066;
            _4067 = _4068;
            continue;
        }
        else
        {
            _4406 = _4058;
            break;
        }
    }
    uint _4408 = outAt >> 2u;
    results._m0[_4408] = 1u;
    results._m0[_4408 + 1u] = root.status;
    results._m0[_4408 + 2u] = _4406;
    results._m0[_4408 + 3u] = 0u;
    uint _4416 = (outAt + 16u) >> 2u;
    uint4 _4418 = as_type<uint4>(box);
    results._m0[_4416] = _4418.x;
    results._m0[_4416 + 1u] = _4418.y;
    results._m0[_4416 + 2u] = _4418.z;
    results._m0[_4416 + 3u] = _4418.w;
    uint _4428 = (outAt + 144u) >> 2u;
    uint4 _4431 = as_type<uint4>(root.enclosure);
    results._m0[_4428] = _4431.x;
    results._m0[_4428 + 1u] = _4431.y;
    results._m0[_4428 + 2u] = _4431.z;
    results._m0[_4428 + 3u] = _4431.w;
    uint _4441 = (outAt + 160u) >> 2u;
    uint4 _4447 = as_type<uint4>(float4(root.contraction, _1638, 0.0));
    results._m0[_4441] = _4447.x;
    results._m0[_4441 + 1u] = _4447.y;
    results._m0[_4441 + 2u] = _4447.z;
    results._m0[_4441 + 3u] = _4447.w;
    uint _4457 = (outAt + 176u) >> 2u;
    uint2 _4459 = as_type<uint2>(float2(_4065, _4067));
    results._m0[_4457] = _4459.x;
    results._m0[_4457 + 1u] = _4459.y;
    uint _4465 = (outAt + 184u) >> 2u;
    uint2 _4467 = as_type<uint2>(float2(_4061, _4063));
    results._m0[_4465] = _4467.x;
    results._m0[_4465 + 1u] = _4467.y;
}

static inline __attribute__((always_inline))
void src_feature_jets_main(thread const uint3& id, constant type_Settings& Settings, device type_ByteAddressBuffer& frames, device type_ByteAddressBuffer& regions, device type_ByteAddressBuffer& queries, device type_ByteAddressBuffer& optical, device type_RWByteAddressBuffer& results, constant type_RootSettings& RootSettings, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, thread float& interval_sine_upper, thread bool& jetBranchKnown, thread uint& jetFailureSite, thread float4& jetFailureArguments, constant type_TargetSettings& TargetSettings)
{
    bool _2306;
    if (id.y < Settings.queryCount)
    {
        _2306 = id.y >= 64u;
    }
    else
    {
        _2306 = true;
    }
    bool _2309;
    if (!_2306)
    {
        _2309 = id.x >= 128u;
    }
    else
    {
        _2309 = true;
    }
    if (_2309)
    {
        return;
    }
    uint _1464 = ((id.y * 128u) + id.x) * 192u;
    uint _1465 = id.y * 6160u;
    uint _1466 = id.y * 512u;
    for (uint _2310 = 0u; _2310 < 12u; _2310++)
    {
        uint _2312 = (_1464 + (_2310 * 16u)) >> 2u;
        results._m0[_2312] = 0u;
        results._m0[_2312 + 1u] = 0u;
        results._m0[_2312 + 2u] = 0u;
        results._m0[_2312 + 3u] = 0u;
    }
    uint _2317 = _1465 >> 2u;
    uint _2319 = regions._m0[_2317];
    bool _2329;
    if (Settings.queryCount <= 64u)
    {
        _2329 = RootSettings.rootCapacity != 128u;
    }
    else
    {
        _2329 = true;
    }
    bool _2334;
    if (!_2329)
    {
        _2334 = RootSettings.rootFrameStride != 512u;
    }
    else
    {
        _2334 = true;
    }
    bool _2339;
    if (!_2334)
    {
        _2339 = RootSettings.rootResultStride != 192u;
    }
    else
    {
        _2339 = true;
    }
    bool _2344;
    if (!_2339)
    {
        _2344 = RootSettings.rootMode > 1u;
    }
    else
    {
        _2344 = true;
    }
    bool _2347;
    if (!_2344)
    {
        _2347 = _2319 > 128u;
    }
    else
    {
        _2347 = true;
    }
    bool _2350;
    if (!_2347)
    {
        _2350 = regions._m0[(_1465 + 4u) >> 2u] != 0u;
    }
    else
    {
        _2350 = true;
    }
    if (_2350)
    {
        results._m0[(_1464 + 12u) >> 2u] = 32u;
        return;
    }
    bool _2357;
    if (RootSettings.rootMode != 0u)
    {
        _2357 = id.x != 0u;
    }
    else
    {
        _2357 = false;
    }
    if (_2357)
    {
        return;
    }
    bool _2362;
    if (RootSettings.rootMode == 0u)
    {
        _2362 = id.x >= _2319;
    }
    else
    {
        _2362 = false;
    }
    if (_2362)
    {
        return;
    }
    float4 box = float4(1000000015047466219876688855040.0, 1000000015047466219876688855040.0, -1000000015047466219876688855040.0, -1000000015047466219876688855040.0);
    uint _2366;
    if (RootSettings.rootMode != 0u)
    {
        _2366 = 0u;
    }
    else
    {
        _2366 = id.x;
    }
    bool _2367;
    _2367 = false;
    bool _2368;
    uint _2369 = _2366;
    for (;;)
    {
        uint _2373;
        if (RootSettings.rootMode != 0u)
        {
            _2373 = _2319;
        }
        else
        {
            _2373 = id.x + 1u;
        }
        if (_2369 < _2373)
        {
            uint _2375 = (((id.y * 128u) + _2369) * 32u) >> 2u;
            uint _2377 = optical._m0[_2375];
            uint _1480 = _2375 + 2u;
            bool _2386;
            if (optical._m0[_2375 + 1u] == 0u)
            {
                _2386 = optical._m0[_1480] == 0u;
            }
            else
            {
                _2386 = true;
            }
            bool _2389;
            if (!_2386)
            {
                _2389 = optical._m0[_1480] > 8192u;
            }
            else
            {
                _2389 = true;
            }
            bool _2392;
            if (!_2389)
            {
                _2392 = _2377 > optical._m0[_1480];
            }
            else
            {
                _2392 = true;
            }
            bool _2395;
            if (!_2392)
            {
                _2395 = optical._m0[_2375 + 3u] != (optical._m0[_1480] - _2377);
            }
            else
            {
                _2395 = true;
            }
            if (_2395)
            {
                results._m0[(_1464 + 12u) >> 2u] = 32u;
                return;
            }
            if (_2377 == 0u)
            {
                _2368 = _2367;
                uint _1492 = _2369 + 1u;
                _2367 = _2368;
                _2369 = _1492;
                continue;
            }
            uint _2399 = ((((id.y * 128u) + _2369) * 32u) + 16u) >> 2u;
            uint _2401 = optical._m0[_2399];
            uint _2403 = optical._m0[_2399 + 1u];
            uint _2405 = optical._m0[_2399 + 2u];
            uint _2407 = optical._m0[_2399 + 3u];
            float4 _2409 = as_type<float4>(uint4(_2401, _2403, _2405, _2407));
            bool4 _2410 = isnan(_2409);
            bool4 _2411 = isinf(_2409);
            bool _2419;
            if (all(not(bool4(_2410.x || _2411.x, _2410.y || _2411.y, _2410.z || _2411.z, _2410.w || _2411.w))))
            {
                _2419 = any(_2409.xy > _2409.zw);
            }
            else
            {
                _2419 = true;
            }
            if (_2419)
            {
                results._m0[(_1464 + 12u) >> 2u] = 64u;
                return;
            }
            box = float4(precise::min(box.xy, _2409.xy), precise::max(box.zw, _2409.zw));
            _2368 = true;
            uint _1492 = _2369 + 1u;
            _2367 = _2368;
            _2369 = _1492;
            continue;
        }
        else
        {
            break;
        }
    }
    uint _2434 = (_1466 + 496u) >> 2u;
    if (any(uint4(frames._m0[_2434], frames._m0[_2434 + 1u], frames._m0[_2434 + 2u], frames._m0[_2434 + 3u]) != uint4(1u, 0u, 0u, 0u)))
    {
        results._m0[(_1464 + 12u) >> 2u] = 64u;
        return;
    }
    uint _2448 = _1466 >> 2u;
    uint _2450 = frames._m0[_2448];
    uint _2452 = frames._m0[_2448 + 1u];
    uint _2454 = frames._m0[_2448 + 2u];
    uint _2456 = frames._m0[_2448 + 3u];
    float4 _2458 = as_type<float4>(uint4(_2450, _2452, _2454, _2456));
    uint _2459 = (_1466 + 16u) >> 2u;
    uint _2461 = frames._m0[_2459];
    uint _2463 = frames._m0[_2459 + 1u];
    uint _2465 = frames._m0[_2459 + 2u];
    uint _2467 = frames._m0[_2459 + 3u];
    float4 _2469 = as_type<float4>(uint4(_2461, _2463, _2465, _2467));
    uint _2470 = (_1466 + 32u) >> 2u;
    uint _2472 = frames._m0[_2470];
    uint _2474 = frames._m0[_2470 + 1u];
    uint _2476 = frames._m0[_2470 + 2u];
    uint _2478 = frames._m0[_2470 + 3u];
    float4 _2480 = as_type<float4>(uint4(_2472, _2474, _2476, _2478));
    uint _2481 = (_1466 + 48u) >> 2u;
    uint _2483 = frames._m0[_2481];
    uint _2485 = frames._m0[_2481 + 1u];
    uint _2487 = frames._m0[_2481 + 2u];
    uint _2489 = frames._m0[_2481 + 3u];
    float4 _2491 = as_type<float4>(uint4(_2483, _2485, _2487, _2489));
    uint _2492 = (_1466 + 64u) >> 2u;
    uint _2494 = frames._m0[_2492];
    uint _2496 = frames._m0[_2492 + 1u];
    uint _2498 = frames._m0[_2492 + 2u];
    uint _2500 = frames._m0[_2492 + 3u];
    float4 _2502 = as_type<float4>(uint4(_2494, _2496, _2498, _2500));
    uint _2503 = (_1466 + 80u) >> 2u;
    uint _2505 = frames._m0[_2503];
    uint _2507 = frames._m0[_2503 + 1u];
    uint _2509 = frames._m0[_2503 + 2u];
    uint _2511 = frames._m0[_2503 + 3u];
    float4 _2513 = as_type<float4>(uint4(_2505, _2507, _2509, _2511));
    spvUnsafeArray<ReflectionSpecularPlane, 4> planes;
    for (uint _2514 = 0u; _2514 < 4u; _2514++)
    {
        uint _2516 = ((_1466 + 96u) + (_2514 * 48u)) >> 2u;
        planes[_2514].a = as_type<float4>(uint4(frames._m0[_2516], frames._m0[_2516 + 1u], frames._m0[_2516 + 2u], frames._m0[_2516 + 3u]));
        uint _2528 = ((_1466 + 112u) + (_2514 * 48u)) >> 2u;
        planes[_2514].b = as_type<float4>(uint4(frames._m0[_2528], frames._m0[_2528 + 1u], frames._m0[_2528 + 2u], frames._m0[_2528 + 3u]));
        uint _2540 = ((_1466 + 128u) + (_2514 * 48u)) >> 2u;
        planes[_2514].c = as_type<float4>(uint4(frames._m0[_2540], frames._m0[_2540 + 1u], frames._m0[_2540 + 2u], frames._m0[_2540 + 3u]));
    }
    uint _2552 = (_1466 + 288u) >> 2u;
    uint _2554 = frames._m0[_2552];
    uint _2556 = frames._m0[_2552 + 1u];
    uint _2558 = frames._m0[_2552 + 2u];
    uint _2560 = frames._m0[_2552 + 3u];
    float4 _2562 = as_type<float4>(uint4(_2554, _2556, _2558, _2560));
    uint _2563 = (_1466 + 304u) >> 2u;
    uint _2565 = frames._m0[_2563];
    uint _2567 = frames._m0[_2563 + 1u];
    uint _2569 = frames._m0[_2563 + 2u];
    uint _2571 = frames._m0[_2563 + 3u];
    float4 _2573 = as_type<float4>(uint4(_2565, _2567, _2569, _2571));
    uint _2574 = (_1466 + 320u) >> 2u;
    uint _2576 = frames._m0[_2574];
    uint _2578 = frames._m0[_2574 + 1u];
    uint _2580 = frames._m0[_2574 + 2u];
    uint _2582 = frames._m0[_2574 + 3u];
    float4 _2584 = as_type<float4>(uint4(_2576, _2578, _2580, _2582));
    uint _2585 = (_1466 + 336u) >> 2u;
    uint _2587 = frames._m0[_2585];
    uint _2589 = frames._m0[_2585 + 1u];
    uint _2591 = frames._m0[_2585 + 2u];
    uint _2593 = frames._m0[_2585 + 3u];
    float4 _2595 = as_type<float4>(uint4(_2587, _2589, _2591, _2593));
    uint _2596 = (_1466 + 352u) >> 2u;
    uint _2598 = frames._m0[_2596];
    uint _2600 = frames._m0[_2596 + 1u];
    uint _2602 = frames._m0[_2596 + 2u];
    uint _2604 = frames._m0[_2596 + 3u];
    float4 _2606 = as_type<float4>(uint4(_2598, _2600, _2602, _2604));
    uint _2607 = (_1466 + 368u) >> 2u;
    uint _2609 = frames._m0[_2607];
    uint _2611 = frames._m0[_2607 + 1u];
    uint _2613 = frames._m0[_2607 + 2u];
    uint _2615 = frames._m0[_2607 + 3u];
    float4 _2617 = as_type<float4>(uint4(_2609, _2611, _2613, _2615));
    uint _2618 = (_1466 + 384u) >> 2u;
    uint _2620 = frames._m0[_2618];
    uint _2622 = frames._m0[_2618 + 1u];
    uint _2624 = frames._m0[_2618 + 2u];
    uint _2626 = frames._m0[_2618 + 3u];
    float4 _2628 = as_type<float4>(uint4(_2620, _2622, _2624, _2626));
    uint _2629 = (_1466 + 400u) >> 2u;
    uint _2631 = frames._m0[_2629];
    uint _2633 = frames._m0[_2629 + 1u];
    uint _2635 = frames._m0[_2629 + 2u];
    uint _2637 = frames._m0[_2629 + 3u];
    float4 _2639 = as_type<float4>(uint4(_2631, _2633, _2635, _2637));
    uint _2640 = (_1466 + 416u) >> 2u;
    uint _2642 = frames._m0[_2640];
    uint _2644 = frames._m0[_2640 + 1u];
    uint _2646 = frames._m0[_2640 + 2u];
    uint _2648 = frames._m0[_2640 + 3u];
    float4 _2650 = as_type<float4>(uint4(_2642, _2644, _2646, _2648));
    uint _2651 = (_1466 + 480u) >> 2u;
    uint _2653 = frames._m0[_2651];
    uint _1577 = _2651 + 1u;
    uint _2655 = frames._m0[_1577];
    uint _1578 = _2651 + 2u;
    uint _2657 = frames._m0[_1578];
    uint _1579 = _2651 + 3u;
    uint _2659 = frames._m0[_1579];
    uint4 _2660 = uint4(_2653, _2655, _2657, _2659);
    uint _2664 = (id.y * 48u) >> 2u;
    uint _2667 = ((id.y * 48u) + 44u) >> 2u;
    uint _2670 = (_1466 + 432u) >> 2u;
    uint _2681 = (_1466 + 448u) >> 2u;
    uint _2692 = (_1466 + 464u) >> 2u;
    bool _2707;
    if (_2653 <= 4u)
    {
        _2707 = (_2655 >> (_2653 & 31u)) != 0u;
    }
    else
    {
        _2707 = true;
    }
    bool _2710;
    if (!_2707)
    {
        _2710 = _2657 > 1u;
    }
    else
    {
        _2710 = true;
    }
    bool _2713;
    if (!_2710)
    {
        _2713 = _2659 < 1u;
    }
    else
    {
        _2713 = true;
    }
    bool _2716;
    if (!_2713)
    {
        _2716 = _2659 > 3u;
    }
    else
    {
        _2716 = true;
    }
    bool _2722;
    if (!_2716)
    {
        bool _2721;
        if (_2659 == 3u)
        {
            _2721 = _2653 != 4u;
        }
        else
        {
            _2721 = _2653 == 4u;
        }
        _2722 = _2721;
    }
    else
    {
        _2722 = true;
    }
    bool _2725;
    if (!_2722)
    {
        _2725 = queries._m0[_2664] == 4294967295u;
    }
    else
    {
        _2725 = true;
    }
    bool _2730;
    if (!_2725)
    {
        uint _2728;
        if (queries._m0[_2664] == 4294967293u)
        {
            _2728 = 1u;
        }
        else
        {
            _2728 = 0u;
        }
        _2730 = _2657 != _2728;
    }
    else
    {
        _2730 = true;
    }
    bool _2735;
    if (!_2730)
    {
        _2735 = queries._m0[_2667] >= Settings.lobes;
    }
    else
    {
        _2735 = true;
    }
    bool _2738;
    if (!_2735)
    {
        _2738 = queries._m0[_2667] > 7u;
    }
    else
    {
        _2738 = true;
    }
    bool _2745;
    if (!_2738)
    {
        _2745 = queries._m0[((id.y * 48u) + 20u) >> 2u] != (((_2659 << 8u) | (_2655 << 4u)) | _2653);
    }
    else
    {
        _2745 = true;
    }
    bool _2753;
    if (!_2745)
    {
        bool4 _2747 = isnan(_2513);
        bool4 _2748 = isinf(_2513);
        _2753 = !all(not(bool4(_2747.x || _2748.x, _2747.y || _2748.y, _2747.z || _2748.z, _2747.w || _2748.w)));
    }
    else
    {
        _2753 = true;
    }
    bool _2757;
    if (!_2753)
    {
        _2757 = _2513.x < 0.0;
    }
    else
    {
        _2757 = true;
    }
    bool _2761;
    if (!_2757)
    {
        _2761 = _2513.x > 1.0;
    }
    else
    {
        _2761 = true;
    }
    bool _2766;
    if (!_2761)
    {
        _2766 = _2513.y != float(queries._m0[_2667]);
    }
    else
    {
        _2766 = true;
    }
    bool _2771;
    if (!_2766)
    {
        _2771 = any(_2513.zw != float2(0.0));
    }
    else
    {
        _2771 = true;
    }
    bool _2779;
    if (!_2771)
    {
        bool4 _2773 = isnan(_2491);
        bool4 _2774 = isinf(_2491);
        _2779 = !all(not(bool4(_2773.x || _2774.x, _2773.y || _2774.y, _2773.z || _2774.z, _2773.w || _2774.w)));
    }
    else
    {
        _2779 = true;
    }
    bool _2784;
    if (!_2779)
    {
        _2784 = any(_2491.xy <= float2(0.0));
    }
    else
    {
        _2784 = true;
    }
    bool _2792;
    if (!_2784)
    {
        bool4 _2786 = isnan(_2502);
        bool4 _2787 = isinf(_2502);
        _2792 = !all(not(bool4(_2786.x || _2787.x, _2786.y || _2787.y, _2786.z || _2787.z, _2786.w || _2787.w)));
    }
    else
    {
        _2792 = true;
    }
    bool _2804;
    if (!_2792)
    {
        _2804 = any(_2502.xy != float2(float(Settings.width), float(Settings.height)));
    }
    else
    {
        _2804 = true;
    }
    bool _2809;
    if (!_2804)
    {
        _2809 = any(_2502.xy < float2(1.0));
    }
    else
    {
        _2809 = true;
    }
    bool _2814;
    if (!_2809)
    {
        _2814 = any(_2502.xy > float2(16384.0));
    }
    else
    {
        _2814 = true;
    }
    bool _2818;
    if (!_2814)
    {
        _2818 = _2502.z <= 0.0;
    }
    else
    {
        _2818 = true;
    }
    bool _2823;
    if (!_2818)
    {
        _2823 = _2502.w <= _2502.z;
    }
    else
    {
        _2823 = true;
    }
    bool _2831;
    if (!_2823)
    {
        bool4 _2825 = isnan(_2650);
        bool4 _2826 = isinf(_2650);
        _2831 = !all(not(bool4(_2825.x || _2826.x, _2825.y || _2826.y, _2825.z || _2826.z, _2825.w || _2826.w)));
    }
    else
    {
        _2831 = true;
    }
    bool _2838;
    if (!_2831)
    {
        int _2835;
        if (_2659 == 1u)
        {
            _2835 = 1;
        }
        else
        {
            _2835 = 0;
        }
        _2838 = _2650.w != float(_2835);
    }
    else
    {
        _2838 = true;
    }
    bool _2843;
    if (!_2838)
    {
        float3 param_var_f = _2650.xyz;
        uint param_var_kind = _2659;
        _2843 = !feature_valid(param_var_f, param_var_kind);
    }
    else
    {
        _2843 = true;
    }
    bool _2851;
    if (!_2843)
    {
        bool _2850;
        if (_2650.w != 0.0)
        {
            ReflectionSpecularPlane param_var_plane = ReflectionSpecularPlane{ as_type<float4>(uint4(frames._m0[_2670], frames._m0[_2670 + 1u], frames._m0[_2670 + 2u], frames._m0[_2670 + 3u])), as_type<float4>(uint4(frames._m0[_2681], frames._m0[_2681 + 1u], frames._m0[_2681 + 2u], frames._m0[_2681 + 3u])), as_type<float4>(uint4(frames._m0[_2692], frames._m0[_2692 + 1u], frames._m0[_2692 + 2u], frames._m0[_2692 + 3u])) };
            _2850 = !reflection_specular_plane_valid(param_var_plane);
        }
        else
        {
            _2850 = false;
        }
        _2851 = _2850;
    }
    else
    {
        _2851 = true;
    }
    bool _2858;
    if (!_2851)
    {
        bool _2857;
        if (_2657 == 0u)
        {
            ReflectionRoughFrame param_var_old = ReflectionRoughFrame{ _2458, _2469, _2480, _2491, _2502, _2513 };
            _2857 = !reflection_rough_frame_valid(param_var_old);
        }
        else
        {
            _2857 = false;
        }
        _2858 = _2857;
    }
    else
    {
        _2858 = true;
    }
    if (_2858)
    {
        results._m0[(_1464 + 12u) >> 2u] = 64u;
        return;
    }
    bool _2863;
    if (_2657 == 0u)
    {
        _2863 = _2655 != 0u;
    }
    else
    {
        _2863 = true;
    }
    if (_2863)
    {
        ReflectionLiquidFrame param_var_old_1 = ReflectionLiquidFrame{ _2562, _2573, _2584, _2595, _2606, _2617, _2628, _2639 };
        bool _2868;
        if (reflection_liquid_frame_valid(param_var_old_1))
        {
            _2868 = any(_2491 != _2617);
        }
        else
        {
            _2868 = true;
        }
        bool _2872;
        if (!_2868)
        {
            _2872 = any(_2502 != _2628);
        }
        else
        {
            _2872 = true;
        }
        if (_2872)
        {
            results._m0[(_1464 + 12u) >> 2u] = 64u;
            return;
        }
    }
    for (uint _2875 = 0u; _2875 < _2653; _2875++)
    {
        bool _2885;
        if ((_2655 & (1u << (_2875 & 31u))) == 0u)
        {
            ReflectionSpecularPlane param_var_plane_1 = planes[_2875];
            _2885 = !reflection_specular_plane_valid(param_var_plane_1);
        }
        else
        {
            _2885 = false;
        }
        if (_2885)
        {
            results._m0[(_1464 + 12u) >> 2u] = 64u;
            return;
        }
    }
    if (!_2367)
    {
        results._m0[(_1464 + 12u) >> 2u] = 4096u;
        return;
    }
    bool4 _2892 = isnan(box);
    bool4 _2893 = isinf(box);
    bool _2902;
    if (all(not(bool4(_2892.x || _2893.x, _2892.y || _2893.y, _2892.z || _2893.z, _2892.w || _2893.w))))
    {
        _2902 = any(box.xy > box.zw);
    }
    else
    {
        _2902 = true;
    }
    if (_2902)
    {
        results._m0[(_1464 + 12u) >> 2u] = 64u;
        return;
    }
    intervalFailed = false;
    uint param_var_at = _1466;
    float4 param_var_feature = _2650;
    Interval3 _2905 = native_target(param_var_at, param_var_feature, frames, intervalFailed, optical_product_upper, interval_divide_upper, TargetSettings);
    if (intervalFailed)
    {
        results._m0[(_1464 + 12u) >> 2u] = 128u;
        return;
    }
    for (uint _2909 = 0u; _2909 < 2u; _2909++)
    {
        for (uint _2911 = 0u; _2911 < 12u; _2911++)
        {
            uint _2913 = (_1464 + (_2911 * 16u)) >> 2u;
            results._m0[_2913] = 0u;
            results._m0[_2913 + 1u] = 0u;
            results._m0[_2913 + 2u] = 0u;
            results._m0[_2913 + 3u] = 0u;
        }
        uint param_var_outAt = _1464;
        float4 param_var_box = box;
        ReflectionRoughFrame param_var_receiver = ReflectionRoughFrame{ _2458, _2469, _2480, _2491, _2502, _2513 };
        ReflectionLiquidFrame param_var_liquid = ReflectionLiquidFrame{ _2562, _2573, _2584, _2595, _2606, _2617, _2628, _2639 };
        spvUnsafeArray<ReflectionSpecularPlane, 4> param_var_planes = planes;
        uint4 param_var_control = _2660;
        Interval3 param_var_target = _2905;
        bool param_var_finiteTarget = _2650.w != 0.0;
        uint _2927;
        if (RootSettings.rootMode != 0u)
        {
            _2927 = 2u;
        }
        else
        {
            _2927 = 0u;
        }
        uint param_var_refinements = _2927;
        write_optical_local_root(param_var_outAt, param_var_box, param_var_receiver, param_var_liquid, param_var_planes, param_var_control, param_var_target, param_var_finiteTarget, param_var_refinements, results, intervalFailed, optical_product_upper, interval_divide_upper, interval_sine_upper, jetBranchKnown, jetFailureSite, jetFailureArguments);
        bool _2933;
        if (RootSettings.rootMode != 0u)
        {
            _2933 = _2909 != 0u;
        }
        else
        {
            _2933 = true;
        }
        bool _2939;
        if (!_2933)
        {
            _2939 = results._m0[_1464 >> 2u] != 1u;
        }
        else
        {
            _2939 = true;
        }
        bool _2945;
        if (!_2939)
        {
            _2945 = results._m0[(_1464 + 4u) >> 2u] != 0u;
        }
        else
        {
            _2945 = true;
        }
        bool _2951;
        if (!_2945)
        {
            _2951 = results._m0[(_1464 + 12u) >> 2u] != 0u;
        }
        else
        {
            _2951 = true;
        }
        if (_2951)
        {
            return;
        }
        intervalFailed = false;
        Interval _2296 = Interval{ box.x, box.x };
        Interval _2297 = Interval{ as_type<float>(3187671040u), as_type<float>(3187671040u) };
        Interval _2958 = iadd(_2296, _2297, intervalFailed);
        Interval _2294 = Interval{ box.y, box.y };
        Interval _2295 = Interval{ as_type<float>(3187671040u), as_type<float>(3187671040u) };
        Interval _2966 = iadd(_2294, _2295, intervalFailed);
        Interval param_var_a = Interval{ box.z, box.z };
        Interval param_var_b = Interval{ 0.125, 0.125 };
        Interval _2971 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval param_var_a_1 = Interval{ box.w, box.w };
        Interval param_var_b_1 = Interval{ 0.125, 0.125 };
        Interval _2976 = iadd(param_var_a_1, param_var_b_1, intervalFailed);
        if (intervalFailed)
        {
            return;
        }
        float4 _2993 = float4(precise::min(box.xy, precise::max(float2(0.5), float2(_2958.lo, _2966.lo))), precise::max(box.zw, precise::min(spvFSub(_2502.xy, float2(0.5)), float2(_2971.hi, _2976.hi))));
        bool4 _2994 = isnan(_2993);
        bool4 _2995 = isinf(_2993);
        bool _3004;
        if (all(not(bool4(_2994.x || _2995.x, _2994.y || _2995.y, _2994.z || _2995.z, _2994.w || _2995.w))))
        {
            _3004 = any(_2993.xy > box.xy);
        }
        else
        {
            _3004 = true;
        }
        bool _3011;
        if (!_3004)
        {
            _3011 = any(_2993.zw < box.zw);
        }
        else
        {
            _3011 = true;
        }
        bool _3016;
        if (!_3011)
        {
            _3016 = all(_2993 == box);
        }
        else
        {
            _3016 = true;
        }
        if (_3016)
        {
            return;
        }
        box = _2993;
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

