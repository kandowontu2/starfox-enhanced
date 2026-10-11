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

struct type_ComposeSettings
{
    uint currentWidth;
    uint currentHeight;
    uint currentPrefix;
    uint currentRecords;
    uint currentTriangles;
    uint composeFirst;
    uint composeStride;
    uint composeOffset;
    uint composeBytes;
    uint composeFlags;
    uint composeCount;
    uint composeReserved;
    float composeWeight;
    uint composeLobes;
    uint composePathStride;
    uint composePad;
};

kernel void feature_compose_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& current [[buffer(2)]], device type_ByteAddressBuffer& queries [[buffer(3)]], device type_RWByteAddressBuffer& results [[buffer(4)]], constant type_ComposeSettings& ComposeSettings [[buffer(1)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        bool _360;
        if (ComposeSettings.composeLobes != 1u)
        {
            _360 = ComposeSettings.composeLobes != 8u;
        }
        else
        {
            _360 = false;
        }
        if (_360)
        {
            break;
        }
        uint _15 = gl_GlobalInvocationID.x * ComposeSettings.composeLobes;
        bool _370;
        if (_15 < ComposeSettings.composeCount)
        {
            _370 = _15 >= 64u;
        }
        else
        {
            _370 = true;
        }
        if (_370)
        {
            break;
        }
        uint _374 = min(ComposeSettings.composeLobes, min((ComposeSettings.composeCount - _15), (64u - _15)));
        for (uint _376 = 0u; _376 < _374; )
        {
            uint _19 = (_15 + _376) * 24576u;
            uint _380 = (_19 + 512u) >> 2u;
            results._m0[_380] = 0u;
            results._m0[_380 + 1u] = 0u;
            results._m0[_380 + 2u] = 0u;
            results._m0[_380 + 3u] = 0u;
            uint _385 = (_19 + 528u) >> 2u;
            results._m0[_385] = 0u;
            results._m0[_385 + 1u] = 0u;
            results._m0[_385 + 2u] = 0u;
            results._m0[_385 + 3u] = 0u;
            uint _390 = (_19 + 544u) >> 2u;
            results._m0[_390] = 0u;
            results._m0[_390 + 1u] = 0u;
            results._m0[_390 + 2u] = 0u;
            results._m0[_390 + 3u] = 0u;
            uint _395 = (_19 + 560u) >> 2u;
            results._m0[_395] = 0u;
            results._m0[_395 + 1u] = 0u;
            results._m0[_395 + 2u] = 0u;
            results._m0[_395 + 3u] = 0u;
            _376++;
            continue;
        }
        bool _406;
        if (ComposeSettings.composeCount == Settings.queryCount)
        {
            _406 = ComposeSettings.composeCount > 64u;
        }
        else
        {
            _406 = true;
        }
        bool _411;
        if (!_406)
        {
            _411 = _374 != ComposeSettings.composeLobes;
        }
        else
        {
            _411 = true;
        }
        bool _416;
        if (!_411)
        {
            _416 = (ComposeSettings.composeCount % ComposeSettings.composeLobes) != 0u;
        }
        else
        {
            _416 = true;
        }
        bool _423;
        if (!_416)
        {
            _423 = (ComposeSettings.composeFirst % ComposeSettings.composeLobes) != 0u;
        }
        else
        {
            _423 = true;
        }
        bool _430;
        if (!_423)
        {
            _430 = ComposeSettings.composeStride != 24576u;
        }
        else
        {
            _430 = true;
        }
        bool _437;
        if (!_430)
        {
            _437 = ComposeSettings.composeOffset != 512u;
        }
        else
        {
            _437 = true;
        }
        bool _444;
        if (!_437)
        {
            _444 = ComposeSettings.composeBytes != 64u;
        }
        else
        {
            _444 = true;
        }
        bool _451;
        if (!_444)
        {
            _451 = ComposeSettings.composeReserved != 0u;
        }
        else
        {
            _451 = true;
        }
        bool _458;
        if (!_451)
        {
            _458 = ComposeSettings.composePad != 0u;
        }
        else
        {
            _458 = true;
        }
        bool _465;
        if (!_458)
        {
            _465 = ComposeSettings.composeFlags > 15u;
        }
        else
        {
            _465 = true;
        }
        bool _472;
        if (!_465)
        {
            _472 = ComposeSettings.composeLobes != Settings.lobes;
        }
        else
        {
            _472 = true;
        }
        bool _481;
        if (!_472)
        {
            _481 = ComposeSettings.composePathStride != Settings.pathStride;
        }
        else
        {
            _481 = true;
        }
        bool _492;
        if (!_481)
        {
            bool _491;
            if (Settings.pathStride != 52u)
            {
                _491 = Settings.pathStride != 64u;
            }
            else
            {
                _491 = false;
            }
            _492 = _491;
        }
        else
        {
            _492 = true;
        }
        bool _499;
        if (!_492)
        {
            _499 = ComposeSettings.currentWidth == 0u;
        }
        else
        {
            _499 = true;
        }
        bool _506;
        if (!_499)
        {
            _506 = ComposeSettings.currentHeight == 0u;
        }
        else
        {
            _506 = true;
        }
        bool _513;
        if (!_506)
        {
            _513 = ComposeSettings.currentWidth > 16384u;
        }
        else
        {
            _513 = true;
        }
        bool _520;
        if (!_513)
        {
            _520 = ComposeSettings.currentHeight > 16384u;
        }
        else
        {
            _520 = true;
        }
        bool _529;
        if (!_520)
        {
            _529 = isnan(ComposeSettings.composeWeight) || isinf(ComposeSettings.composeWeight);
        }
        else
        {
            _529 = true;
        }
        bool _536;
        if (!_529)
        {
            _536 = ComposeSettings.composeWeight <= 0.0;
        }
        else
        {
            _536 = true;
        }
        bool _543;
        if (!_536)
        {
            _543 = ComposeSettings.composeWeight > 0.949999988079071044921875;
        }
        else
        {
            _543 = true;
        }
        bool _570;
        if (!_543)
        {
            bool _569;
            if ((ComposeSettings.composeFlags & 2u) != 0u)
            {
                bool _561;
                if (Settings.pathStride == 52u)
                {
                    _561 = ComposeSettings.currentPrefix != 4u;
                }
                else
                {
                    _561 = true;
                }
                bool _568;
                if (!_561)
                {
                    _568 = ComposeSettings.currentRecords != 44u;
                }
                else
                {
                    _568 = true;
                }
                _569 = _568;
            }
            else
            {
                _569 = false;
            }
            _570 = _569;
        }
        else
        {
            _570 = true;
        }
        bool _592;
        if (!_570)
        {
            bool _591;
            if ((ComposeSettings.composeFlags & 4u) != 0u)
            {
                bool _590;
                if (Settings.pathStride == 64u)
                {
                    _590 = ComposeSettings.currentRecords != (ComposeSettings.currentPrefix + 40u);
                }
                else
                {
                    _590 = true;
                }
                _591 = _590;
            }
            else
            {
                _591 = false;
            }
            _592 = _591;
        }
        else
        {
            _592 = true;
        }
        bool _612;
        if (!_592)
        {
            bool _611;
            if ((ComposeSettings.composeFlags & 6u) == 0u)
            {
                bool _610;
                if (ComposeSettings.currentPrefix == 4u)
                {
                    _610 = ComposeSettings.currentRecords != 28u;
                }
                else
                {
                    _610 = true;
                }
                _611 = _610;
            }
            else
            {
                _611 = false;
            }
            _612 = _611;
        }
        else
        {
            _612 = true;
        }
        if (_612)
        {
            for (uint _616 = 0u; _616 < _374; )
            {
                uint _620 = (((_15 + _616) * 24576u) + 512u) >> 2u;
                results._m0[_620] = 0u;
                results._m0[_620 + 1u] = 2u;
                results._m0[_620 + 2u] = 4294967295u;
                results._m0[_620 + 3u] = 0u;
                _616++;
                continue;
            }
            break;
        }
        uint _28 = ComposeSettings.composeFirst + _15;
        uint _29 = _28 / ComposeSettings.composeLobes;
        uint _30 = ComposeSettings.currentWidth * ComposeSettings.currentHeight;
        bool _635;
        if (_28 >= ComposeSettings.composeFirst)
        {
            _635 = _29 >= _30;
        }
        else
        {
            _635 = true;
        }
        if (_635)
        {
            for (uint _639 = 0u; _639 < _374; )
            {
                uint _643 = (((_15 + _639) * 24576u) + 512u) >> 2u;
                results._m0[_643] = 0u;
                results._m0[_643 + 1u] = 2u;
                results._m0[_643 + 2u] = 4294967295u;
                results._m0[_643 + 3u] = 0u;
                _639++;
                continue;
            }
            break;
        }
        uint _31 = _29 * 4u;
        uint _648 = _31 >> 2u;
        uint _650 = current._m0[_648];
        uint _653 = ((_30 * ComposeSettings.currentPrefix) + _31) >> 2u;
        uint _655 = current._m0[_653];
        for (uint _657 = 0u; _657 < _374; )
        {
            uint _665 = (((_15 + _657) * 24576u) + 528u) >> 2u;
            uint _668 = current._m0[(((_30 * ComposeSettings.currentRecords) + ((_28 + _657) * Settings.pathStride)) + 36u) >> 2u];
            results._m0[_665] = _650;
            results._m0[_665 + 1u] = _668;
            results._m0[_665 + 2u] = 0u;
            results._m0[_665 + 3u] = 0u;
            _657++;
            continue;
        }
        bool _676 = (ComposeSettings.composeFlags & 4u) != 0u;
        bool _678 = (ComposeSettings.composeFlags & 2u) != 0u;
        bool _687;
        if (_676)
        {
            _687 = _655 == 4294967293u;
        }
        else
        {
            bool _686;
            if (_678)
            {
                _686 = _655 == 4294967294u;
            }
            else
            {
                _686 = false;
            }
            _687 = _686;
        }
        uint _48 = _29 * 16u;
        uint _688 = ((_30 * (ComposeSettings.currentPrefix + 8u)) + _48) >> 2u;
        uint _690 = current._m0[_688];
        uint _692 = current._m0[_688 + 1u];
        uint _694 = current._m0[_688 + 2u];
        uint _696 = current._m0[_688 + 3u];
        float4 _698 = as_type<float4>(uint4(_690, _692, _694, _696));
        float4 _714;
        if ((!_676) ? _678 : true)
        {
            uint _703 = ((_30 * (ComposeSettings.currentPrefix + 24u)) + _48) >> 2u;
            _714 = as_type<float4>(uint4(current._m0[_703], current._m0[_703 + 1u], current._m0[_703 + 2u], current._m0[_703 + 3u]));
        }
        else
        {
            _714 = float4(0.0, 0.0, 0.0, 1.0);
        }
        bool _738;
        if (_655 != 4294967295u)
        {
            bool _737;
            if (_687)
            {
                bool _726;
                if (_676)
                {
                    _726 = (ComposeSettings.composeFlags & 8u) == 0u;
                }
                else
                {
                    _726 = false;
                }
                _737 = (_650 >> 24u) != (_726 ? 253u : 254u);
            }
            else
            {
                bool _736;
                if (_655 < ComposeSettings.currentTriangles)
                {
                    _736 = (_650 >> 24u) != 255u;
                }
                else
                {
                    _736 = true;
                }
                _737 = _736;
            }
            _738 = _737;
        }
        else
        {
            _738 = true;
        }
        bool _744;
        if (!_738)
        {
            _744 = _698.w != 1.0;
        }
        else
        {
            _744 = true;
        }
        bool _754;
        if (!_744)
        {
            bool4 _748 = isnan(_698);
            bool4 _749 = isinf(_698);
            _754 = !all(not(bool4(_748.x || _749.x, _748.y || _749.y, _748.z || _749.z, _748.w || _749.w)));
        }
        else
        {
            _754 = true;
        }
        bool _761;
        if (!_754)
        {
            _761 = any(_698.xyz < float3(0.0));
        }
        else
        {
            _761 = true;
        }
        bool _768;
        if (!_761)
        {
            _768 = any(_698.xyz > float3(1.0));
        }
        else
        {
            _768 = true;
        }
        bool _774;
        if (!_768)
        {
            _774 = _714.w != 1.0;
        }
        else
        {
            _774 = true;
        }
        bool _784;
        if (!_774)
        {
            bool4 _778 = isnan(_714);
            bool4 _779 = isinf(_714);
            _784 = !all(not(bool4(_778.x || _779.x, _778.y || _779.y, _778.z || _779.z, _778.w || _779.w)));
        }
        else
        {
            _784 = true;
        }
        bool _791;
        if (!_784)
        {
            _791 = any(_714.xyz < float3(0.0));
        }
        else
        {
            _791 = true;
        }
        bool _798;
        if (!_791)
        {
            _798 = any(_714.xyz > float3(999999995904.0));
        }
        else
        {
            _798 = true;
        }
        if (_798)
        {
            for (uint _802 = 0u; _802 < _374; )
            {
                uint _806 = (((_15 + _802) * 24576u) + 512u) >> 2u;
                results._m0[_806] = 0u;
                results._m0[_806 + 1u] = 3u;
                results._m0[_806 + 2u] = _29;
                results._m0[_806 + 3u] = 0u;
                _802++;
                continue;
            }
            break;
        }
        float3 _812;
        _812 = float3(0.0);
        float3 _101;
        spvUnsafeArray<uint, 8> _349;
        spvUnsafeArray<uint, 8> _350;
        uint _815;
        uint _1262;
        bool _1263;
        uint _814 = 0u;
        uint _816 = 0u;
        for (;;)
        {
            if (_816 < _374)
            {
                uint _59 = _15 + _816;
                uint _60 = _59 * 24576u;
                uint _64 = (_30 * ComposeSettings.currentRecords) + ((_28 + _816) * Settings.pathStride);
                uint _824 = (_64 + 36u) >> 2u;
                uint _826 = current._m0[_824];
                uint _829 = current._m0[(_64 + 16u) >> 2u];
                uint _830 = (_64 + 40u) >> 2u;
                uint _832 = current._m0[_830];
                uint _834 = current._m0[_830 + 1u];
                uint _836 = current._m0[_830 + 2u];
                float3 _838 = as_type<float3>(uint3(_832, _834, _836));
                float3 _851;
                if (Settings.pathStride == 64u)
                {
                    uint _842 = (_64 + 52u) >> 2u;
                    _851 = as_type<float3>(uint3(current._m0[_842], current._m0[_842 + 1u], current._m0[_842 + 2u]));
                }
                else
                {
                    _851 = float3(0.0);
                }
                bool3 _852 = isnan(_838);
                bool3 _853 = isinf(_838);
                bool _861;
                if (all(not(bool3(_852.x || _853.x, _852.y || _853.y, _852.z || _853.z))))
                {
                    _861 = any(_838 < float3(0.0));
                }
                else
                {
                    _861 = true;
                }
                bool _867;
                if (!_861)
                {
                    _867 = any(_838 > float3(1.0));
                }
                else
                {
                    _867 = true;
                }
                bool _877;
                if (!_867)
                {
                    bool3 _871 = isnan(_851);
                    bool3 _872 = isinf(_851);
                    _877 = !all(not(bool3(_871.x || _872.x, _871.y || _872.y, _871.z || _872.z)));
                }
                else
                {
                    _877 = true;
                }
                bool _883;
                if (!_877)
                {
                    _883 = any(_851 < float3(0.0));
                }
                else
                {
                    _883 = true;
                }
                bool _889;
                if (!_883)
                {
                    _889 = any(_851 > float3(999999995904.0));
                }
                else
                {
                    _889 = true;
                }
                if (_889)
                {
                    for (uint _893 = 0u; _893 < _374; )
                    {
                        uint _897 = (((_15 + _893) * 24576u) + 512u) >> 2u;
                        results._m0[_897] = 0u;
                        results._m0[_897 + 1u] = 3u;
                        results._m0[_897 + 2u] = _29;
                        results._m0[_897 + 3u] = 0u;
                        _893++;
                        continue;
                    }
                    _1262 = _814;
                    _1263 = true;
                    break;
                }
                float3 _145 = float3(float(_826 & 255u), float((_826 >> 8u) & 255u), float((_826 >> 16u) & 255u)) / float3(255.0);
                bool _912 = (ComposeSettings.composeFlags & 1u) != 0u;
                float3 _938;
                if (_912)
                {
                    float _916 = _145.x;
                    float _922;
                    if (_916 <= 0.040449999272823333740234375)
                    {
                        _922 = _916 / 12.9200000762939453125;
                    }
                    else
                    {
                        _922 = precise::powr(spvFAdd(_916, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                    }
                    float _923 = _145.y;
                    float _929;
                    if (_923 <= 0.040449999272823333740234375)
                    {
                        _929 = _923 / 12.9200000762939453125;
                    }
                    else
                    {
                        _929 = precise::powr(spvFAdd(_923, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                    }
                    float _930 = _145.z;
                    float _936;
                    if (_930 <= 0.040449999272823333740234375)
                    {
                        _936 = _930 / 12.9200000762939453125;
                    }
                    else
                    {
                        _936 = precise::powr(spvFAdd(_930, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                    }
                    _938 = float3(_922, _929, _936);
                }
                else
                {
                    _938 = _145;
                }
                _349[_816] = _826;
                uint _940 = (_60 + 384u) >> 2u;
                uint _75 = _940 + 2u;
                bool _953;
                if (results._m0[_940] == 1u)
                {
                    _953 = results._m0[_940 + 1u] == 0u;
                }
                else
                {
                    _953 = false;
                }
                bool _957;
                if (_953)
                {
                    _957 = results._m0[_75] > 0u;
                }
                else
                {
                    _957 = false;
                }
                bool _961;
                if (_957)
                {
                    _961 = results._m0[_75] <= 9u;
                }
                else
                {
                    _961 = false;
                }
                bool _965;
                if (_961)
                {
                    _965 = results._m0[_940 + 3u] == 0u;
                }
                else
                {
                    _965 = false;
                }
                bool _972;
                if (_965)
                {
                    _972 = results._m0[_60 >> 2u] == 1u;
                }
                else
                {
                    _972 = false;
                }
                bool _979;
                if (_972)
                {
                    _979 = results._m0[(_60 + 4u) >> 2u] == 1u;
                }
                else
                {
                    _979 = false;
                }
                bool _986;
                if (_979)
                {
                    _986 = results._m0[(_60 + 12u) >> 2u] == 0u;
                }
                else
                {
                    _986 = false;
                }
                bool _1005;
                if (_986)
                {
                    uint _989 = (_60 + 192u) >> 2u;
                    _1005 = all(uint4(results._m0[_989], results._m0[_989 + 1u], results._m0[_989 + 2u], results._m0[_989 + 3u]) == uint4(1u, 31u, 0u, results._m0[(_60 + 204u) >> 2u]));
                }
                else
                {
                    _1005 = false;
                }
                bool _1012;
                if (_1005)
                {
                    _1012 = results._m0[(_60 + 204u) >> 2u] > 0u;
                }
                else
                {
                    _1012 = false;
                }
                bool _1019;
                if (_1012)
                {
                    _1019 = results._m0[(_60 + 256u) >> 2u] == 1u;
                }
                else
                {
                    _1019 = false;
                }
                bool _1026;
                if (_1019)
                {
                    _1026 = results._m0[(_60 + 260u) >> 2u] == 0u;
                }
                else
                {
                    _1026 = false;
                }
                bool _1041;
                if (_1026)
                {
                    uint _1029 = (_60 + 320u) >> 2u;
                    _1041 = all(uint4(results._m0[_1029], results._m0[_1029 + 1u], results._m0[_1029 + 2u], results._m0[_1029 + 3u]) == uint4(1u, 0u, 0u, 0u));
                }
                else
                {
                    _1041 = false;
                }
                float3 _1168;
                if (_1041)
                {
                    uint _1044 = (_60 + 464u) >> 2u;
                    uint _1046 = results._m0[_1044];
                    uint _1048 = results._m0[_1044 + 1u];
                    uint _1050 = results._m0[_1044 + 2u];
                    uint _1052 = results._m0[_1044 + 3u];
                    float4 _1054 = as_type<float4>(uint4(_1046, _1048, _1050, _1052));
                    uint _96 = _59 * 48u;
                    bool _1069;
                    if (queries._m0[(_96 + 44u) >> 2u] == _816)
                    {
                        _1069 = queries._m0[(_96 + 20u) >> 2u] != _829;
                    }
                    else
                    {
                        _1069 = true;
                    }
                    bool _1075;
                    if (!_1069)
                    {
                        _1075 = _1054.w != 0.0;
                    }
                    else
                    {
                        _1075 = true;
                    }
                    bool _1085;
                    if (!_1075)
                    {
                        bool4 _1079 = isnan(_1054);
                        bool4 _1080 = isinf(_1054);
                        _1085 = !all(not(bool4(_1079.x || _1080.x, _1079.y || _1080.y, _1079.z || _1080.z, _1079.w || _1080.w)));
                    }
                    else
                    {
                        _1085 = true;
                    }
                    bool _1092;
                    if (!_1085)
                    {
                        _1092 = any(_1054.xyz < float3(0.0));
                    }
                    else
                    {
                        _1092 = true;
                    }
                    bool _1099;
                    if (!_1092)
                    {
                        _1099 = any(_1054.xyz > float3(1.0));
                    }
                    else
                    {
                        _1099 = true;
                    }
                    bool _1106;
                    if (!_1099)
                    {
                        _1106 = as_type<float>(results._m0[(_60 + 412u) >> 2u]) != ComposeSettings.composeWeight;
                    }
                    else
                    {
                        _1106 = true;
                    }
                    bool _1112;
                    if (!_1106)
                    {
                        _1112 = (_826 >> 24u) != 255u;
                    }
                    else
                    {
                        _1112 = true;
                    }
                    if (_1112)
                    {
                        for (uint _1116 = 0u; _1116 < _374; )
                        {
                            uint _1120 = (((_15 + _1116) * 24576u) + 512u) >> 2u;
                            results._m0[_1120] = 0u;
                            results._m0[_1120 + 1u] = 4u;
                            results._m0[_1120 + 2u] = _29;
                            results._m0[_1120 + 3u] = 0u;
                            _1116++;
                            continue;
                        }
                        _1262 = _814;
                        _1263 = true;
                        break;
                    }
                    float3 _1125 = _1054.xyz;
                    float3 _1154;
                    if (_912)
                    {
                        float3 _1130 = fast::clamp(_1125, float3(0.0), float3(1.0));
                        float _1131 = _1130.x;
                        float _1137;
                        if (_1131 <= 0.003130800090730190277099609375)
                        {
                            _1137 = spvFMul(_1131, 12.9200000762939453125);
                        }
                        else
                        {
                            _1137 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1131, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                        }
                        float _1138 = _1130.y;
                        float _1144;
                        if (_1138 <= 0.003130800090730190277099609375)
                        {
                            _1144 = spvFMul(_1138, 12.9200000762939453125);
                        }
                        else
                        {
                            _1144 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1138, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                        }
                        float _1145 = _1130.z;
                        float _1151;
                        if (_1145 <= 0.003130800090730190277099609375)
                        {
                            _1151 = spvFMul(_1145, 12.9200000762939453125);
                        }
                        else
                        {
                            _1151 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1145, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                        }
                        _1154 = float3(_1137, _1144, _1151);
                    }
                    else
                    {
                        _1154 = fast::clamp(_1125, float3(0.0), float3(1.0));
                    }
                    uint3 _1156 = uint3(floor(spvFAdd(_1154 * 255.0, float3(0.5))));
                    _349[_816] = ((_1156.x | (_1156.y << 8u)) | (_1156.z << 16u)) | (_826 & 4278190080u);
                    _815 = _814 | (1u << (_816 & 31u));
                    _1168 = _1125;
                }
                else
                {
                    _815 = _814;
                    _1168 = _938;
                }
                float3 _100 = spvFAdd(spvFMul(_1168, _838), _851);
                bool3 _1169 = isnan(_100);
                bool3 _1170 = isinf(_100);
                if (!all(not(bool3(_1169.x || _1170.x, _1169.y || _1170.y, _1169.z || _1170.z))))
                {
                    for (uint _1178 = 0u; _1178 < _374; )
                    {
                        uint _1182 = (((_15 + _1178) * 24576u) + 512u) >> 2u;
                        results._m0[_1182] = 0u;
                        results._m0[_1182 + 1u] = 4u;
                        results._m0[_1182 + 2u] = _29;
                        results._m0[_1182 + 3u] = 0u;
                        _1178++;
                        continue;
                    }
                    _1262 = _815;
                    _1263 = true;
                    break;
                }
                float3 _1214;
                if (_912)
                {
                    float3 _1190 = fast::clamp(_100, float3(0.0), float3(1.0));
                    float _1191 = _1190.x;
                    float _1197;
                    if (_1191 <= 0.003130800090730190277099609375)
                    {
                        _1197 = spvFMul(_1191, 12.9200000762939453125);
                    }
                    else
                    {
                        _1197 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1191, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                    }
                    float _1198 = _1190.y;
                    float _1204;
                    if (_1198 <= 0.003130800090730190277099609375)
                    {
                        _1204 = spvFMul(_1198, 12.9200000762939453125);
                    }
                    else
                    {
                        _1204 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1198, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                    }
                    float _1205 = _1190.z;
                    float _1211;
                    if (_1205 <= 0.003130800090730190277099609375)
                    {
                        _1211 = spvFMul(_1205, 12.9200000762939453125);
                    }
                    else
                    {
                        _1211 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1205, 0.4166666567325592041015625)), 0.054999999701976776123046875);
                    }
                    _1214 = float3(_1197, _1204, _1211);
                }
                else
                {
                    _1214 = fast::clamp(_100, float3(0.0), float3(1.0));
                }
                uint3 _1216 = uint3(floor(spvFAdd(_1214 * 255.0, float3(0.5))));
                _350[_816] = ((_1216.x | (_1216.y << 8u)) | (_1216.z << 16u)) | 4278190080u;
                float3 _191 = float3(float(_350[_816] & 255u), float((_350[_816] >> 8u) & 255u), float((_350[_816] >> 16u) & 255u)) / float3(255.0);
                float3 _1261;
                if (_912)
                {
                    float _1239 = _191.x;
                    float _1245;
                    if (_1239 <= 0.040449999272823333740234375)
                    {
                        _1245 = _1239 / 12.9200000762939453125;
                    }
                    else
                    {
                        _1245 = precise::powr(spvFAdd(_1239, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                    }
                    float _1246 = _191.y;
                    float _1252;
                    if (_1246 <= 0.040449999272823333740234375)
                    {
                        _1252 = _1246 / 12.9200000762939453125;
                    }
                    else
                    {
                        _1252 = precise::powr(spvFAdd(_1246, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                    }
                    float _1253 = _191.z;
                    float _1259;
                    if (_1253 <= 0.040449999272823333740234375)
                    {
                        _1259 = _1253 / 12.9200000762939453125;
                    }
                    else
                    {
                        _1259 = precise::powr(spvFAdd(_1253, 0.054999999701976776123046875) / 1.05499994754791259765625, 2.400000095367431640625);
                    }
                    _1261 = float3(_1245, _1252, _1259);
                }
                else
                {
                    _1261 = _191;
                }
                _101 = spvFAdd(_812, _1261);
                _812 = _101;
                _814 = _815;
                _816++;
                continue;
            }
            else
            {
                _1262 = _814;
                _1263 = false;
                break;
            }
        }
        if (_1263)
        {
            break;
        }
        if (_1262 == 0u)
        {
            for (uint _1269 = 0u; _1269 < _374; )
            {
                uint _1273 = (((_15 + _1269) * 24576u) + 512u) >> 2u;
                results._m0[_1273] = 0u;
                results._m0[_1273 + 1u] = 1u;
                results._m0[_1273 + 2u] = _29;
                results._m0[_1273 + 3u] = 0u;
                _1269++;
                continue;
            }
            break;
        }
        float3 _105 = spvFAdd(_714.xyz, spvFMul(_812 / float3(float(ComposeSettings.composeLobes)), _698.xyz));
        bool3 _1282 = isnan(_105);
        bool3 _1283 = isinf(_105);
        if (!all(not(bool3(_1282.x || _1283.x, _1282.y || _1283.y, _1282.z || _1283.z))))
        {
            for (uint _1291 = 0u; _1291 < _374; )
            {
                uint _1295 = (((_15 + _1291) * 24576u) + 512u) >> 2u;
                results._m0[_1295] = 0u;
                results._m0[_1295 + 1u] = 4u;
                results._m0[_1295 + 2u] = _29;
                results._m0[_1295 + 3u] = 0u;
                _1291++;
                continue;
            }
            break;
        }
        float3 _1330;
        if ((ComposeSettings.composeFlags & 1u) != 0u)
        {
            float3 _1306 = fast::clamp(_105, float3(0.0), float3(1.0));
            float _1307 = _1306.x;
            float _1313;
            if (_1307 <= 0.003130800090730190277099609375)
            {
                _1313 = spvFMul(_1307, 12.9200000762939453125);
            }
            else
            {
                _1313 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1307, 0.4166666567325592041015625)), 0.054999999701976776123046875);
            }
            float _1314 = _1306.y;
            float _1320;
            if (_1314 <= 0.003130800090730190277099609375)
            {
                _1320 = spvFMul(_1314, 12.9200000762939453125);
            }
            else
            {
                _1320 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1314, 0.4166666567325592041015625)), 0.054999999701976776123046875);
            }
            float _1321 = _1306.z;
            float _1327;
            if (_1321 <= 0.003130800090730190277099609375)
            {
                _1327 = spvFMul(_1321, 12.9200000762939453125);
            }
            else
            {
                _1327 = spvFSub(spvFMul(1.05499994754791259765625, precise::powr(_1321, 0.4166666567325592041015625)), 0.054999999701976776123046875);
            }
            _1330 = float3(_1313, _1320, _1327);
        }
        else
        {
            _1330 = fast::clamp(_105, float3(0.0), float3(1.0));
        }
        uint3 _1332 = uint3(floor(spvFAdd(_1330 * 255.0, float3(0.5))));
        uint _1340 = ((_1332.x | (_1332.y << 8u)) | (_1332.z << 16u)) | (_650 & 4278190080u);
        for (uint _1342 = 0u; _1342 < _374; )
        {
            uint _107 = (_15 + _1342) * 24576u;
            uint _1346 = (_107 + 528u) >> 2u;
            results._m0[_1346] = _1340;
            results._m0[_1346 + 1u] = _349[_1342];
            results._m0[_1346 + 2u] = _350[_1342];
            results._m0[_1346 + 3u] = 0u;
            uint _1355 = (_107 + 544u) >> 2u;
            uint4 _1360 = as_type<uint4>(float4(_105, 0.0));
            results._m0[_1355] = _1360.x;
            results._m0[_1355 + 1u] = _1360.y;
            results._m0[_1355 + 2u] = _1360.z;
            results._m0[_1355 + 3u] = _1360.w;
            _1342++;
            continue;
        }
        for (uint _1370 = 0u; _1370 < _374; )
        {
            uint _1374 = (((_15 + _1370) * 24576u) + 512u) >> 2u;
            results._m0[_1374] = 1u;
            results._m0[_1374 + 1u] = 0u;
            results._m0[_1374 + 2u] = _29;
            results._m0[_1374 + 3u] = _1262;
            _1370++;
            continue;
        }
        break;
    } while(false);
}

