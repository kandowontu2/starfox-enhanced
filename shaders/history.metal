#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

struct type_Settings
{
    uint width;
    uint height;
    uint previousWidth;
    uint previousHeight;
    uint flags;
    float weight;
    uint prefixStride;
    uint padding;
};

kernel void reflection_history_main(device type_ByteAddressBuffer& current [[buffer(0)]], device type_ByteAddressBuffer& history [[buffer(1)]], device type_RWByteAddressBuffer& resolved [[buffer(2)]], constant type_Settings& Settings [[buffer(3)]], texture2d<float> ownership [[texture(0)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    bool _388 = false;
    bool _397 = false;
    bool _441 = false;
    bool _450 = false;
    bool _478 = false;
    uint3 _574 = gl_GlobalInvocationID;
    _478 = false;
    do
    {
        bool _479 = true;
        bool _591;
        if (gl_GlobalInvocationID.x < Settings.width)
        {
            bool _590 = gl_GlobalInvocationID.y >= Settings.height;
            _479 = _590;
            _591 = _590;
        }
        else
        {
            _591 = true;
        }
        if (_591)
        {
            _478 = true;
            break;
        }
        uint _649;
        bool _651;
        uint _16 = Settings.width * Settings.height;
        uint _480 = _16;
        uint _18 = (gl_GlobalInvocationID.y * Settings.width) + gl_GlobalInvocationID.x;
        uint _481 = _18;
        uint _19 = _18 * 4u;
        uint _598 = _19 >> 2u;
        uint _600 = current._m0[_598];
        uint _482 = _600;
        uint _21 = _18 * 16u;
        uint _603 = ((_16 * Settings.prefixStride) + _21) >> 2u;
        uint _23 = _603 + 1u;
        uint _24 = _603 + 2u;
        uint _25 = _603 + 3u;
        float4 _483 = as_type<float4>(uint4(current._m0[_603], current._m0[_23], current._m0[_24], current._m0[_25]));
        uint _26 = Settings.prefixStride + 16u;
        uint _27 = _16 * _26;
        uint _614 = (_27 + _21) >> 2u;
        uint _29 = _614 + 1u;
        uint _30 = _614 + 2u;
        uint _31 = _614 + 3u;
        uint4 _484 = uint4(current._m0[_614], current._m0[_29], current._m0[_30], current._m0[_31]);
        uint _32 = Settings.prefixStride + 32u;
        uint _33 = _16 * _32;
        uint _624 = (_33 + _21) >> 2u;
        uint _35 = _624 + 1u;
        uint _36 = _624 + 2u;
        uint _37 = _624 + 3u;
        float4 _485 = as_type<float4>(uint4(current._m0[_624], current._m0[_35], current._m0[_36], current._m0[_37]));
        uint2 _487 = gl_GlobalInvocationID.xy;
        uint _488 = _600;
        float4 _489 = _485;
        _450 = false;
        bool _840;
        do
        {
            float4 _452 = ownership.read(uint2(int3(int(_487.x), int(_487.y), 0).xy), 0);
            _649 = Settings.flags;
            _651 = (_649 & 4u) != 0u;
            bool _453 = _651;
            bool _451;
            if (_651)
            {
                uint _454 = _16;
                uint _150 = (_487.y * Settings.width) + _487.x;
                uint _455 = _150;
                uint _658 = ((_16 * (Settings.prefixStride + 48u)) + (_150 * 4u)) >> 2u;
                uint _456 = current._m0[_658];
                uint _157 = _150 * 16u;
                uint _661 = ((_16 * (Settings.prefixStride + 52u)) + _157) >> 2u;
                float4 _457 = as_type<float4>(uint4(current._m0[_661], current._m0[_661 + 1u], current._m0[_661 + 2u], current._m0[_661 + 3u]));
                uint _672 = ((_16 * (Settings.prefixStride + 68u)) + _157) >> 2u;
                float4 _458 = as_type<float4>(uint4(current._m0[_672], current._m0[_672 + 1u], current._m0[_672 + 2u], current._m0[_672 + 3u]));
                uint _683 = current._m0[_658] >> 24u;
                bool _459 = true;
                bool _691;
                if (_683 == 255u)
                {
                    bool _690 = _457.w != 1.0;
                    _459 = _690;
                    _691 = _690;
                }
                else
                {
                    _691 = true;
                }
                bool _460 = true;
                bool _698;
                if (!_691)
                {
                    bool _697 = _458.w != 0.0;
                    _460 = _697;
                    _698 = _697;
                }
                else
                {
                    _698 = true;
                }
                bool _461 = true;
                bool _709;
                if (!_698)
                {
                    bool4 _703 = isnan(_457);
                    bool4 _704 = isinf(_457);
                    bool _708 = !all(not(bool4(_703.x || _704.x, _703.y || _704.y, _703.z || _704.z, _703.w || _704.w)));
                    _461 = _708;
                    _709 = _708;
                }
                else
                {
                    _709 = true;
                }
                bool _462 = true;
                bool _720;
                if (!_709)
                {
                    bool4 _714 = isnan(_458);
                    bool4 _715 = isinf(_458);
                    bool _719 = !all(not(bool4(_714.x || _715.x, _714.y || _715.y, _714.z || _715.z, _714.w || _715.w)));
                    _462 = _719;
                    _720 = _719;
                }
                else
                {
                    _720 = true;
                }
                bool _463 = true;
                bool _728;
                if (!_720)
                {
                    bool _727 = any(_457.xyz < float3(0.0));
                    _463 = _727;
                    _728 = _727;
                }
                else
                {
                    _728 = true;
                }
                bool _464 = true;
                bool _736;
                if (!_728)
                {
                    bool _735 = any(_457.xyz > float3(65504.0));
                    _464 = _735;
                    _736 = _735;
                }
                else
                {
                    _736 = true;
                }
                bool _465 = true;
                bool _744;
                if (!_736)
                {
                    bool _743 = any(_458.xyz < float3(0.0));
                    _465 = _743;
                    _744 = _743;
                }
                else
                {
                    _744 = true;
                }
                bool _466 = true;
                bool _752;
                if (!_744)
                {
                    bool _751 = any(_458.xyz > float3(1.0));
                    _466 = _751;
                    _752 = _751;
                }
                else
                {
                    _752 = true;
                }
                bool _467 = true;
                bool _761;
                if (!_752)
                {
                    bool _760 = !any(_458.xyz > float3(0.0));
                    _467 = _760;
                    _761 = _760;
                }
                else
                {
                    _761 = true;
                }
                if (_761)
                {
                    _450 = true;
                    _451 = false;
                    _840 = false;
                    break;
                }
            }
            bool _468 = false;
            bool _809;
            if (_452.w > 0.5)
            {
                uint _469 = _600;
                float4 _470 = _452;
                bool _471 = true;
                _441 = false;
                bool _808;
                do
                {
                    uint _775 = uint(rint(spvFMul(_470.z, 255.0)));
                    uint _443 = _775;
                    bool _444 = true;
                    bool _787;
                    if ((isunordered(_470.x, 0.0) || _470.x > 0.0))
                    {
                        bool _445 = false;
                        bool _786;
                        if (_775 != 1u)
                        {
                            bool _785 = _775 != 2u;
                            _445 = _785;
                            _786 = _785;
                        }
                        else
                        {
                            _786 = false;
                        }
                        _444 = _786;
                        _787 = _786;
                    }
                    else
                    {
                        _787 = true;
                    }
                    bool _442;
                    if (_787)
                    {
                        _441 = true;
                        _442 = false;
                        _808 = false;
                        break;
                    }
                    uint _790 = _600 >> 24u;
                    uint _446 = _790;
                    if (_790 == 253u)
                    {
                        _441 = true;
                        _442 = true;
                        _808 = true;
                        break;
                    }
                    if (_470.x == 0.0039215688593685626983642578125)
                    {
                        bool _798 = _790 == 254u;
                        _441 = true;
                        _442 = _798;
                        _808 = _798;
                        break;
                    }
                    bool _799 = _790 == 255u;
                    bool _447 = false;
                    if (_799)
                    {
                        _447 = true;
                    }
                    bool _448 = false;
                    bool _807;
                    if (_799)
                    {
                        bool _806 = _470.y > 0.0;
                        _448 = _806;
                        _807 = _806;
                    }
                    else
                    {
                        _807 = false;
                    }
                    _441 = true;
                    _442 = _807;
                    _808 = _807;
                    break;
                } while(false);
                bool _449 = _808;
                _468 = _808;
                _809 = _808;
            }
            else
            {
                _809 = false;
            }
            bool _472 = false;
            bool _818;
            if (_809)
            {
                bool _473 = true;
                bool _817;
                if (!_651)
                {
                    bool _816 = (_600 >> 24u) == 255u;
                    _473 = _816;
                    _817 = _816;
                }
                else
                {
                    _817 = true;
                }
                _472 = _817;
                _818 = _817;
            }
            else
            {
                _818 = false;
            }
            bool _474 = false;
            bool _824;
            if (_818)
            {
                bool _823 = _489.w == 1.0;
                _474 = _823;
                _824 = _823;
            }
            else
            {
                _824 = false;
            }
            bool _475 = false;
            bool _833;
            if (_824)
            {
                bool4 _828 = isnan(_489);
                bool4 _829 = isinf(_489);
                bool _832 = all(not(bool4(_828.x || _829.x, _828.y || _829.y, _828.z || _829.z, _828.w || _829.w)));
                _475 = _832;
                _833 = _832;
            }
            else
            {
                _833 = false;
            }
            bool _476 = false;
            bool _839;
            if (_833)
            {
                bool _838 = _489.z > 0.0;
                _476 = _838;
                _839 = _838;
            }
            else
            {
                _839 = false;
            }
            _450 = true;
            _451 = _839;
            _840 = _839;
            break;
        } while(false);
        bool _477 = _840;
        bool _490 = false;
        bool _847;
        if (_840)
        {
            bool _846 = all(_484.xy != uint2(4294967295u));
            _490 = _846;
            _847 = _846;
        }
        else
        {
            _847 = false;
        }
        bool _491 = false;
        bool _854;
        if (_847)
        {
            bool _853 = all(_485.xy >= float2(0.0));
            _491 = _853;
            _854 = _853;
        }
        else
        {
            _854 = false;
        }
        bool _492 = false;
        bool _862;
        if (_854)
        {
            bool _861 = spvFAdd(_485.x, _485.y) <= 1.000010013580322265625;
            _492 = _861;
            _862 = _861;
        }
        else
        {
            _862 = false;
        }
        bool _486 = _862;
        uint4 _864 = as_type<uint4>(_483);
        resolved._m0[_603] = _864.x;
        resolved._m0[_23] = _864.y;
        resolved._m0[_24] = _864.z;
        resolved._m0[_25] = _864.w;
        resolved._m0[_614] = _484.x;
        resolved._m0[_29] = _484.y;
        resolved._m0[_30] = _484.z;
        resolved._m0[_31] = _484.w;
        int _493;
        if (_862)
        {
            _493 = 1;
        }
        else
        {
            _493 = 0;
        }
        uint4 _893 = as_type<uint4>(float4(_485.xyz, float(int(_862))));
        resolved._m0[_624] = _893.x;
        resolved._m0[_35] = _893.y;
        resolved._m0[_36] = _893.z;
        resolved._m0[_37] = _893.w;
        resolved._m0[_598] = _600;
        bool _903 = Settings.prefixStride == 24u;
        if (_903)
        {
            uint _906 = ((_16 * 4u) + _19) >> 2u;
            resolved._m0[_906] = current._m0[_906];
        }
        bool _494 = true;
        bool _911 = Settings.prefixStride != 20u;
        if (_911)
        {
            _494 = _903;
        }
        if (_911 ? _903 : true)
        {
            uint _43 = (_16 * (Settings.prefixStride - 16u)) + _21;
            uint _495 = _43;
            uint _917 = _43 >> 2u;
            uint _44 = _917 + 1u;
            uint _921 = current._m0[_44];
            uint _45 = _917 + 2u;
            uint _923 = current._m0[_45];
            uint _46 = _917 + 3u;
            uint _925 = current._m0[_46];
            resolved._m0[_917] = current._m0[_917];
            resolved._m0[_44] = _921;
            resolved._m0[_45] = _923;
            resolved._m0[_46] = _925;
        }
        bool _496 = _651;
        uint _498;
        uint _937;
        if (_651)
        {
            uint _934 = ((_16 * (Settings.prefixStride + 48u)) + _19) >> 2u;
            _498 = current._m0[_934];
            _937 = current._m0[_934];
        }
        else
        {
            _498 = _600;
            _937 = _600;
        }
        uint _497 = _937;
        float4 _499 = float4(0.0);
        float4 _500 = float4(0.0);
        float4 _972;
        float4 _973;
        if (_651)
        {
            uint _940 = ((_16 * (Settings.prefixStride + 52u)) + _21) >> 2u;
            uint _942 = current._m0[_940];
            uint _53 = _940 + 1u;
            uint _944 = current._m0[_53];
            uint _54 = _940 + 2u;
            uint _946 = current._m0[_54];
            uint _55 = _940 + 3u;
            uint _948 = current._m0[_55];
            float4 _950 = as_type<float4>(uint4(_942, _944, _946, _948));
            _499 = _950;
            uint _951 = ((_16 * (Settings.prefixStride + 68u)) + _21) >> 2u;
            uint _953 = current._m0[_951];
            uint _59 = _951 + 1u;
            uint _955 = current._m0[_59];
            uint _60 = _951 + 2u;
            uint _957 = current._m0[_60];
            uint _61 = _951 + 3u;
            uint _959 = current._m0[_61];
            float4 _961 = as_type<float4>(uint4(_953, _955, _957, _959));
            _500 = _961;
            resolved._m0[((_16 * (Settings.prefixStride + 48u)) + _19) >> 2u] = _937;
            resolved._m0[_940] = _942;
            resolved._m0[_53] = _944;
            resolved._m0[_54] = _946;
            resolved._m0[_55] = _948;
            resolved._m0[_951] = _953;
            resolved._m0[_59] = _955;
            resolved._m0[_60] = _957;
            resolved._m0[_61] = _959;
            _972 = _961;
            _973 = _950;
        }
        else
        {
            _972 = float4(0.0);
            _973 = float4(0.0);
        }
        bool _501 = true;
        bool _979;
        if (_862)
        {
            bool _978 = (_649 & 1u) == 0u;
            _501 = _978;
            _979 = _978;
        }
        else
        {
            _979 = true;
        }
        bool _502 = true;
        bool _986;
        if (!_979)
        {
            bool _985 = Settings.weight == 0.0;
            _502 = _985;
            _986 = _985;
        }
        else
        {
            _986 = true;
        }
        bool _503 = true;
        bool _993;
        if (!_986)
        {
            bool _992 = _483.w != 1.0;
            _503 = _992;
            _993 = _992;
        }
        else
        {
            _993 = true;
        }
        bool _504 = true;
        bool _1004;
        if (!_993)
        {
            bool4 _998 = isnan(_483);
            bool4 _999 = isinf(_483);
            bool _1003 = !all(not(bool4(_998.x || _999.x, _998.y || _999.y, _998.z || _999.z, _998.w || _999.w)));
            _504 = _1003;
            _1004 = _1003;
        }
        else
        {
            _1004 = true;
        }
        bool _505 = true;
        bool _1011;
        if (!_1004)
        {
            bool _1010 = _483.z <= 0.0;
            _505 = _1010;
            _1011 = _1010;
        }
        else
        {
            _1011 = true;
        }
        bool _506 = true;
        bool _1019;
        if (!_1011)
        {
            bool _1018 = any(_484.zw == uint2(4294967295u));
            _506 = _1018;
            _1019 = _1018;
        }
        else
        {
            _1019 = true;
        }
        if (_1019)
        {
            _478 = true;
            break;
        }
        float2 _507 = spvFSub(_483.xy, float2(0.5));
        bool _508 = true;
        bool _1040;
        if (!any(_507 < float2(0.0)))
        {
            bool _1039 = any(_507 > float2(float(Settings.previousWidth - 1u), float(Settings.previousHeight - 1u)));
            _508 = _1039;
            _1040 = _1039;
        }
        else
        {
            _1040 = true;
        }
        if (_1040)
        {
            _478 = true;
            break;
        }
        int2 _1045 = int2(floor(_507));
        int2 _509 = _1045;
        float2 _510 = spvFSub(_507, float2(_1045));
        uint _69 = Settings.previousWidth * Settings.previousHeight;
        uint _511 = _69;
        float3 _512 = float3(0.0);
        bool _514 = false;
        bool _1061;
        if (_651)
        {
            uint _1054 = _600 >> 24u;
            bool _515 = true;
            bool _1060;
            if (_1054 != 254u)
            {
                bool _1059 = _1054 == 253u;
                _515 = _1059;
                _1060 = _1059;
            }
            else
            {
                _1060 = true;
            }
            _514 = _1060;
            _1061 = _1060;
        }
        else
        {
            _1061 = false;
        }
        bool _516 = false;
        bool _1067;
        if (_1061)
        {
            bool _1066 = _484.x == 4294967294u;
            _516 = _1066;
            _1067 = _1066;
        }
        else
        {
            _1067 = false;
        }
        bool _517 = false;
        bool _1073;
        if (_1067)
        {
            bool _1072 = _484.z == 4294967294u;
            _517 = _1072;
            _1073 = _1072;
        }
        else
        {
            _1073 = false;
        }
        bool _513 = _1073;
        float _518 = 0.0;
        float3 _1078;
        _1078 = float3(0.0);
        float _433;
        float _434;
        float _435;
        float3 _438;
        float _522;
        float _523;
        uint _545;
        bool _1076;
        float3 _1079;
        float _1081;
        float3 _1368;
        float _1369;
        bool _1370;
        uint _519 = 0u;
        bool _1075 = false;
        float _1080 = 0.0;
        uint _1082 = 0u;
        for (;;)
        {
            if (_1082 < 2u)
            {
                _1079 = _1078;
                float3 _1087;
                float _1090;
                uint _520 = 0u;
                float _1089 = _1080;
                uint _1091 = 0u;
                for (;;)
                {
                    if (_1091 < 2u)
                    {
                        float _1103;
                        if (_1091 != 0u)
                        {
                            _522 = _510.x;
                            _1103 = _510.x;
                        }
                        else
                        {
                            float _70 = spvFSub(1.0, _510.x);
                            _522 = _70;
                            _1103 = _70;
                        }
                        float _1112;
                        if (_1082 != 0u)
                        {
                            _523 = _510.y;
                            _1112 = _510.y;
                        }
                        else
                        {
                            float _71 = spvFSub(1.0, _510.y);
                            _523 = _71;
                            _1112 = _71;
                        }
                        float _72 = spvFMul(_1103, _1112);
                        float _521 = _72;
                        if (_72 <= 0.0)
                        {
                            _1087 = _1079;
                            _1090 = _1089;
                            uint _119 = _1091 + 1u;
                            _520 = _119;
                            _1079 = _1087;
                            _1089 = _1090;
                            _1091 = _119;
                            continue;
                        }
                        int2 _524 = min((_1045 + int2(int(_1091), int(_1082))), int2(int(Settings.previousWidth - 1u), int(Settings.previousHeight - 1u)));
                        uint _77 = (uint(_524.y) * Settings.previousWidth) + uint(_524.x);
                        uint _525 = _77;
                        uint _78 = _69 * _26;
                        uint _79 = _77 * 16u;
                        uint _1129 = (_78 + _79) >> 2u;
                        uint4 _1138 = uint4(history._m0[_1129], history._m0[_1129 + 1u], history._m0[_1129 + 2u], history._m0[_1129 + 3u]);
                        uint4 _526 = _1138;
                        uint _84 = _69 * _32;
                        uint _1139 = (_84 + _79) >> 2u;
                        float4 _527 = as_type<float4>(uint4(history._m0[_1139], history._m0[_1139 + 1u], history._m0[_1139 + 2u], history._m0[_1139 + 3u]));
                        bool _528 = true;
                        bool _1161;
                        if (!any(_1138.xy != _484.zw))
                        {
                            bool _1160 = _527.w != 1.0;
                            _528 = _1160;
                            _1161 = _1160;
                        }
                        else
                        {
                            _1161 = true;
                        }
                        bool _529 = true;
                        bool _1172;
                        if (!_1161)
                        {
                            bool4 _1166 = isnan(_527);
                            bool4 _1167 = isinf(_527);
                            bool _1171 = !all(not(bool4(_1166.x || _1167.x, _1166.y || _1167.y, _1166.z || _1167.z, _1166.w || _1167.w)));
                            _529 = _1171;
                            _1172 = _1171;
                        }
                        else
                        {
                            _1172 = true;
                        }
                        bool _530 = true;
                        bool _1179;
                        if (!_1172)
                        {
                            bool _1178 = _527.z <= 0.0;
                            _530 = _1178;
                            _1179 = _1178;
                        }
                        else
                        {
                            _1179 = true;
                        }
                        bool _531 = true;
                        bool _1195;
                        if (!_1179)
                        {
                            bool _532 = false;
                            bool _1194;
                            if (!_1073)
                            {
                                bool _1193 = abs(spvFSub(_527.z, _483.z)) > precise::max(0.00999999977648258209228515625, spvFMul(_483.z, 0.004999999888241291046142578125));
                                _532 = _1193;
                                _1194 = _1193;
                            }
                            else
                            {
                                _1194 = false;
                            }
                            _531 = _1194;
                            _1195 = _1194;
                        }
                        else
                        {
                            _1195 = true;
                        }
                        if (_1195)
                        {
                            _478 = true;
                            _1081 = _1089;
                            _1076 = true;
                            break;
                        }
                        float _1202;
                        if (_1073)
                        {
                            float _92 = spvFAdd(_1089, _72 / _527.z);
                            _518 = _92;
                            _1202 = _92;
                        }
                        else
                        {
                            _1202 = _1089;
                        }
                        float2 _533 = float2(0.00200000009499490261077880859375);
                        float2 _1204;
                        _1204 = float2(0.00200000009499490261077880859375);
                        float2 _110;
                        int _111;
                        int _534 = 0;
                        int _1206 = 0;
                        for (; _1206 < 2; _111 = _1206 + 1, _534 = _111, _1204 = _110, _1206 = _111)
                        {
                            float2 _535 = float2(0.0);
                            float2 _1211;
                            _1211 = float2(0.0);
                            int _107;
                            float2 _1212;
                            int _536 = -1;
                            int _1214 = -1;
                            for (; _1214 <= 1; _107 = _1214 + 2, _536 = _107, _1211 = _1212, _1214 = _107)
                            {
                                int2 _537 = _524;
                                uint _1219 = uint(_1206);
                                _537[_1219] += _1214;
                                bool _538 = true;
                                bool _1234;
                                if (!any(_537 < int2(0)))
                                {
                                    bool _1233 = any(_537 >= int2(int(Settings.previousWidth), int(Settings.previousHeight)));
                                    _538 = _1233;
                                    _1234 = _1233;
                                }
                                else
                                {
                                    _1234 = true;
                                }
                                if (_1234)
                                {
                                    _1212 = _1211;
                                    continue;
                                }
                                uint _95 = (uint(_537.y) * Settings.previousWidth) + uint(_537.x);
                                uint _539 = _95;
                                uint _96 = _95 * 16u;
                                uint _1243 = (_78 + _96) >> 2u;
                                uint4 _1252 = uint4(history._m0[_1243], history._m0[_1243 + 1u], history._m0[_1243 + 2u], history._m0[_1243 + 3u]);
                                uint4 _540 = _1252;
                                uint _1253 = (_84 + _96) >> 2u;
                                float4 _541 = as_type<float4>(uint4(history._m0[_1253], history._m0[_1253 + 1u], history._m0[_1253 + 2u], history._m0[_1253 + 3u]));
                                bool _542 = false;
                                bool _1274;
                                if (all(_1252.xy == _484.zw))
                                {
                                    bool _1273 = _541.w == 1.0;
                                    _542 = _1273;
                                    _1274 = _1273;
                                }
                                else
                                {
                                    _1274 = false;
                                }
                                bool _543 = false;
                                bool _1283;
                                if (_1274)
                                {
                                    bool4 _1278 = isnan(_541);
                                    bool4 _1279 = isinf(_541);
                                    bool _1282 = all(not(bool4(_1278.x || _1279.x, _1278.y || _1279.y, _1278.z || _1279.z, _1278.w || _1279.w)));
                                    _543 = _1282;
                                    _1283 = _1282;
                                }
                                else
                                {
                                    _1283 = false;
                                }
                                float2 _1292;
                                if (_1283)
                                {
                                    float2 _1291 = precise::max(_1211, abs(spvFSub(_541.xy, _527.xy)) * 1.5);
                                    _535 = _1291;
                                    _1292 = _1291;
                                }
                                else
                                {
                                    _1292 = _1211;
                                }
                                _1212 = _1292;
                            }
                            uint _1293 = uint(_1206);
                            _110 = spvFAdd(_1204, _1211 * abs(spvFSub(_507[_1293], float(_524[_1293]))));
                            _533 = _110;
                        }
                        if (any(abs(spvFSub(_527.xy, _485.xy)) > precise::min(_1204, float2(0.0500000007450580596923828125))))
                        {
                            _478 = true;
                            _1081 = _1202;
                            _1076 = true;
                            break;
                        }
                        uint _1313;
                        if (_651)
                        {
                            uint _114 = _69 * (Settings.prefixStride + 48u);
                            _545 = _114;
                            _1313 = _114;
                        }
                        else
                        {
                            _545 = 0u;
                            _1313 = 0u;
                        }
                        uint _1314 = (_1313 + (_77 * 4u)) >> 2u;
                        uint _544 = history._m0[_1314];
                        if ((history._m0[_1314] >> 24u) != 255u)
                        {
                            _478 = true;
                            _1081 = _1202;
                            _1076 = true;
                            break;
                        }
                        uint _546 = history._m0[_1314];
                        float3 _169 = float3(float(history._m0[_1314] & 255u), float((history._m0[_1314] >> 8u) & 255u), float((history._m0[_1314] >> 16u) & 255u)) / float3(255.0);
                        float3 _437 = _169;
                        float3 _1366;
                        if ((_649 & 2u) != 0u)
                        {
                            float3 _439 = _169;
                            float _1344;
                            if (_439.x <= 0.040449999272823333740234375)
                            {
                                float _170 = _439.x / 12.9200000762939453125;
                                _433 = _170;
                                _1344 = _170;
                            }
                            else
                            {
                                float _1343 = precise::powr(spvFAdd(_439.x, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                                _433 = _1343;
                                _1344 = _1343;
                            }
                            float _1354;
                            if (_439.y <= 0.040449999272823333740234375)
                            {
                                float _173 = _439.y / 12.9200000762939453125;
                                _434 = _173;
                                _1354 = _173;
                            }
                            else
                            {
                                float _1353 = precise::powr(spvFAdd(_439.y, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                                _434 = _1353;
                                _1354 = _1353;
                            }
                            float _1364;
                            if (_439.z <= 0.040449999272823333740234375)
                            {
                                float _176 = _439.z / 12.9200000762939453125;
                                _435 = _176;
                                _1364 = _176;
                            }
                            else
                            {
                                float _1363 = precise::powr(spvFAdd(_439.z, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                                _435 = _1363;
                                _1364 = _1363;
                            }
                            float3 _1365 = float3(_1344, _1354, _1364);
                            float3 _436 = _1365;
                            _438 = _1365;
                            _1366 = _1365;
                        }
                        else
                        {
                            _438 = _169;
                            _1366 = _169;
                        }
                        float3 _440 = _1366;
                        float3 _118 = spvFAdd(_1079, _1366 * _72);
                        _512 = _118;
                        _1087 = _118;
                        _1090 = _1202;
                        uint _119 = _1091 + 1u;
                        _520 = _119;
                        _1079 = _1087;
                        _1089 = _1090;
                        _1091 = _119;
                        continue;
                    }
                    else
                    {
                        _1081 = _1089;
                        _1076 = _1075;
                        break;
                    }
                }
                if (_1076)
                {
                    _1368 = _1079;
                    _1369 = _1081;
                    _1370 = _1076;
                    break;
                }
                uint _120 = _1082 + 1u;
                _519 = _120;
                _1075 = _1076;
                _1078 = _1079;
                _1080 = _1081;
                _1082 = _120;
                continue;
            }
            else
            {
                _1368 = _1078;
                _1369 = _1080;
                _1370 = _1075;
                break;
            }
        }
        if (_1370)
        {
            break;
        }
        bool _547 = false;
        bool _1392;
        if (_1073)
        {
            bool _548 = true;
            bool _1381;
            if (!(isnan(_1369) || isinf(_1369)))
            {
                bool _1380 = _1369 <= 0.0;
                _548 = _1380;
                _1381 = _1380;
            }
            else
            {
                _1381 = true;
            }
            bool _549 = true;
            bool _1391;
            if (!_1381)
            {
                bool _1390 = abs(spvFSub(1.0 / _1369, _483.z)) > precise::max(0.00999999977648258209228515625, spvFMul(_483.z, 0.004999999888241291046142578125));
                _549 = _1390;
                _1391 = _1390;
            }
            else
            {
                _1391 = true;
            }
            _547 = _1391;
            _1392 = _1391;
        }
        else
        {
            _1392 = false;
        }
        if (_1392)
        {
            _478 = true;
            break;
        }
        uint _551 = _937;
        float3 _179 = float3(float(_937 & 255u), float((_937 >> 8u) & 255u), float((_937 >> 16u) & 255u)) / float3(255.0);
        float3 _429 = _179;
        bool _1405 = (_649 & 2u) != 0u;
        float3 _430;
        float3 _1440;
        if (_1405)
        {
            float3 _431 = _179;
            float _425;
            float _1418;
            if (_431.x <= 0.040449999272823333740234375)
            {
                float _180 = _431.x / 12.9200000762939453125;
                _425 = _180;
                _1418 = _180;
            }
            else
            {
                float _1417 = precise::powr(spvFAdd(_431.x, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                _425 = _1417;
                _1418 = _1417;
            }
            float _426;
            float _1428;
            if (_431.y <= 0.040449999272823333740234375)
            {
                float _183 = _431.y / 12.9200000762939453125;
                _426 = _183;
                _1428 = _183;
            }
            else
            {
                float _1427 = precise::powr(spvFAdd(_431.y, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                _426 = _1427;
                _1428 = _1427;
            }
            float _427;
            float _1438;
            if (_431.z <= 0.040449999272823333740234375)
            {
                float _186 = _431.z / 12.9200000762939453125;
                _427 = _186;
                _1438 = _186;
            }
            else
            {
                float _1437 = precise::powr(spvFAdd(_431.z, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                _427 = _1437;
                _1438 = _1437;
            }
            float3 _1439 = float3(_1418, _1428, _1438);
            float3 _428 = _1439;
            _430 = _1439;
            _1440 = _1439;
        }
        else
        {
            _430 = _179;
            _1440 = _179;
        }
        float3 _432 = _1440;
        float3 _550 = _1440;
        float3 _552 = _1440;
        float3 _1442;
        float3 _1445;
        _1442 = _1440;
        _1445 = _1440;
        int _143;
        float _380;
        float _381;
        float _382;
        float3 _385;
        bool _389;
        bool _398;
        uint _565;
        float3 _1443;
        float3 _1446;
        int _553 = -1;
        int _1447 = -1;
        for (; _1447 <= 1; _143 = _1447 + 1, _553 = _143, _1442 = _1443, _1445 = _1446, _1447 = _143)
        {
            _1443 = _1442;
            _1446 = _1445;
            int _142;
            float3 _1452;
            float3 _1454;
            int _554 = -1;
            int _1455 = -1;
            for (; _1455 <= 1; _142 = _1455 + 1, _554 = _142, _1443 = _1452, _1446 = _1454, _1455 = _142)
            {
                int2 _555 = clamp(int2(gl_GlobalInvocationID.xy) + int2(_1455, _1447), int2(0), int2(int(Settings.width - 1u), int(Settings.height - 1u)));
                uint _128 = (uint(_555.y) * Settings.width) + uint(_555.x);
                uint _556 = _128;
                uint _129 = _128 * 16u;
                uint _1471 = (_27 + _129) >> 2u;
                uint4 _1480 = uint4(current._m0[_1471], current._m0[_1471 + 1u], current._m0[_1471 + 2u], current._m0[_1471 + 3u]);
                uint4 _557 = _1480;
                uint _1481 = (_33 + _129) >> 2u;
                float4 _1491 = as_type<float4>(uint4(current._m0[_1481], current._m0[_1481 + 1u], current._m0[_1481 + 2u], current._m0[_1481 + 3u]));
                float4 _558 = _1491;
                uint _138 = _128 * 4u;
                uint _1492 = _138 >> 2u;
                uint _559 = current._m0[_1492];
                bool _560 = true;
                bool _1706;
                if (!any(_1480.xy != _484.xy))
                {
                    uint2 _561 = uint2(_555);
                    uint _562 = current._m0[_1492];
                    float4 _563 = _1491;
                    _397 = false;
                    bool _1704;
                    do
                    {
                        float4 _399 = ownership.read(uint2(int3(int(_561.x), int(_561.y), 0).xy), 0);
                        bool _400 = _651;
                        if (_651)
                        {
                            uint _401 = _16;
                            uint _190 = (_561.y * Settings.width) + _561.x;
                            uint _402 = _190;
                            uint _1522 = ((_16 * (Settings.prefixStride + 48u)) + (_190 * 4u)) >> 2u;
                            uint _403 = current._m0[_1522];
                            uint _197 = _190 * 16u;
                            uint _1525 = ((_16 * (Settings.prefixStride + 52u)) + _197) >> 2u;
                            float4 _404 = as_type<float4>(uint4(current._m0[_1525], current._m0[_1525 + 1u], current._m0[_1525 + 2u], current._m0[_1525 + 3u]));
                            uint _1536 = ((_16 * (Settings.prefixStride + 68u)) + _197) >> 2u;
                            float4 _405 = as_type<float4>(uint4(current._m0[_1536], current._m0[_1536 + 1u], current._m0[_1536 + 2u], current._m0[_1536 + 3u]));
                            uint _1547 = current._m0[_1522] >> 24u;
                            bool _406 = true;
                            bool _1555;
                            if (_1547 == 255u)
                            {
                                bool _1554 = _404.w != 1.0;
                                _406 = _1554;
                                _1555 = _1554;
                            }
                            else
                            {
                                _1555 = true;
                            }
                            bool _407 = true;
                            bool _1562;
                            if (!_1555)
                            {
                                bool _1561 = _405.w != 0.0;
                                _407 = _1561;
                                _1562 = _1561;
                            }
                            else
                            {
                                _1562 = true;
                            }
                            bool _408 = true;
                            bool _1573;
                            if (!_1562)
                            {
                                bool4 _1567 = isnan(_404);
                                bool4 _1568 = isinf(_404);
                                bool _1572 = !all(not(bool4(_1567.x || _1568.x, _1567.y || _1568.y, _1567.z || _1568.z, _1567.w || _1568.w)));
                                _408 = _1572;
                                _1573 = _1572;
                            }
                            else
                            {
                                _1573 = true;
                            }
                            bool _409 = true;
                            bool _1584;
                            if (!_1573)
                            {
                                bool4 _1578 = isnan(_405);
                                bool4 _1579 = isinf(_405);
                                bool _1583 = !all(not(bool4(_1578.x || _1579.x, _1578.y || _1579.y, _1578.z || _1579.z, _1578.w || _1579.w)));
                                _409 = _1583;
                                _1584 = _1583;
                            }
                            else
                            {
                                _1584 = true;
                            }
                            bool _410 = true;
                            bool _1592;
                            if (!_1584)
                            {
                                bool _1591 = any(_404.xyz < float3(0.0));
                                _410 = _1591;
                                _1592 = _1591;
                            }
                            else
                            {
                                _1592 = true;
                            }
                            bool _411 = true;
                            bool _1600;
                            if (!_1592)
                            {
                                bool _1599 = any(_404.xyz > float3(65504.0));
                                _411 = _1599;
                                _1600 = _1599;
                            }
                            else
                            {
                                _1600 = true;
                            }
                            bool _412 = true;
                            bool _1608;
                            if (!_1600)
                            {
                                bool _1607 = any(_405.xyz < float3(0.0));
                                _412 = _1607;
                                _1608 = _1607;
                            }
                            else
                            {
                                _1608 = true;
                            }
                            bool _413 = true;
                            bool _1616;
                            if (!_1608)
                            {
                                bool _1615 = any(_405.xyz > float3(1.0));
                                _413 = _1615;
                                _1616 = _1615;
                            }
                            else
                            {
                                _1616 = true;
                            }
                            bool _414 = true;
                            bool _1625;
                            if (!_1616)
                            {
                                bool _1624 = !any(_405.xyz > float3(0.0));
                                _414 = _1624;
                                _1625 = _1624;
                            }
                            else
                            {
                                _1625 = true;
                            }
                            if (_1625)
                            {
                                _397 = true;
                                _398 = false;
                                _1704 = false;
                                break;
                            }
                        }
                        bool _415 = false;
                        bool _1673;
                        if (_399.w > 0.5)
                        {
                            uint _416 = current._m0[_1492];
                            float4 _417 = _399;
                            bool _418 = true;
                            _388 = false;
                            bool _1672;
                            do
                            {
                                uint _1639 = uint(rint(spvFMul(_417.z, 255.0)));
                                uint _390 = _1639;
                                bool _391 = true;
                                bool _1651;
                                if ((isunordered(_417.x, 0.0) || _417.x > 0.0))
                                {
                                    bool _392 = false;
                                    bool _1650;
                                    if (_1639 != 1u)
                                    {
                                        bool _1649 = _1639 != 2u;
                                        _392 = _1649;
                                        _1650 = _1649;
                                    }
                                    else
                                    {
                                        _1650 = false;
                                    }
                                    _391 = _1650;
                                    _1651 = _1650;
                                }
                                else
                                {
                                    _1651 = true;
                                }
                                if (_1651)
                                {
                                    _388 = true;
                                    _389 = false;
                                    _1672 = false;
                                    break;
                                }
                                uint _1654 = current._m0[_1492] >> 24u;
                                uint _393 = _1654;
                                if (_1654 == 253u)
                                {
                                    _388 = true;
                                    _389 = true;
                                    _1672 = true;
                                    break;
                                }
                                if (_417.x == 0.0039215688593685626983642578125)
                                {
                                    bool _1662 = _1654 == 254u;
                                    _388 = true;
                                    _389 = _1662;
                                    _1672 = _1662;
                                    break;
                                }
                                bool _1663 = _1654 == 255u;
                                bool _394 = false;
                                if (_1663)
                                {
                                    _394 = true;
                                }
                                bool _395 = false;
                                bool _1671;
                                if (_1663)
                                {
                                    bool _1670 = _417.y > 0.0;
                                    _395 = _1670;
                                    _1671 = _1670;
                                }
                                else
                                {
                                    _1671 = false;
                                }
                                _388 = true;
                                _389 = _1671;
                                _1672 = _1671;
                                break;
                            } while(false);
                            bool _396 = _1672;
                            _415 = _1672;
                            _1673 = _1672;
                        }
                        else
                        {
                            _1673 = false;
                        }
                        bool _419 = false;
                        bool _1682;
                        if (_1673)
                        {
                            bool _420 = true;
                            bool _1681;
                            if (!_651)
                            {
                                bool _1680 = (current._m0[_1492] >> 24u) == 255u;
                                _420 = _1680;
                                _1681 = _1680;
                            }
                            else
                            {
                                _1681 = true;
                            }
                            _419 = _1681;
                            _1682 = _1681;
                        }
                        else
                        {
                            _1682 = false;
                        }
                        bool _421 = false;
                        bool _1688;
                        if (_1682)
                        {
                            bool _1687 = _563.w == 1.0;
                            _421 = _1687;
                            _1688 = _1687;
                        }
                        else
                        {
                            _1688 = false;
                        }
                        bool _422 = false;
                        bool _1697;
                        if (_1688)
                        {
                            bool4 _1692 = isnan(_563);
                            bool4 _1693 = isinf(_563);
                            bool _1696 = all(not(bool4(_1692.x || _1693.x, _1692.y || _1693.y, _1692.z || _1693.z, _1692.w || _1693.w)));
                            _422 = _1696;
                            _1697 = _1696;
                        }
                        else
                        {
                            _1697 = false;
                        }
                        bool _423 = false;
                        bool _1703;
                        if (_1697)
                        {
                            bool _1702 = _563.z > 0.0;
                            _423 = _1702;
                            _1703 = _1702;
                        }
                        else
                        {
                            _1703 = false;
                        }
                        _397 = true;
                        _398 = _1703;
                        _1704 = _1703;
                        break;
                    } while(false);
                    bool _424 = _1704;
                    bool _1705 = !_1704;
                    _560 = _1705;
                    _1706 = _1705;
                }
                else
                {
                    _1706 = true;
                }
                if (_1706)
                {
                    _1452 = _1443;
                    _1454 = _1446;
                    continue;
                }
                uint _1715;
                if (_651)
                {
                    uint _1712 = ((_16 * (Settings.prefixStride + 48u)) + _138) >> 2u;
                    _565 = current._m0[_1712];
                    _1715 = current._m0[_1712];
                }
                else
                {
                    _565 = current._m0[_1492];
                    _1715 = current._m0[_1492];
                }
                uint _564 = _1715;
                uint _567 = _1715;
                float3 _209 = float3(float(_1715 & 255u), float((_1715 >> 8u) & 255u), float((_1715 >> 16u) & 255u)) / float3(255.0);
                float3 _384 = _209;
                float3 _1759;
                if (_1405)
                {
                    float3 _386 = _209;
                    float _1737;
                    if (_386.x <= 0.040449999272823333740234375)
                    {
                        float _210 = _386.x / 12.9200000762939453125;
                        _380 = _210;
                        _1737 = _210;
                    }
                    else
                    {
                        float _1736 = precise::powr(spvFAdd(_386.x, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                        _380 = _1736;
                        _1737 = _1736;
                    }
                    float _1747;
                    if (_386.y <= 0.040449999272823333740234375)
                    {
                        float _213 = _386.y / 12.9200000762939453125;
                        _381 = _213;
                        _1747 = _213;
                    }
                    else
                    {
                        float _1746 = precise::powr(spvFAdd(_386.y, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                        _381 = _1746;
                        _1747 = _1746;
                    }
                    float _1757;
                    if (_386.z <= 0.040449999272823333740234375)
                    {
                        float _216 = _386.z / 12.9200000762939453125;
                        _382 = _216;
                        _1757 = _216;
                    }
                    else
                    {
                        float _1756 = precise::powr(spvFAdd(_386.z, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                        _382 = _1756;
                        _1757 = _1756;
                    }
                    float3 _1758 = float3(_1737, _1747, _1757);
                    float3 _383 = _1758;
                    _385 = _1758;
                    _1759 = _1758;
                }
                else
                {
                    _385 = _209;
                    _1759 = _209;
                }
                float3 _387 = _1759;
                float3 _566 = _1759;
                float3 _1760 = precise::min(_1446, _1759);
                _550 = _1760;
                float3 _1761 = precise::max(_1443, _1759);
                _552 = _1761;
                _1452 = _1761;
                _1454 = _1760;
            }
        }
        uint _569 = _937;
        float3 _376 = _179;
        float3 _377;
        float3 _1796;
        if (_1405)
        {
            float3 _378 = _179;
            float _372;
            float _1774;
            if (_378.x <= 0.040449999272823333740234375)
            {
                float _219 = _378.x / 12.9200000762939453125;
                _372 = _219;
                _1774 = _219;
            }
            else
            {
                float _1773 = precise::powr(spvFAdd(_378.x, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                _372 = _1773;
                _1774 = _1773;
            }
            float _373;
            float _1784;
            if (_378.y <= 0.040449999272823333740234375)
            {
                float _222 = _378.y / 12.9200000762939453125;
                _373 = _222;
                _1784 = _222;
            }
            else
            {
                float _1783 = precise::powr(spvFAdd(_378.y, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                _373 = _1783;
                _1784 = _1783;
            }
            float _374;
            float _1794;
            if (_378.z <= 0.040449999272823333740234375)
            {
                float _225 = _378.z / 12.9200000762939453125;
                _374 = _225;
                _1794 = _225;
            }
            else
            {
                float _1793 = precise::powr(spvFAdd(_378.z, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                _374 = _1793;
                _1794 = _1793;
            }
            float3 _1795 = float3(_1774, _1784, _1794);
            float3 _375 = _1795;
            _377 = _1795;
            _1796 = _1795;
        }
        else
        {
            _377 = _179;
            _1796 = _179;
        }
        float3 _379 = _1796;
        float3 _1801 = mix(_1796, fast::clamp(_1368, _1445, _1442), float3(Settings.weight));
        float3 _568 = _1801;
        float3 _1858;
        if (_651)
        {
            float3 _570 = _1801;
            uint _571 = 4278190080u;
            float3 _368;
            float3 _1841;
            if (_1405)
            {
                float3 _369 = fast::clamp(_1801, float3(0.0), float3(1.0));
                float _364;
                float _1818;
                if (_369.x <= 0.003130800090730190277099609375)
                {
                    float _230 = spvFMul(_369.x, 12.9200000762939453125);
                    _364 = _230;
                    _1818 = _230;
                }
                else
                {
                    float _232 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_369.x, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                    _364 = _232;
                    _1818 = _232;
                }
                float _365;
                float _1828;
                if (_369.y <= 0.003130800090730190277099609375)
                {
                    float _233 = spvFMul(_369.y, 12.9200000762939453125);
                    _365 = _233;
                    _1828 = _233;
                }
                else
                {
                    float _235 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_369.y, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                    _365 = _235;
                    _1828 = _235;
                }
                float _366;
                float _1838;
                if (_369.z <= 0.003130800090730190277099609375)
                {
                    float _236 = spvFMul(_369.z, 12.9200000762939453125);
                    _366 = _236;
                    _1838 = _236;
                }
                else
                {
                    float _238 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_369.z, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                    _366 = _238;
                    _1838 = _238;
                }
                float3 _1839 = float3(_1818, _1828, _1838);
                float3 _367 = _1839;
                _368 = _1839;
                _1841 = _1839;
            }
            else
            {
                float3 _1840 = fast::clamp(_1801, float3(0.0), float3(1.0));
                _368 = _1840;
                _1841 = _1840;
            }
            _570 = _1841;
            uint3 _370 = uint3(floor(spvFAdd(_1841 * 255.0, float3(0.5))));
            uint _1854 = ((_370.x | (_370.y << 8u)) | (_370.z << 16u)) | 4278190080u;
            uint _371 = _1854;
            resolved._m0[((_16 * (Settings.prefixStride + 48u)) + _19) >> 2u] = _1854;
            float3 _148 = spvFAdd(_973.xyz, spvFMul(_1801, _972.xyz));
            _568 = _148;
            _1858 = _148;
        }
        else
        {
            _1858 = _1801;
        }
        float3 _572 = _1858;
        uint _1859 = _600 & 4278190080u;
        uint _573 = _1859;
        float3 _360;
        float3 _1896;
        if (_1405)
        {
            float3 _361 = fast::clamp(_1858, float3(0.0), float3(1.0));
            float _356;
            float _1873;
            if (_361.x <= 0.003130800090730190277099609375)
            {
                float _241 = spvFMul(_361.x, 12.9200000762939453125);
                _356 = _241;
                _1873 = _241;
            }
            else
            {
                float _243 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_361.x, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                _356 = _243;
                _1873 = _243;
            }
            float _357;
            float _1883;
            if (_361.y <= 0.003130800090730190277099609375)
            {
                float _244 = spvFMul(_361.y, 12.9200000762939453125);
                _357 = _244;
                _1883 = _244;
            }
            else
            {
                float _246 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_361.y, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                _357 = _246;
                _1883 = _246;
            }
            float _358;
            float _1893;
            if (_361.z <= 0.003130800090730190277099609375)
            {
                float _247 = spvFMul(_361.z, 12.9200000762939453125);
                _358 = _247;
                _1893 = _247;
            }
            else
            {
                float _249 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_361.z, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                _358 = _249;
                _1893 = _249;
            }
            float3 _1894 = float3(_1873, _1883, _1893);
            float3 _359 = _1894;
            _360 = _1894;
            _1896 = _1894;
        }
        else
        {
            float3 _1895 = fast::clamp(_1858, float3(0.0), float3(1.0));
            _360 = _1895;
            _1896 = _1895;
        }
        _572 = _1896;
        uint3 _362 = uint3(floor(spvFAdd(_1896 * 255.0, float3(0.5))));
        uint _1909 = ((_362.x | (_362.y << 8u)) | (_362.z << 16u)) | _1859;
        uint _363 = _1909;
        resolved._m0[_598] = _1909;
        _478 = true;
        break;
    } while(false);
}

