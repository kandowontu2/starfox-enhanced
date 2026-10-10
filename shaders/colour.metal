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

struct type_ColourSettings
{
    uint currentWidth;
    uint currentHeight;
    uint currentPrefix;
    uint currentRecords;
    uint currentTriangles;
    uint colourFirst;
    uint colourStride;
    uint colourOffset;
    uint colourBytes;
    uint colourFlags;
    uint colourCount;
    uint colourReserved;
    float colourWeight;
    uint colourLobes;
    uint colourPathStride;
    uint colourPad;
};

constant bool _338 = {};

kernel void feature_colour_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& current [[buffer(1)]], device type_ByteAddressBuffer& source [[buffer(2)]], device type_ByteAddressBuffer& queries [[buffer(3)]], device type_RWByteAddressBuffer& results [[buffer(4)]], constant type_ColourSettings& ColourSettings [[buffer(5)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        bool _358;
        if (gl_GlobalInvocationID.x < Settings.queryCount)
        {
            _358 = gl_GlobalInvocationID.x >= 64u;
        }
        else
        {
            _358 = true;
        }
        if (_358)
        {
            break;
        }
        uint _16 = gl_GlobalInvocationID.x * 24576u;
        uint _361 = (_16 + 384u) >> 2u;
        results._m0[_361] = 0u;
        uint _18 = _361 + 1u;
        results._m0[_18] = 0u;
        uint _19 = _361 + 2u;
        results._m0[_19] = 0u;
        uint _20 = _361 + 3u;
        results._m0[_20] = 0u;
        uint _366 = (_16 + 400u) >> 2u;
        results._m0[_366] = 0u;
        uint _214 = _366 + 1u;
        results._m0[_214] = 0u;
        uint _215 = _366 + 2u;
        results._m0[_215] = 0u;
        uint _216 = _366 + 3u;
        results._m0[_216] = 0u;
        uint _371 = (_16 + 416u) >> 2u;
        results._m0[_371] = 0u;
        uint _218 = _371 + 1u;
        results._m0[_218] = 0u;
        uint _219 = _371 + 2u;
        results._m0[_219] = 0u;
        uint _220 = _371 + 3u;
        results._m0[_220] = 0u;
        uint _376 = (_16 + 432u) >> 2u;
        results._m0[_376] = 0u;
        uint _222 = _376 + 1u;
        results._m0[_222] = 0u;
        uint _223 = _376 + 2u;
        results._m0[_223] = 0u;
        uint _224 = _376 + 3u;
        results._m0[_224] = 0u;
        uint _381 = (_16 + 448u) >> 2u;
        results._m0[_381] = 0u;
        uint _226 = _381 + 1u;
        results._m0[_226] = 0u;
        uint _227 = _381 + 2u;
        results._m0[_227] = 0u;
        uint _228 = _381 + 3u;
        results._m0[_228] = 0u;
        uint _386 = (_16 + 464u) >> 2u;
        results._m0[_386] = 0u;
        uint _230 = _386 + 1u;
        results._m0[_230] = 0u;
        uint _231 = _386 + 2u;
        results._m0[_231] = 0u;
        uint _232 = _386 + 3u;
        results._m0[_232] = 0u;
        bool _397;
        if (Settings.queryCount <= 64u)
        {
            _397 = ColourSettings.colourCount != Settings.queryCount;
        }
        else
        {
            _397 = true;
        }
        bool _404;
        if (!_397)
        {
            _404 = ColourSettings.colourStride != 24576u;
        }
        else
        {
            _404 = true;
        }
        bool _411;
        if (!_404)
        {
            _411 = ColourSettings.colourOffset != 384u;
        }
        else
        {
            _411 = true;
        }
        bool _418;
        if (!_411)
        {
            _418 = ColourSettings.colourBytes != 96u;
        }
        else
        {
            _418 = true;
        }
        bool _425;
        if (!_418)
        {
            _425 = ColourSettings.colourReserved != 0u;
        }
        else
        {
            _425 = true;
        }
        bool _432;
        if (!_425)
        {
            _432 = ColourSettings.colourPad != 0u;
        }
        else
        {
            _432 = true;
        }
        bool _439;
        if (!_432)
        {
            _439 = ColourSettings.colourFlags > 15u;
        }
        else
        {
            _439 = true;
        }
        bool _448;
        if (!_439)
        {
            _448 = ColourSettings.colourLobes != Settings.lobes;
        }
        else
        {
            _448 = true;
        }
        bool _457;
        if (!_448)
        {
            _457 = ColourSettings.colourPathStride != Settings.pathStride;
        }
        else
        {
            _457 = true;
        }
        bool _468;
        if (!_457)
        {
            bool _467;
            if (Settings.pathStride != 52u)
            {
                _467 = Settings.pathStride != 64u;
            }
            else
            {
                _467 = false;
            }
            _468 = _467;
        }
        else
        {
            _468 = true;
        }
        bool _479;
        if (!_468)
        {
            bool _478;
            if (Settings.lobes != 1u)
            {
                _478 = Settings.lobes != 8u;
            }
            else
            {
                _478 = false;
            }
            _479 = _478;
        }
        else
        {
            _479 = true;
        }
        bool _486;
        if (!_479)
        {
            _486 = Settings.width == 0u;
        }
        else
        {
            _486 = true;
        }
        bool _493;
        if (!_486)
        {
            _493 = Settings.height == 0u;
        }
        else
        {
            _493 = true;
        }
        bool _500;
        if (!_493)
        {
            _500 = ColourSettings.currentWidth == 0u;
        }
        else
        {
            _500 = true;
        }
        bool _507;
        if (!_500)
        {
            _507 = ColourSettings.currentHeight == 0u;
        }
        else
        {
            _507 = true;
        }
        bool _514;
        if (!_507)
        {
            _514 = Settings.width > 16384u;
        }
        else
        {
            _514 = true;
        }
        bool _521;
        if (!_514)
        {
            _521 = Settings.height > 16384u;
        }
        else
        {
            _521 = true;
        }
        bool _528;
        if (!_521)
        {
            _528 = ColourSettings.currentWidth > 16384u;
        }
        else
        {
            _528 = true;
        }
        bool _535;
        if (!_528)
        {
            _535 = ColourSettings.currentHeight > 16384u;
        }
        else
        {
            _535 = true;
        }
        bool _544;
        if (!_535)
        {
            _544 = isnan(ColourSettings.colourWeight) || isinf(ColourSettings.colourWeight);
        }
        else
        {
            _544 = true;
        }
        bool _551;
        if (!_544)
        {
            _551 = ColourSettings.colourWeight < 0.0;
        }
        else
        {
            _551 = true;
        }
        bool _558;
        if (!_551)
        {
            _558 = ColourSettings.colourWeight > 0.949999988079071044921875;
        }
        else
        {
            _558 = true;
        }
        bool _585;
        if (!_558)
        {
            bool _584;
            if ((ColourSettings.colourFlags & 2u) != 0u)
            {
                bool _576;
                if (Settings.pathStride == 52u)
                {
                    _576 = ColourSettings.currentPrefix != 4u;
                }
                else
                {
                    _576 = true;
                }
                bool _583;
                if (!_576)
                {
                    _583 = ColourSettings.currentRecords != 44u;
                }
                else
                {
                    _583 = true;
                }
                _584 = _583;
            }
            else
            {
                _584 = false;
            }
            _585 = _584;
        }
        else
        {
            _585 = true;
        }
        bool _607;
        if (!_585)
        {
            bool _606;
            if ((ColourSettings.colourFlags & 4u) != 0u)
            {
                bool _605;
                if (Settings.pathStride == 64u)
                {
                    _605 = ColourSettings.currentRecords != (ColourSettings.currentPrefix + 40u);
                }
                else
                {
                    _605 = true;
                }
                _606 = _605;
            }
            else
            {
                _606 = false;
            }
            _607 = _606;
        }
        else
        {
            _607 = true;
        }
        bool _627;
        if (!_607)
        {
            bool _626;
            if ((ColourSettings.colourFlags & 6u) == 0u)
            {
                bool _625;
                if (ColourSettings.currentPrefix == 4u)
                {
                    _625 = ColourSettings.currentRecords != 28u;
                }
                else
                {
                    _625 = true;
                }
                _626 = _625;
            }
            else
            {
                _626 = false;
            }
            _627 = _626;
        }
        else
        {
            _627 = true;
        }
        if (_627)
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 2u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        bool _640;
        if (results._m0[_16 >> 2u] == 1u)
        {
            _640 = results._m0[(_16 + 4u) >> 2u] != 1u;
        }
        else
        {
            _640 = true;
        }
        bool _648;
        if (!_640)
        {
            _648 = results._m0[(_16 + 12u) >> 2u] != 0u;
        }
        else
        {
            _648 = true;
        }
        bool _668;
        if (!_648)
        {
            uint _652 = (_16 + 192u) >> 2u;
            _668 = any(uint4(results._m0[_652], results._m0[_652 + 1u], results._m0[_652 + 2u], results._m0[_652 + 3u]) != uint4(1u, 31u, 0u, results._m0[(_16 + 204u) >> 2u]));
        }
        else
        {
            _668 = true;
        }
        bool _676;
        if (!_668)
        {
            _676 = results._m0[(_16 + 204u) >> 2u] == 0u;
        }
        else
        {
            _676 = true;
        }
        bool _684;
        if (!_676)
        {
            _684 = results._m0[(_16 + 256u) >> 2u] != 1u;
        }
        else
        {
            _684 = true;
        }
        bool _692;
        if (!_684)
        {
            _692 = results._m0[(_16 + 260u) >> 2u] != 0u;
        }
        else
        {
            _692 = true;
        }
        bool _708;
        if (!_692)
        {
            uint _696 = (_16 + 320u) >> 2u;
            _708 = any(uint4(results._m0[_696], results._m0[_696 + 1u], results._m0[_696 + 2u], results._m0[_696 + 3u]) != uint4(1u, 0u, 0u, 0u));
        }
        else
        {
            _708 = true;
        }
        if (_708)
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 1u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        uint _36 = ColourSettings.colourFirst + gl_GlobalInvocationID.x;
        uint _37 = _36 / Settings.lobes;
        uint _38 = _36 % Settings.lobes;
        bool _723;
        if (_36 >= ColourSettings.colourFirst)
        {
            _723 = _37 >= (ColourSettings.currentWidth * ColourSettings.currentHeight);
        }
        else
        {
            _723 = true;
        }
        bool _731;
        if (!_723)
        {
            _731 = queries._m0[((gl_GlobalInvocationID.x * 48u) + 44u) >> 2u] != _38;
        }
        else
        {
            _731 = true;
        }
        if (_731)
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 2u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        uint _107;
        uint _108;
        uint _737;
        uint _739;
        uint _741;
        uint _744;
        bool _892;
        do
        {
            _737 = ColourSettings.currentWidth;
            _739 = ColourSettings.currentHeight;
            _107 = _737 * _739;
            _741 = ColourSettings.currentPrefix;
            _108 = _107 * _741;
            uint _109 = _37 * 4u;
            _744 = current._m0[(_108 + _109) >> 2u];
            if (_744 == 4294967295u)
            {
                _892 = false;
                break;
            }
            uint _751 = current._m0[_109 >> 2u] >> 24u;
            bool _755 = (ColourSettings.colourFlags & 4u) != 0u;
            bool _757 = (ColourSettings.colourFlags & 2u) != 0u;
            bool _766;
            if (_755)
            {
                _766 = _744 == 4294967293u;
            }
            else
            {
                bool _765;
                if (_757)
                {
                    _765 = _744 == 4294967294u;
                }
                else
                {
                    _765 = false;
                }
                _766 = _765;
            }
            bool _784;
            if (_766)
            {
                bool _774;
                if (_755)
                {
                    _774 = (ColourSettings.colourFlags & 8u) == 0u;
                }
                else
                {
                    _774 = false;
                }
                _784 = _751 != (_774 ? 253u : 254u);
            }
            else
            {
                bool _783;
                if (_744 < ColourSettings.currentTriangles)
                {
                    _783 = _751 != 255u;
                }
                else
                {
                    _783 = true;
                }
                _784 = _783;
            }
            if (_784)
            {
                _892 = false;
                break;
            }
            float _790 = as_type<float>(current._m0[((_107 * (_741 + 4u)) + _109) >> 2u]);
            uint _116 = _37 * 16u;
            uint _791 = ((_107 * (_741 + 8u)) + _116) >> 2u;
            float4 _801 = as_type<float4>(uint4(current._m0[_791], current._m0[_791 + 1u], current._m0[_791 + 2u], current._m0[_791 + 3u]));
            bool _809;
            if (!(isnan(_790) || isinf(_790)))
            {
                _809 = _790 <= 0.0;
            }
            else
            {
                _809 = true;
            }
            bool _815;
            if (!_809)
            {
                _815 = _801.w != 1.0;
            }
            else
            {
                _815 = true;
            }
            bool _825;
            if (!_815)
            {
                bool4 _819 = isnan(_801);
                bool4 _820 = isinf(_801);
                _825 = !all(not(bool4(_819.x || _820.x, _819.y || _820.y, _819.z || _820.z, _819.w || _820.w)));
            }
            else
            {
                _825 = true;
            }
            bool _832;
            if (!_825)
            {
                _832 = any(_801.xyz < float3(0.0));
            }
            else
            {
                _832 = true;
            }
            bool _839;
            if (!_832)
            {
                _839 = any(_801.xyz > float3(1.0));
            }
            else
            {
                _839 = true;
            }
            bool _847;
            if (!_839)
            {
                _847 = !any(_801.xyz > float3(0.0));
            }
            else
            {
                _847 = true;
            }
            if (_847)
            {
                _892 = false;
                break;
            }
            if ((!_755) ? _757 : true)
            {
                uint _854 = ((_107 * (_741 + 24u)) + _116) >> 2u;
                float4 _864 = as_type<float4>(uint4(current._m0[_854], current._m0[_854 + 1u], current._m0[_854 + 2u], current._m0[_854 + 3u]));
                float _865 = _864.w;
                bool _875;
                if ((isunordered(_865, 1.0) || _865 == 1.0))
                {
                    bool4 _869 = isnan(_864);
                    bool4 _870 = isinf(_864);
                    _875 = !all(not(bool4(_869.x || _870.x, _869.y || _870.y, _869.z || _870.z, _869.w || _870.w)));
                }
                else
                {
                    _875 = true;
                }
                bool _882;
                if (!_875)
                {
                    _882 = any(_864.xyz < float3(0.0));
                }
                else
                {
                    _882 = true;
                }
                bool _889;
                if (!_882)
                {
                    _889 = any(_864.xyz > float3(999999995904.0));
                }
                else
                {
                    _889 = true;
                }
                if (_889)
                {
                    _892 = false;
                    break;
                }
            }
            _892 = true;
            break;
        } while(false);
        if (!_892)
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 3u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        uint _42 = _107 * ColourSettings.currentRecords;
        uint _44 = _42 + (_36 * Settings.pathStride);
        uint _900 = _44 >> 2u;
        uint _902 = current._m0[_900];
        uint _904 = current._m0[_900 + 1u];
        uint _906 = current._m0[_900 + 2u];
        uint _908 = current._m0[_900 + 3u];
        uint4 _909 = uint4(_902, _904, _906, _908);
        uint _910 = (_44 + 16u) >> 2u;
        uint _912 = current._m0[_910];
        uint _913 = (_44 + 20u) >> 2u;
        uint _915 = current._m0[_913];
        uint _916 = (_44 + 24u) >> 2u;
        float3 _924 = as_type<float3>(uint3(current._m0[_916], current._m0[_916 + 1u], current._m0[_916 + 2u]));
        uint _925 = (_44 + 36u) >> 2u;
        uint _927 = current._m0[_925];
        uint _928 = (_44 + 40u) >> 2u;
        float3 _936 = as_type<float3>(uint3(current._m0[_928], current._m0[_928 + 1u], current._m0[_928 + 2u]));
        bool _937 = Settings.pathStride == 64u;
        float3 _950;
        if (_937)
        {
            uint _941 = (_44 + 52u) >> 2u;
            _950 = as_type<float3>(uint3(current._m0[_941], current._m0[_941 + 1u], current._m0[_941 + 2u]));
        }
        else
        {
            _950 = float3(0.0);
        }
        uint4 _347 = _909;
        bool _1141;
        do
        {
            uint _953 = _912 & 7u;
            uint _954 = _912 >> 8u;
            uint _960;
            if (_937)
            {
                _960 = (_912 >> 4u) & 15u;
            }
            else
            {
                _960 = 0u;
            }
            bool _965;
            if (_953 <= 4u)
            {
                _965 = _954 < 1u;
            }
            else
            {
                _965 = true;
            }
            bool _970;
            if (!_965)
            {
                _970 = _954 > 3u;
            }
            else
            {
                _970 = true;
            }
            bool _979;
            if (!_970)
            {
                _979 = _912 != (((_954 << 8u) | (_960 << 4u)) | _953);
            }
            else
            {
                _979 = true;
            }
            bool _985;
            if (!_979)
            {
                _985 = (_960 >> _953) != 0u;
            }
            else
            {
                _985 = true;
            }
            bool _996;
            if (!_985)
            {
                bool _995;
                if (_954 == 3u)
                {
                    _995 = _953 != 4u;
                }
                else
                {
                    _995 = _953 == 4u;
                }
                _996 = _995;
            }
            else
            {
                _996 = true;
            }
            bool _1002;
            if (!_996)
            {
                _1002 = (_927 >> 24u) != 255u;
            }
            else
            {
                _1002 = true;
            }
            bool _1012;
            if (!_1002)
            {
                bool3 _1006 = isnan(_924);
                bool3 _1007 = isinf(_924);
                _1012 = !all(not(bool3(_1006.x || _1007.x, _1006.y || _1007.y, _1006.z || _1007.z)));
            }
            else
            {
                _1012 = true;
            }
            bool _1022;
            if (!_1012)
            {
                bool3 _1016 = isnan(_936);
                bool3 _1017 = isinf(_936);
                _1022 = !all(not(bool3(_1016.x || _1017.x, _1016.y || _1017.y, _1016.z || _1017.z)));
            }
            else
            {
                _1022 = true;
            }
            bool _1032;
            if (!_1022)
            {
                bool3 _1026 = isnan(_950);
                bool3 _1027 = isinf(_950);
                _1032 = !all(not(bool3(_1026.x || _1027.x, _1026.y || _1027.y, _1026.z || _1027.z)));
            }
            else
            {
                _1032 = true;
            }
            bool _1038;
            if (!_1032)
            {
                _1038 = any(_936 < float3(0.0));
            }
            else
            {
                _1038 = true;
            }
            bool _1044;
            if (!_1038)
            {
                _1044 = any(_936 > float3(1.0));
            }
            else
            {
                _1044 = true;
            }
            bool _1050;
            if (!_1044)
            {
                _1050 = any(_950 < float3(0.0));
            }
            else
            {
                _1050 = true;
            }
            bool _1056;
            if (!_1050)
            {
                _1056 = any(_950 > float3(999999995904.0));
            }
            else
            {
                _1056 = true;
            }
            if (_1056)
            {
                _1141 = false;
                break;
            }
            bool _1110;
            bool _1111;
            uint _1060 = 0u;
            for (;;)
            {
                if (_1060 < 4u)
                {
                    if (_1060 >= _953)
                    {
                        if (_347[_1060] != 4294967295u)
                        {
                            _1110 = false;
                            _1111 = true;
                            break;
                        }
                    }
                    else
                    {
                        if ((_960 & (1u << (_1060 & 31u))) != 0u)
                        {
                            if (_347[_1060] != 4294967293u)
                            {
                                _1110 = false;
                                _1111 = true;
                                break;
                            }
                        }
                        else
                        {
                            bool _1107;
                            if (_347[_1060] >= ColourSettings.currentTriangles)
                            {
                                bool _1100;
                                if (Settings.pathStride == 52u)
                                {
                                    _1100 = (ColourSettings.colourFlags & 2u) != 0u;
                                }
                                else
                                {
                                    _1100 = false;
                                }
                                bool _1105;
                                if (_1100)
                                {
                                    _1105 = _347[_1060] == 4294967294u;
                                }
                                else
                                {
                                    _1105 = false;
                                }
                                _1107 = !_1105;
                            }
                            else
                            {
                                _1107 = false;
                            }
                            if (_1107)
                            {
                                _1110 = false;
                                _1111 = true;
                                break;
                            }
                        }
                    }
                    _1060++;
                    continue;
                }
                else
                {
                    _1110 = _338;
                    _1111 = false;
                    break;
                }
            }
            if (_1111)
            {
                _1141 = _1110;
                break;
            }
            if (_954 == 1u)
            {
                bool _1123;
                if (_915 < ColourSettings.currentTriangles)
                {
                    _1123 = _924.z == 0.0;
                }
                else
                {
                    _1123 = false;
                }
                bool _1129;
                if (_1123)
                {
                    _1129 = all(_924.xy >= float2(0.0));
                }
                else
                {
                    _1129 = false;
                }
                bool _1134;
                if (_1129)
                {
                    _1134 = dot(_924.xy, float2(1.0)) <= 1.0;
                }
                else
                {
                    _1134 = false;
                }
                _1141 = _1134;
                break;
            }
            bool _1140;
            if (_915 == 4294967295u)
            {
                _1140 = abs(spvFSub(dot(_924, _924), 1.0)) < 9.9999997473787516355514526367188e-05;
            }
            else
            {
                _1140 = false;
            }
            _1141 = _1140;
            break;
        } while(false);
        bool _1148;
        if (_1141)
        {
            _1148 = _912 != queries._m0[((gl_GlobalInvocationID.x * 48u) + 20u) >> 2u];
        }
        else
        {
            _1148 = true;
        }
        if (_1148)
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 3u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        uint _1151 = (_16 + 352u) >> 2u;
        uint _1153 = results._m0[_1151];
        uint _1155 = results._m0[_1151 + 1u];
        uint _1157 = results._m0[_1151 + 2u];
        uint _1159 = results._m0[_1151 + 3u];
        float4 _1161 = as_type<float4>(uint4(_1153, _1155, _1157, _1159));
        uint _1162 = (_16 + 336u) >> 2u;
        float4 _1172 = as_type<float4>(uint4(results._m0[_1162], results._m0[_1162 + 1u], results._m0[_1162 + 2u], results._m0[_1162 + 3u]));
        float _1173 = _1161.w;
        bool _1183;
        if ((isunordered(_1173, 1.0) || _1173 == 1.0))
        {
            bool4 _1177 = isnan(_1161);
            bool4 _1178 = isinf(_1161);
            _1183 = !all(not(bool4(_1177.x || _1178.x, _1177.y || _1178.y, _1177.z || _1178.z, _1177.w || _1178.w)));
        }
        else
        {
            _1183 = true;
        }
        bool _1193;
        if (!_1183)
        {
            bool4 _1187 = isnan(_1172);
            bool4 _1188 = isinf(_1172);
            _1193 = !all(not(bool4(_1187.x || _1188.x, _1187.y || _1188.y, _1187.z || _1188.z, _1187.w || _1188.w)));
        }
        else
        {
            _1193 = true;
        }
        bool _1201;
        if (!_1193)
        {
            _1201 = any(_1161.xy < _1172.xy);
        }
        else
        {
            _1201 = true;
        }
        bool _1209;
        if (!_1201)
        {
            _1209 = any(_1161.xy > _1172.zw);
        }
        else
        {
            _1209 = true;
        }
        bool _1216;
        if (!_1209)
        {
            _1216 = any(_1161.xy < float2(0.5));
        }
        else
        {
            _1216 = true;
        }
        bool _1230;
        if (!_1216)
        {
            _1230 = any(_1161.xy > spvFSub(float2(float(Settings.width), float(Settings.height)), float2(0.5)));
        }
        else
        {
            _1230 = true;
        }
        if (_1230)
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 2u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        float3 _146 = float3(float(_927 & 255u), float((_927 >> 8u) & 255u), float((_927 >> 16u) & 255u)) / float3(255.0);
        bool _1245 = (ColourSettings.colourFlags & 1u) != 0u;
        float3 _1271;
        if (_1245)
        {
            float _1249 = _146.x;
            float _1255;
            if (_1249 <= 0.040449999272823333740234375)
            {
                _1255 = _1249 / 12.9200000762939453125;
            }
            else
            {
                _1255 = precise::powr(spvFAdd(_1249, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
            }
            float _1256 = _146.y;
            float _1262;
            if (_1256 <= 0.040449999272823333740234375)
            {
                _1262 = _1256 / 12.9200000762939453125;
            }
            else
            {
                _1262 = precise::powr(spvFAdd(_1256, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
            }
            float _1263 = _146.z;
            float _1269;
            if (_1263 <= 0.040449999272823333740234375)
            {
                _1269 = _1263 / 12.9200000762939453125;
            }
            else
            {
                _1269 = precise::powr(spvFAdd(_1263, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
            }
            _1271 = float3(_1255, _1262, _1269);
        }
        else
        {
            _1271 = _146;
        }
        int2 _1274 = int2(int(_37 % _737), int(_37 / _737));
        float3 _1281;
        float3 _1283;
        _1281 = _1271;
        _1283 = _1271;
        bool _1277;
        uint _1280;
        float3 _1282;
        float3 _1284;
        bool _1286;
        float3 _1778;
        float3 _1779;
        uint _1780;
        bool _1781;
        bool _1276 = false;
        uint _1279 = 0u;
        bool _1285;
        int _1287 = -1;
        for (;;)
        {
            if (_1287 <= 1)
            {
                _1280 = _1279;
                _1282 = _1281;
                _1284 = _1283;
                uint _1292;
                float3 _1294;
                float3 _1295;
                bool _1297;
                bool _1296 = _1285;
                int _1298 = -1;
                for (;;)
                {
                    if (_1298 <= 1)
                    {
                        int2 _1302 = int2(_1298, _1287);
                        int2 _58 = _1274 + _1302;
                        bool _1313;
                        if (!any(_58 < int2(0)))
                        {
                            _1313 = any(_58 >= int2(int(_737), int(_739)));
                        }
                        else
                        {
                            _1313 = true;
                        }
                        if (_1313)
                        {
                            _1292 = _1280;
                            _1294 = _1282;
                            _1295 = _1284;
                            _1297 = _1296;
                            int _66 = _1298 + 1;
                            _1280 = _1292;
                            _1282 = _1294;
                            _1284 = _1295;
                            _1296 = _1297;
                            _1298 = _66;
                            continue;
                        }
                        uint _1324;
                        uint _60 = (uint(_58.y) * _737) + uint(_58.x);
                        bool _1470;
                        do
                        {
                            uint _156 = _60 * 4u;
                            _1324 = current._m0[(_108 + _156) >> 2u];
                            if (_1324 == 4294967295u)
                            {
                                _1470 = false;
                                break;
                            }
                            uint _1331 = current._m0[_156 >> 2u] >> 24u;
                            bool _1333 = (ColourSettings.colourFlags & 4u) != 0u;
                            bool _1335 = (ColourSettings.colourFlags & 2u) != 0u;
                            bool _1344;
                            if (_1333)
                            {
                                _1344 = _1324 == 4294967293u;
                            }
                            else
                            {
                                bool _1343;
                                if (_1335)
                                {
                                    _1343 = _1324 == 4294967294u;
                                }
                                else
                                {
                                    _1343 = false;
                                }
                                _1344 = _1343;
                            }
                            bool _1362;
                            if (_1344)
                            {
                                bool _1352;
                                if (_1333)
                                {
                                    _1352 = (ColourSettings.colourFlags & 8u) == 0u;
                                }
                                else
                                {
                                    _1352 = false;
                                }
                                _1362 = _1331 != (_1352 ? 253u : 254u);
                            }
                            else
                            {
                                bool _1361;
                                if (_1324 < ColourSettings.currentTriangles)
                                {
                                    _1361 = _1331 != 255u;
                                }
                                else
                                {
                                    _1361 = true;
                                }
                                _1362 = _1361;
                            }
                            if (_1362)
                            {
                                _1470 = false;
                                break;
                            }
                            float _1368 = as_type<float>(current._m0[((_107 * (_741 + 4u)) + _156) >> 2u]);
                            uint _163 = _60 * 16u;
                            uint _1369 = ((_107 * (_741 + 8u)) + _163) >> 2u;
                            float4 _1379 = as_type<float4>(uint4(current._m0[_1369], current._m0[_1369 + 1u], current._m0[_1369 + 2u], current._m0[_1369 + 3u]));
                            bool _1387;
                            if (!(isnan(_1368) || isinf(_1368)))
                            {
                                _1387 = _1368 <= 0.0;
                            }
                            else
                            {
                                _1387 = true;
                            }
                            bool _1393;
                            if (!_1387)
                            {
                                _1393 = _1379.w != 1.0;
                            }
                            else
                            {
                                _1393 = true;
                            }
                            bool _1403;
                            if (!_1393)
                            {
                                bool4 _1397 = isnan(_1379);
                                bool4 _1398 = isinf(_1379);
                                _1403 = !all(not(bool4(_1397.x || _1398.x, _1397.y || _1398.y, _1397.z || _1398.z, _1397.w || _1398.w)));
                            }
                            else
                            {
                                _1403 = true;
                            }
                            bool _1410;
                            if (!_1403)
                            {
                                _1410 = any(_1379.xyz < float3(0.0));
                            }
                            else
                            {
                                _1410 = true;
                            }
                            bool _1417;
                            if (!_1410)
                            {
                                _1417 = any(_1379.xyz > float3(1.0));
                            }
                            else
                            {
                                _1417 = true;
                            }
                            bool _1425;
                            if (!_1417)
                            {
                                _1425 = !any(_1379.xyz > float3(0.0));
                            }
                            else
                            {
                                _1425 = true;
                            }
                            if (_1425)
                            {
                                _1470 = false;
                                break;
                            }
                            if ((!_1333) ? _1335 : true)
                            {
                                uint _1432 = ((_107 * (_741 + 24u)) + _163) >> 2u;
                                float4 _1442 = as_type<float4>(uint4(current._m0[_1432], current._m0[_1432 + 1u], current._m0[_1432 + 2u], current._m0[_1432 + 3u]));
                                float _1443 = _1442.w;
                                bool _1453;
                                if ((isunordered(_1443, 1.0) || _1443 == 1.0))
                                {
                                    bool4 _1447 = isnan(_1442);
                                    bool4 _1448 = isinf(_1442);
                                    _1453 = !all(not(bool4(_1447.x || _1448.x, _1447.y || _1448.y, _1447.z || _1448.z, _1447.w || _1448.w)));
                                }
                                else
                                {
                                    _1453 = true;
                                }
                                bool _1460;
                                if (!_1453)
                                {
                                    _1460 = any(_1442.xyz < float3(0.0));
                                }
                                else
                                {
                                    _1460 = true;
                                }
                                bool _1467;
                                if (!_1460)
                                {
                                    _1467 = any(_1442.xyz > float3(999999995904.0));
                                }
                                else
                                {
                                    _1467 = true;
                                }
                                if (_1467)
                                {
                                    _1470 = false;
                                    break;
                                }
                            }
                            _1470 = true;
                            break;
                        } while(false);
                        bool _1474;
                        if (_1470)
                        {
                            _1474 = _1324 != _744;
                        }
                        else
                        {
                            _1474 = true;
                        }
                        if (_1474)
                        {
                            _1292 = _1280;
                            _1294 = _1282;
                            _1295 = _1284;
                            _1297 = _1296;
                            int _66 = _1298 + 1;
                            _1280 = _1292;
                            _1282 = _1294;
                            _1284 = _1295;
                            _1296 = _1297;
                            _1298 = _66;
                            continue;
                        }
                        uint _64 = _42 + (((_60 * Settings.lobes) + _38) * Settings.pathStride);
                        uint _1477 = _64 >> 2u;
                        uint4 _1486 = uint4(current._m0[_1477], current._m0[_1477 + 1u], current._m0[_1477 + 2u], current._m0[_1477 + 3u]);
                        uint _1487 = (_64 + 16u) >> 2u;
                        uint _1490 = (_64 + 20u) >> 2u;
                        uint _1493 = (_64 + 24u) >> 2u;
                        float3 _1501 = as_type<float3>(uint3(current._m0[_1493], current._m0[_1493 + 1u], current._m0[_1493 + 2u]));
                        uint _1502 = (_64 + 36u) >> 2u;
                        uint _1505 = (_64 + 40u) >> 2u;
                        float3 _1513 = as_type<float3>(uint3(current._m0[_1505], current._m0[_1505 + 1u], current._m0[_1505 + 2u]));
                        float3 _1526;
                        if (_937)
                        {
                            uint _1517 = (_64 + 52u) >> 2u;
                            _1526 = as_type<float3>(uint3(current._m0[_1517], current._m0[_1517 + 1u], current._m0[_1517 + 2u]));
                        }
                        else
                        {
                            _1526 = float3(0.0);
                        }
                        uint4 _346 = _1486;
                        bool _1715;
                        do
                        {
                            uint _1529 = current._m0[_1487] & 7u;
                            uint _1530 = current._m0[_1487] >> 8u;
                            uint _1536;
                            if (_937)
                            {
                                _1536 = (current._m0[_1487] >> 4u) & 15u;
                            }
                            else
                            {
                                _1536 = 0u;
                            }
                            bool _1541;
                            if (_1529 <= 4u)
                            {
                                _1541 = _1530 < 1u;
                            }
                            else
                            {
                                _1541 = true;
                            }
                            bool _1546;
                            if (!_1541)
                            {
                                _1546 = _1530 > 3u;
                            }
                            else
                            {
                                _1546 = true;
                            }
                            bool _1555;
                            if (!_1546)
                            {
                                _1555 = current._m0[_1487] != (((_1530 << 8u) | (_1536 << 4u)) | _1529);
                            }
                            else
                            {
                                _1555 = true;
                            }
                            bool _1561;
                            if (!_1555)
                            {
                                _1561 = (_1536 >> _1529) != 0u;
                            }
                            else
                            {
                                _1561 = true;
                            }
                            bool _1572;
                            if (!_1561)
                            {
                                bool _1571;
                                if (_1530 == 3u)
                                {
                                    _1571 = _1529 != 4u;
                                }
                                else
                                {
                                    _1571 = _1529 == 4u;
                                }
                                _1572 = _1571;
                            }
                            else
                            {
                                _1572 = true;
                            }
                            bool _1578;
                            if (!_1572)
                            {
                                _1578 = (current._m0[_1502] >> 24u) != 255u;
                            }
                            else
                            {
                                _1578 = true;
                            }
                            bool _1588;
                            if (!_1578)
                            {
                                bool3 _1582 = isnan(_1501);
                                bool3 _1583 = isinf(_1501);
                                _1588 = !all(not(bool3(_1582.x || _1583.x, _1582.y || _1583.y, _1582.z || _1583.z)));
                            }
                            else
                            {
                                _1588 = true;
                            }
                            bool _1598;
                            if (!_1588)
                            {
                                bool3 _1592 = isnan(_1513);
                                bool3 _1593 = isinf(_1513);
                                _1598 = !all(not(bool3(_1592.x || _1593.x, _1592.y || _1593.y, _1592.z || _1593.z)));
                            }
                            else
                            {
                                _1598 = true;
                            }
                            bool _1608;
                            if (!_1598)
                            {
                                bool3 _1602 = isnan(_1526);
                                bool3 _1603 = isinf(_1526);
                                _1608 = !all(not(bool3(_1602.x || _1603.x, _1602.y || _1603.y, _1602.z || _1603.z)));
                            }
                            else
                            {
                                _1608 = true;
                            }
                            bool _1614;
                            if (!_1608)
                            {
                                _1614 = any(_1513 < float3(0.0));
                            }
                            else
                            {
                                _1614 = true;
                            }
                            bool _1620;
                            if (!_1614)
                            {
                                _1620 = any(_1513 > float3(1.0));
                            }
                            else
                            {
                                _1620 = true;
                            }
                            bool _1626;
                            if (!_1620)
                            {
                                _1626 = any(_1526 < float3(0.0));
                            }
                            else
                            {
                                _1626 = true;
                            }
                            bool _1632;
                            if (!_1626)
                            {
                                _1632 = any(_1526 > float3(999999995904.0));
                            }
                            else
                            {
                                _1632 = true;
                            }
                            if (_1632)
                            {
                                _1715 = false;
                                break;
                            }
                            bool _1684;
                            bool _1685;
                            uint _1636 = 0u;
                            for (;;)
                            {
                                if (_1636 < 4u)
                                {
                                    if (_1636 >= _1529)
                                    {
                                        if (_346[_1636] != 4294967295u)
                                        {
                                            _1684 = false;
                                            _1685 = true;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        if ((_1536 & (1u << (_1636 & 31u))) != 0u)
                                        {
                                            if (_346[_1636] != 4294967293u)
                                            {
                                                _1684 = false;
                                                _1685 = true;
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            bool _1681;
                                            if (_346[_1636] >= ColourSettings.currentTriangles)
                                            {
                                                bool _1674;
                                                if (Settings.pathStride == 52u)
                                                {
                                                    _1674 = (ColourSettings.colourFlags & 2u) != 0u;
                                                }
                                                else
                                                {
                                                    _1674 = false;
                                                }
                                                bool _1679;
                                                if (_1674)
                                                {
                                                    _1679 = _346[_1636] == 4294967294u;
                                                }
                                                else
                                                {
                                                    _1679 = false;
                                                }
                                                _1681 = !_1679;
                                            }
                                            else
                                            {
                                                _1681 = false;
                                            }
                                            if (_1681)
                                            {
                                                _1684 = false;
                                                _1685 = true;
                                                break;
                                            }
                                        }
                                    }
                                    _1636++;
                                    continue;
                                }
                                else
                                {
                                    _1684 = _1296;
                                    _1685 = false;
                                    break;
                                }
                            }
                            if (_1685)
                            {
                                _1715 = _1684;
                                break;
                            }
                            if (_1530 == 1u)
                            {
                                bool _1697;
                                if (current._m0[_1490] < ColourSettings.currentTriangles)
                                {
                                    _1697 = _1501.z == 0.0;
                                }
                                else
                                {
                                    _1697 = false;
                                }
                                bool _1703;
                                if (_1697)
                                {
                                    _1703 = all(_1501.xy >= float2(0.0));
                                }
                                else
                                {
                                    _1703 = false;
                                }
                                bool _1708;
                                if (_1703)
                                {
                                    _1708 = dot(_1501.xy, float2(1.0)) <= 1.0;
                                }
                                else
                                {
                                    _1708 = false;
                                }
                                _1715 = _1708;
                                break;
                            }
                            bool _1714;
                            if (current._m0[_1490] == 4294967295u)
                            {
                                _1714 = abs(spvFSub(dot(_1501, _1501), 1.0)) < 9.9999997473787516355514526367188e-05;
                            }
                            else
                            {
                                _1714 = false;
                            }
                            _1715 = _1714;
                            break;
                        } while(false);
                        bool _1729;
                        if (_1715)
                        {
                            bool _1723;
                            if (all(_1486 == _909))
                            {
                                _1723 = current._m0[_1487] == _912;
                            }
                            else
                            {
                                _1723 = false;
                            }
                            bool _1727;
                            if (_1723)
                            {
                                _1727 = current._m0[_1490] == _915;
                            }
                            else
                            {
                                _1727 = false;
                            }
                            _1729 = !_1727;
                        }
                        else
                        {
                            _1729 = true;
                        }
                        if (_1729)
                        {
                            _1292 = _1280;
                            _1294 = _1282;
                            _1295 = _1284;
                            _1297 = _1715;
                            int _66 = _1298 + 1;
                            _1280 = _1292;
                            _1282 = _1294;
                            _1284 = _1295;
                            _1296 = _1297;
                            _1298 = _66;
                            continue;
                        }
                        float3 _193 = float3(float(current._m0[_1502] & 255u), float((current._m0[_1502] >> 8u) & 255u), float((current._m0[_1502] >> 16u) & 255u)) / float3(255.0);
                        float3 _1766;
                        if (_1245)
                        {
                            float _1744 = _193.x;
                            float _1750;
                            if (_1744 <= 0.040449999272823333740234375)
                            {
                                _1750 = _1744 / 12.9200000762939453125;
                            }
                            else
                            {
                                _1750 = precise::powr(spvFAdd(_1744, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                            }
                            float _1751 = _193.y;
                            float _1757;
                            if (_1751 <= 0.040449999272823333740234375)
                            {
                                _1757 = _1751 / 12.9200000762939453125;
                            }
                            else
                            {
                                _1757 = precise::powr(spvFAdd(_1751, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                            }
                            float _1758 = _193.z;
                            float _1764;
                            if (_1758 <= 0.040449999272823333740234375)
                            {
                                _1764 = _1758 / 12.9200000762939453125;
                            }
                            else
                            {
                                _1764 = precise::powr(spvFAdd(_1758, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                            }
                            _1766 = float3(_1750, _1757, _1764);
                        }
                        else
                        {
                            _1766 = _193;
                        }
                        bool3 _1767 = isnan(_1766);
                        bool3 _1768 = isinf(_1766);
                        if (!all(not(bool3(_1767.x || _1768.x, _1767.y || _1768.y, _1767.z || _1768.z))))
                        {
                            results._m0[_361] = 0u;
                            results._m0[_18] = 6u;
                            results._m0[_19] = 0u;
                            results._m0[_20] = 0u;
                            _1286 = _1715;
                            _1277 = true;
                            break;
                        }
                        _1292 = _1280 + 1u;
                        _1294 = precise::max(_1282, _1766);
                        _1295 = precise::min(_1284, _1766);
                        _1297 = _1715;
                        int _66 = _1298 + 1;
                        _1280 = _1292;
                        _1282 = _1294;
                        _1284 = _1295;
                        _1296 = _1297;
                        _1298 = _66;
                        continue;
                    }
                    else
                    {
                        _1286 = _1296;
                        _1277 = _1276;
                        break;
                    }
                }
                if (_1277)
                {
                    _1778 = _1282;
                    _1779 = _1284;
                    _1780 = _1280;
                    _1781 = _1277;
                    break;
                }
                _1276 = _1277;
                _1279 = _1280;
                _1281 = _1282;
                _1283 = _1284;
                _1285 = _1286;
                _1287++;
                continue;
            }
            else
            {
                _1778 = _1281;
                _1779 = _1283;
                _1780 = _1279;
                _1781 = _1276;
                break;
            }
        }
        if (_1781)
        {
            break;
        }
        bool _1792;
        if (_1780 != 0u)
        {
            bool3 _1786 = isnan(_1271);
            bool3 _1787 = isinf(_1271);
            _1792 = !all(not(bool3(_1786.x || _1787.x, _1786.y || _1787.y, _1786.z || _1787.z)));
        }
        else
        {
            _1792 = true;
        }
        if (_1792)
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 6u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        float2 _68 = spvFSub(_1161.xy, float2(0.5));
        int2 _1797 = int2(floor(_68));
        float2 _69 = spvFSub(_68, float2(_1797));
        uint _70 = gl_GlobalInvocationID.x * 48u;
        uint _1799 = _70 >> 2u;
        uint _1801 = queries._m0[_1799];
        uint _1802 = (_70 + 20u) >> 2u;
        uint _1804 = queries._m0[_1802];
        uint _1805 = (_70 + 24u) >> 2u;
        uint _1807 = queries._m0[_1805];
        uint _1808 = (_70 + 4u) >> 2u;
        uint _1810 = queries._m0[_1808];
        uint _1812 = queries._m0[_1808 + 1u];
        uint _1814 = queries._m0[_1808 + 2u];
        uint _1816 = queries._m0[_1808 + 3u];
        uint4 _1817 = uint4(_1810, _1812, _1814, _1816);
        float3 _1824;
        _1824 = float3(0.0);
        bool _1820;
        float _1823;
        float3 _1825;
        float _1971;
        float3 _1972;
        bool _1973;
        bool _1819 = _1781;
        float _1822 = 0.0;
        uint _1826 = 0u;
        for (;;)
        {
            if (_1826 < 2u)
            {
                _1823 = _1822;
                _1825 = _1824;
                float _1831;
                float3 _1833;
                uint _1834 = 0u;
                for (;;)
                {
                    if (_1834 < 2u)
                    {
                        float _1844;
                        if (_1834 != 0u)
                        {
                            _1844 = _69.x;
                        }
                        else
                        {
                            _1844 = spvFSub(1.0, _69.x);
                        }
                        float _1851;
                        if (_1826 != 0u)
                        {
                            _1851 = _69.y;
                        }
                        else
                        {
                            _1851 = spvFSub(1.0, _69.y);
                        }
                        float _79 = spvFMul(_1844, _1851);
                        if (_79 <= 0.0)
                        {
                            _1831 = _1823;
                            _1833 = _1825;
                            uint _104 = _1834 + 1u;
                            _1823 = _1831;
                            _1825 = _1833;
                            _1834 = _104;
                            continue;
                        }
                        int2 _1865 = min((_1797 + int2(int(_1834), int(_1826))), int2(int(Settings.width - 1u), int(Settings.height - 1u)));
                        if (any(_1865 < int2(0)))
                        {
                            results._m0[_361] = 0u;
                            results._m0[_18] = 4u;
                            results._m0[_19] = 0u;
                            results._m0[_20] = 0u;
                            _1820 = true;
                            break;
                        }
                        uint _84 = (uint(_1865.y) * Settings.width) + uint(_1865.x);
                        uint _85 = Settings.width * Settings.height;
                        uint _90 = (_85 * Settings.recordPrefix) + (((_84 * Settings.lobes) + _38) * Settings.pathStride);
                        bool _1896;
                        if (source._m0[((_85 * Settings.primaryPrefix) + (_84 * 4u)) >> 2u] == _1801)
                        {
                            uint _1884 = _90 >> 2u;
                            _1896 = any(uint4(source._m0[_1884], source._m0[_1884 + 1u], source._m0[_1884 + 2u], source._m0[_1884 + 3u]) != _1817);
                        }
                        else
                        {
                            _1896 = true;
                        }
                        bool _1904;
                        if (!_1896)
                        {
                            _1904 = source._m0[(_90 + 16u) >> 2u] != _1804;
                        }
                        else
                        {
                            _1904 = true;
                        }
                        bool _1912;
                        if (!_1904)
                        {
                            _1912 = source._m0[(_90 + 20u) >> 2u] != _1807;
                        }
                        else
                        {
                            _1912 = true;
                        }
                        bool _1921;
                        if (!_1912)
                        {
                            _1921 = (source._m0[(_90 + 36u) >> 2u] >> 24u) != 255u;
                        }
                        else
                        {
                            _1921 = true;
                        }
                        if (_1921)
                        {
                            results._m0[_361] = 0u;
                            results._m0[_18] = 4u;
                            results._m0[_19] = 0u;
                            results._m0[_20] = 0u;
                            _1820 = true;
                            break;
                        }
                        uint _1924 = (_90 + 36u) >> 2u;
                        float3 _203 = float3(float(source._m0[_1924] & 255u), float((source._m0[_1924] >> 8u) & 255u), float((source._m0[_1924] >> 16u) & 255u)) / float3(255.0);
                        float3 _1961;
                        if (_1245)
                        {
                            float _1939 = _203.x;
                            float _1945;
                            if (_1939 <= 0.040449999272823333740234375)
                            {
                                _1945 = _1939 / 12.9200000762939453125;
                            }
                            else
                            {
                                _1945 = precise::powr(spvFAdd(_1939, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                            }
                            float _1946 = _203.y;
                            float _1952;
                            if (_1946 <= 0.040449999272823333740234375)
                            {
                                _1952 = _1946 / 12.9200000762939453125;
                            }
                            else
                            {
                                _1952 = precise::powr(spvFAdd(_1946, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                            }
                            float _1953 = _203.z;
                            float _1959;
                            if (_1953 <= 0.040449999272823333740234375)
                            {
                                _1959 = _1953 / 12.9200000762939453125;
                            }
                            else
                            {
                                _1959 = precise::powr(spvFAdd(_1953, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                            }
                            _1961 = float3(_1945, _1952, _1959);
                        }
                        else
                        {
                            _1961 = _203;
                        }
                        bool3 _1962 = isnan(_1961);
                        bool3 _1963 = isinf(_1961);
                        if (!all(not(bool3(_1962.x || _1963.x, _1962.y || _1963.y, _1962.z || _1963.z))))
                        {
                            results._m0[_361] = 0u;
                            results._m0[_18] = 5u;
                            results._m0[_19] = 0u;
                            results._m0[_20] = 0u;
                            _1820 = true;
                            break;
                        }
                        _1831 = spvFAdd(_1823, _79);
                        _1833 = spvFAdd(_1825, _1961 * _79);
                        uint _104 = _1834 + 1u;
                        _1823 = _1831;
                        _1825 = _1833;
                        _1834 = _104;
                        continue;
                    }
                    else
                    {
                        _1820 = _1819;
                        break;
                    }
                }
                if (_1820)
                {
                    _1971 = _1823;
                    _1972 = _1825;
                    _1973 = _1820;
                    break;
                }
                _1819 = _1820;
                _1822 = _1823;
                _1824 = _1825;
                _1826++;
                continue;
            }
            else
            {
                _1971 = _1822;
                _1972 = _1824;
                _1973 = _1819;
                break;
            }
        }
        if (_1973)
        {
            break;
        }
        bool3 _1975 = isnan(_1972);
        bool3 _1976 = isinf(_1972);
        bool _1985;
        if (all(not(bool3(_1975.x || _1976.x, _1975.y || _1976.y, _1975.z || _1976.z))))
        {
            _1985 = isnan(_1971) || isinf(_1971);
        }
        else
        {
            _1985 = true;
        }
        bool _1991;
        if (!_1985)
        {
            _1991 = abs(spvFSub(_1971, 1.0)) > 9.9999997473787516355514526367188e-06;
        }
        else
        {
            _1991 = true;
        }
        if (_1991)
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 5u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        float3 _1998 = mix(_1271, fast::clamp(_1972, _1779, _1778), float3(ColourSettings.colourWeight));
        bool3 _1999 = isnan(_1998);
        bool3 _2000 = isinf(_1998);
        if (!all(not(bool3(_1999.x || _2000.x, _1999.y || _2000.y, _1999.z || _2000.z))))
        {
            results._m0[_361] = 0u;
            results._m0[_18] = 5u;
            results._m0[_19] = 0u;
            results._m0[_20] = 0u;
            break;
        }
        uint4 _2011 = as_type<uint4>(float4(_1271, ColourSettings.colourWeight));
        results._m0[_366] = _2011.x;
        results._m0[_214] = _2011.y;
        results._m0[_215] = _2011.z;
        results._m0[_216] = _2011.w;
        uint4 _2020 = as_type<uint4>(float4(_1779, 0.0));
        results._m0[_371] = _2020.x;
        results._m0[_218] = _2020.y;
        results._m0[_219] = _2020.z;
        results._m0[_220] = _2020.w;
        uint4 _2029 = as_type<uint4>(float4(_1778, 0.0));
        results._m0[_376] = _2029.x;
        results._m0[_222] = _2029.y;
        results._m0[_223] = _2029.z;
        results._m0[_224] = _2029.w;
        uint4 _2038 = as_type<uint4>(float4(_1972, 0.0));
        results._m0[_381] = _2038.x;
        results._m0[_226] = _2038.y;
        results._m0[_227] = _2038.z;
        results._m0[_228] = _2038.w;
        uint4 _2047 = as_type<uint4>(float4(_1998, 0.0));
        results._m0[_386] = _2047.x;
        results._m0[_230] = _2047.y;
        results._m0[_231] = _2047.z;
        results._m0[_232] = _2047.w;
        results._m0[_361] = 1u;
        results._m0[_18] = 0u;
        results._m0[_19] = _1780;
        results._m0[_20] = 0u;
        break;
    } while(false);
}

