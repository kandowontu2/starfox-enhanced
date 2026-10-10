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

struct type_Mapping
{
    uint currentWidth;
    uint currentHeight;
    uint primaryPrefix;
    uint recordPrefix;
    uint pathStride;
    uint currentTriangles;
    uint oldTriangles;
    uint previousMapping;
    uint geometryBytes;
    uint lobes;
    uint queryFirst;
    uint queryCount;
    uint pathFlags;
    uint mapUnused0;
    uint mapUnused1;
    uint mapUnused2;
    float4 currentCube[3];
    float4 previousCube[3];
};

kernel void feature_mapping_main(device type_ByteAddressBuffer& current [[buffer(0)]], device type_ByteAddressBuffer& geometry [[buffer(1)]], device type_RWByteAddressBuffer& queries [[buffer(2)]], constant type_Mapping& Mapping [[buffer(3)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        if (gl_GlobalInvocationID.x >= Mapping.queryCount)
        {
            break;
        }
        uint _13 = Mapping.queryFirst + gl_GlobalInvocationID.x;
        uint _16 = Mapping.currentWidth * Mapping.currentHeight;
        uint _17 = gl_GlobalInvocationID.x * 48u;
        uint _19 = (_13 / Mapping.lobes) * 4u;
        uint _1601 = ((_16 * Mapping.primaryPrefix) + _19) >> 2u;
        float _1607 = as_type<float>(current._m0[((_16 * (Mapping.primaryPrefix + 4u)) + _19) >> 2u]);
        uint _26 = (_16 * Mapping.recordPrefix) + (_13 * Mapping.pathStride);
        uint _1612 = _26 >> 2u;
        uint4 _1621 = uint4(current._m0[_1612], current._m0[_1612 + 1u], current._m0[_1612 + 2u], current._m0[_1612 + 3u]);
        uint _1622 = (_26 + 16u) >> 2u;
        uint _1624 = current._m0[_1622];
        uint _1625 = (_26 + 20u) >> 2u;
        uint _1628 = (_26 + 24u) >> 2u;
        float3 _1636 = as_type<float3>(uint3(current._m0[_1628], current._m0[_1628 + 1u], current._m0[_1628 + 2u]));
        uint _1637 = _1624 & 7u;
        uint _1638 = _1624 >> 4u;
        uint _1639 = _1638 & 15u;
        uint _1640 = _1624 >> 8u;
        bool _1648;
        if (current._m0[_1601] != 4294967295u)
        {
            _1648 = !(isnan(_1607) || isinf(_1607));
        }
        else
        {
            _1648 = false;
        }
        bool _1652;
        if (_1648)
        {
            _1652 = _1607 > 0.0;
        }
        else
        {
            _1652 = false;
        }
        bool _1669;
        if (_1652)
        {
            bool _1660;
            if (_1637 <= 4u)
            {
                _1660 = (_1639 >> _1637) == 0u;
            }
            else
            {
                _1660 = false;
            }
            bool _1664;
            if (_1660)
            {
                _1664 = _1640 >= 1u;
            }
            else
            {
                _1664 = false;
            }
            bool _1668;
            if (_1664)
            {
                _1668 = _1640 <= 3u;
            }
            else
            {
                _1668 = false;
            }
            _1669 = _1668;
        }
        else
        {
            _1669 = false;
        }
        bool _1710;
        if (_1669)
        {
            bool _1709;
            do
            {
                bool3 _1674 = isnan(_1636);
                bool3 _1675 = isinf(_1636);
                if (!all(not(bool3(_1674.x || _1675.x, _1674.y || _1675.y, _1674.z || _1675.z))))
                {
                    _1709 = false;
                    break;
                }
                if (_1640 == 1u)
                {
                    bool _1692;
                    if (_1636.z == 0.0)
                    {
                        _1692 = all(_1636.xy >= float2(0.0));
                    }
                    else
                    {
                        _1692 = false;
                    }
                    bool _1698;
                    if (_1692)
                    {
                        _1698 = spvFAdd(_1636.x, _1636.y) <= 1.0;
                    }
                    else
                    {
                        _1698 = false;
                    }
                    _1709 = _1698;
                    break;
                }
                bool _1703;
                if (_1640 != 2u)
                {
                    _1703 = _1640 == 3u;
                }
                else
                {
                    _1703 = true;
                }
                bool _1708;
                if (_1703)
                {
                    _1708 = abs(spvFSub(dot(_1636, _1636), 1.0)) < 0.00010099999781232327222824096679688;
                }
                else
                {
                    _1708 = false;
                }
                _1709 = _1708;
                break;
            } while(false);
            _1710 = _1709;
        }
        else
        {
            _1710 = false;
        }
        bool _1718;
        if (_1710)
        {
            _1718 = _1624 == (((_1640 << 8u) | (_1639 << 4u)) | _1637);
        }
        else
        {
            _1718 = false;
        }
        bool _1728;
        if (_1718)
        {
            bool _1727;
            if (_1640 == 3u)
            {
                _1727 = _1637 == 4u;
            }
            else
            {
                _1727 = _1637 < 4u;
            }
            _1728 = _1727;
        }
        else
        {
            _1728 = false;
        }
        bool _1739;
        if (_1728)
        {
            bool _1738;
            if ((Mapping.pathFlags & 2u) == 0u)
            {
                _1738 = _1639 == 0u;
            }
            else
            {
                _1738 = true;
            }
            _1739 = _1738;
        }
        else
        {
            _1739 = false;
        }
        uint _1802;
        bool _1803;
        if (current._m0[_1601] == 4294967293u)
        {
            bool _1750;
            if (_1739)
            {
                _1750 = (Mapping.pathFlags & 4u) != 0u;
            }
            else
            {
                _1750 = false;
            }
            _1802 = current._m0[_1601];
            _1803 = _1750;
        }
        else
        {
            uint _1800;
            bool _1801;
            if (current._m0[_1601] == 4294967294u)
            {
                bool _1761;
                if (_1739)
                {
                    _1761 = (Mapping.pathFlags & 1u) != 0u;
                }
                else
                {
                    _1761 = false;
                }
                _1800 = current._m0[_1601];
                _1801 = _1761;
            }
            else
            {
                uint _1798;
                bool _1799;
                if (_1739)
                {
                    uint _1796;
                    bool _1797;
                    do
                    {
                        bool _1776;
                        if (current._m0[_1601] < Mapping.currentTriangles)
                        {
                            _1776 = Mapping.previousMapping > Mapping.geometryBytes;
                        }
                        else
                        {
                            _1776 = true;
                        }
                        bool _1785;
                        if (!_1776)
                        {
                            _1785 = current._m0[_1601] >= ((Mapping.geometryBytes - Mapping.previousMapping) / 4u);
                        }
                        else
                        {
                            _1785 = true;
                        }
                        if (_1785)
                        {
                            _1796 = 4294967295u;
                            _1797 = false;
                            break;
                        }
                        uint _1790 = (Mapping.previousMapping + (current._m0[_1601] * 4u)) >> 2u;
                        _1796 = geometry._m0[_1790];
                        _1797 = geometry._m0[_1790] < Mapping.oldTriangles;
                        break;
                    } while(false);
                    _1798 = _1796;
                    _1799 = _1797;
                }
                else
                {
                    _1798 = 4294967295u;
                    _1799 = false;
                }
                _1800 = _1798;
                _1801 = _1799;
            }
            _1802 = _1800;
            _1803 = _1801;
        }
        uint4 _1872;
        bool _1873;
        if (0u >= _1637)
        {
            bool _1811;
            if (_1803)
            {
                _1811 = current._m0[_1612] == 4294967295u;
            }
            else
            {
                _1811 = false;
            }
            _1872 = _1621;
            _1873 = _1811;
        }
        else
        {
            uint4 _1870;
            bool _1871;
            if ((_1638 & 1u) != 0u)
            {
                bool _1820;
                if (_1803)
                {
                    _1820 = current._m0[_1612] == 4294967293u;
                }
                else
                {
                    _1820 = false;
                }
                _1870 = _1621;
                _1871 = _1820;
            }
            else
            {
                uint4 _1868;
                bool _1869;
                if (current._m0[_1612] == 4294967294u)
                {
                    bool _1831;
                    if (_1803)
                    {
                        _1831 = (Mapping.pathFlags & 1u) != 0u;
                    }
                    else
                    {
                        _1831 = false;
                    }
                    _1868 = _1621;
                    _1869 = _1831;
                }
                else
                {
                    uint _1864;
                    bool _1865;
                    do
                    {
                        bool _1844;
                        if (current._m0[_1612] < Mapping.currentTriangles)
                        {
                            _1844 = Mapping.previousMapping > Mapping.geometryBytes;
                        }
                        else
                        {
                            _1844 = true;
                        }
                        bool _1853;
                        if (!_1844)
                        {
                            _1853 = current._m0[_1612] >= ((Mapping.geometryBytes - Mapping.previousMapping) / 4u);
                        }
                        else
                        {
                            _1853 = true;
                        }
                        if (_1853)
                        {
                            _1864 = 4294967295u;
                            _1865 = false;
                            break;
                        }
                        uint _1858 = (Mapping.previousMapping + (current._m0[_1612] * 4u)) >> 2u;
                        _1864 = geometry._m0[_1858];
                        _1865 = geometry._m0[_1858] < Mapping.oldTriangles;
                        break;
                    } while(false);
                    _1621.x = _1864;
                    _1868 = _1621;
                    _1869 = _1803 ? _1865 : false;
                }
                _1870 = _1868;
                _1871 = _1869;
            }
            _1872 = _1870;
            _1873 = _1871;
        }
        uint4 _1945;
        bool _1946;
        if (1u >= _1637)
        {
            bool _1944;
            if (_1873)
            {
                _1944 = _1872.y == 4294967295u;
            }
            else
            {
                _1944 = false;
            }
            _1945 = _1872;
            _1946 = _1944;
        }
        else
        {
            uint4 _1938;
            bool _1939;
            if ((_1638 & 2u) != 0u)
            {
                bool _1937;
                if (_1873)
                {
                    _1937 = _1872.y == 4294967293u;
                }
                else
                {
                    _1937 = false;
                }
                _1938 = _1872;
                _1939 = _1937;
            }
            else
            {
                uint4 _1931;
                bool _1932;
                if (_1872.y == 4294967294u)
                {
                    bool _1930;
                    if (_1873)
                    {
                        _1930 = (Mapping.pathFlags & 1u) != 0u;
                    }
                    else
                    {
                        _1930 = false;
                    }
                    _1931 = _1872;
                    _1932 = _1930;
                }
                else
                {
                    uint _1920;
                    bool _1921;
                    do
                    {
                        bool _1900;
                        if (_1872.y < Mapping.currentTriangles)
                        {
                            _1900 = Mapping.previousMapping > Mapping.geometryBytes;
                        }
                        else
                        {
                            _1900 = true;
                        }
                        bool _1909;
                        if (!_1900)
                        {
                            _1909 = _1872.y >= ((Mapping.geometryBytes - Mapping.previousMapping) / 4u);
                        }
                        else
                        {
                            _1909 = true;
                        }
                        if (_1909)
                        {
                            _1920 = 4294967295u;
                            _1921 = false;
                            break;
                        }
                        uint _1914 = (Mapping.previousMapping + (_1872.y * 4u)) >> 2u;
                        _1920 = geometry._m0[_1914];
                        _1921 = geometry._m0[_1914] < Mapping.oldTriangles;
                        break;
                    } while(false);
                    uint4 _1922 = _1872;
                    _1922.y = _1920;
                    _1931 = _1922;
                    _1932 = _1873 ? _1921 : false;
                }
                _1938 = _1931;
                _1939 = _1932;
            }
            _1945 = _1938;
            _1946 = _1939;
        }
        uint4 _2018;
        bool _2019;
        if (2u >= _1637)
        {
            bool _2017;
            if (_1946)
            {
                _2017 = _1945.z == 4294967295u;
            }
            else
            {
                _2017 = false;
            }
            _2018 = _1945;
            _2019 = _2017;
        }
        else
        {
            uint4 _2011;
            bool _2012;
            if ((_1638 & 4u) != 0u)
            {
                bool _2010;
                if (_1946)
                {
                    _2010 = _1945.z == 4294967293u;
                }
                else
                {
                    _2010 = false;
                }
                _2011 = _1945;
                _2012 = _2010;
            }
            else
            {
                uint4 _2004;
                bool _2005;
                if (_1945.z == 4294967294u)
                {
                    bool _2003;
                    if (_1946)
                    {
                        _2003 = (Mapping.pathFlags & 1u) != 0u;
                    }
                    else
                    {
                        _2003 = false;
                    }
                    _2004 = _1945;
                    _2005 = _2003;
                }
                else
                {
                    uint _1993;
                    bool _1994;
                    do
                    {
                        bool _1973;
                        if (_1945.z < Mapping.currentTriangles)
                        {
                            _1973 = Mapping.previousMapping > Mapping.geometryBytes;
                        }
                        else
                        {
                            _1973 = true;
                        }
                        bool _1982;
                        if (!_1973)
                        {
                            _1982 = _1945.z >= ((Mapping.geometryBytes - Mapping.previousMapping) / 4u);
                        }
                        else
                        {
                            _1982 = true;
                        }
                        if (_1982)
                        {
                            _1993 = 4294967295u;
                            _1994 = false;
                            break;
                        }
                        uint _1987 = (Mapping.previousMapping + (_1945.z * 4u)) >> 2u;
                        _1993 = geometry._m0[_1987];
                        _1994 = geometry._m0[_1987] < Mapping.oldTriangles;
                        break;
                    } while(false);
                    uint4 _1995 = _1945;
                    _1995.z = _1993;
                    _2004 = _1995;
                    _2005 = _1946 ? _1994 : false;
                }
                _2011 = _2004;
                _2012 = _2005;
            }
            _2018 = _2011;
            _2019 = _2012;
        }
        uint4 _2091;
        bool _2092;
        if (3u >= _1637)
        {
            bool _2090;
            if (_2019)
            {
                _2090 = _2018.w == 4294967295u;
            }
            else
            {
                _2090 = false;
            }
            _2091 = _2018;
            _2092 = _2090;
        }
        else
        {
            uint4 _2084;
            bool _2085;
            if ((_1638 & 8u) != 0u)
            {
                bool _2083;
                if (_2019)
                {
                    _2083 = _2018.w == 4294967293u;
                }
                else
                {
                    _2083 = false;
                }
                _2084 = _2018;
                _2085 = _2083;
            }
            else
            {
                uint4 _2077;
                bool _2078;
                if (_2018.w == 4294967294u)
                {
                    bool _2076;
                    if (_2019)
                    {
                        _2076 = (Mapping.pathFlags & 1u) != 0u;
                    }
                    else
                    {
                        _2076 = false;
                    }
                    _2077 = _2018;
                    _2078 = _2076;
                }
                else
                {
                    uint _2066;
                    bool _2067;
                    do
                    {
                        bool _2046;
                        if (_2018.w < Mapping.currentTriangles)
                        {
                            _2046 = Mapping.previousMapping > Mapping.geometryBytes;
                        }
                        else
                        {
                            _2046 = true;
                        }
                        bool _2055;
                        if (!_2046)
                        {
                            _2055 = _2018.w >= ((Mapping.geometryBytes - Mapping.previousMapping) / 4u);
                        }
                        else
                        {
                            _2055 = true;
                        }
                        if (_2055)
                        {
                            _2066 = 4294967295u;
                            _2067 = false;
                            break;
                        }
                        uint _2060 = (Mapping.previousMapping + (_2018.w * 4u)) >> 2u;
                        _2066 = geometry._m0[_2060];
                        _2067 = geometry._m0[_2060] < Mapping.oldTriangles;
                        break;
                    } while(false);
                    uint4 _2068 = _2018;
                    _2068.w = _2066;
                    _2077 = _2068;
                    _2078 = _2019 ? _2067 : false;
                }
                _2084 = _2077;
                _2085 = _2078;
            }
            _2091 = _2084;
            _2092 = _2085;
        }
        bool _2093 = _1640 == 1u;
        float3 _2244;
        uint _2245;
        bool _2246;
        if (_2093)
        {
            uint _2129;
            bool _2130;
            do
            {
                bool _2109;
                if (current._m0[_1625] < Mapping.currentTriangles)
                {
                    _2109 = Mapping.previousMapping > Mapping.geometryBytes;
                }
                else
                {
                    _2109 = true;
                }
                bool _2118;
                if (!_2109)
                {
                    _2118 = current._m0[_1625] >= ((Mapping.geometryBytes - Mapping.previousMapping) / 4u);
                }
                else
                {
                    _2118 = true;
                }
                if (_2118)
                {
                    _2129 = 4294967295u;
                    _2130 = false;
                    break;
                }
                uint _2123 = (Mapping.previousMapping + (current._m0[_1625] * 4u)) >> 2u;
                _2129 = geometry._m0[_2123];
                _2130 = geometry._m0[_2123] < Mapping.oldTriangles;
                break;
            } while(false);
            _2244 = _1636;
            _2245 = _2129;
            _2246 = _2092 ? _2130 : false;
        }
        else
        {
            bool _2135;
            if (_2092)
            {
                _2135 = current._m0[_1625] == 4294967295u;
            }
            else
            {
                _2135 = false;
            }
            float3 _2203;
            do
            {
                float _2140 = _1636.x;
                float _61 = spvFMul(Mapping.currentCube[0u].x, _2140);
                float _62 = spvFMul(Mapping.currentCube[0u].x, 4097.0);
                float _64 = spvFSub(_62, spvFSub(_62, Mapping.currentCube[0u].x));
                float _65 = spvFSub(Mapping.currentCube[0u].x, _64);
                float _66 = spvFMul(_2140, 4097.0);
                float _68 = spvFSub(_66, spvFSub(_66, _2140));
                float _69 = spvFSub(_2140, _68);
                float _80 = spvFMul(0.0, _2140);
                float _82 = spvFMul(0.0, 0.0);
                float _83 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_64, _68), _61), spvFMul(_64, _69)), spvFMul(_65, _68)), spvFMul(_65, _69)), spvFMul(Mapping.currentCube[0u].x, 0.0)), _80), _82);
                float _84 = spvFAdd(_61, _83);
                float _86 = spvFSub(_83, spvFSub(_84, _61));
                float _87 = spvFAdd(0.0, _84);
                float _88 = spvFSub(_87, 0.0);
                float _93 = spvFAdd(0.0, _86);
                float _94 = spvFSub(_93, 0.0);
                float _99 = spvFAdd(spvFAdd(spvFSub(0.0, spvFSub(_87, _88)), spvFSub(_84, _88)), _93);
                float _100 = spvFAdd(_87, _99);
                float _103 = spvFAdd(spvFSub(_99, spvFSub(_100, _87)), spvFAdd(spvFSub(0.0, spvFSub(_93, _94)), spvFSub(_86, _94)));
                float _104 = spvFAdd(_100, _103);
                float _106 = spvFSub(_103, spvFSub(_104, _100));
                float _2143 = _1636.y;
                float _1177 = spvFMul(Mapping.currentCube[0u].y, _2143);
                float _1178 = spvFMul(Mapping.currentCube[0u].y, 4097.0);
                float _1180 = spvFSub(_1178, spvFSub(_1178, Mapping.currentCube[0u].y));
                float _1181 = spvFSub(Mapping.currentCube[0u].y, _1180);
                float _1182 = spvFMul(_2143, 4097.0);
                float _1184 = spvFSub(_1182, spvFSub(_1182, _2143));
                float _1185 = spvFSub(_2143, _1184);
                float _1196 = spvFMul(0.0, _2143);
                float _1198 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1180, _1184), _1177), spvFMul(_1180, _1185)), spvFMul(_1181, _1184)), spvFMul(_1181, _1185)), spvFMul(Mapping.currentCube[0u].y, 0.0)), _1196), _82);
                float _1199 = spvFAdd(_1177, _1198);
                float _1201 = spvFSub(_1198, spvFSub(_1199, _1177));
                float _1202 = spvFAdd(_104, _1199);
                float _1203 = spvFSub(_1202, _104);
                float _1208 = spvFAdd(_106, _1201);
                float _1209 = spvFSub(_1208, _106);
                float _1214 = spvFAdd(spvFAdd(spvFSub(_104, spvFSub(_1202, _1203)), spvFSub(_1199, _1203)), _1208);
                float _1215 = spvFAdd(_1202, _1214);
                float _1218 = spvFAdd(spvFSub(_1214, spvFSub(_1215, _1202)), spvFAdd(spvFSub(_106, spvFSub(_1208, _1209)), spvFSub(_1201, _1209)));
                float _1219 = spvFAdd(_1215, _1218);
                float _1221 = spvFSub(_1218, spvFSub(_1219, _1215));
                float _2146 = _1636.z;
                float _1222 = spvFMul(Mapping.currentCube[0u].z, _2146);
                float _1223 = spvFMul(Mapping.currentCube[0u].z, 4097.0);
                float _1225 = spvFSub(_1223, spvFSub(_1223, Mapping.currentCube[0u].z));
                float _1226 = spvFSub(Mapping.currentCube[0u].z, _1225);
                float _1227 = spvFMul(_2146, 4097.0);
                float _1229 = spvFSub(_1227, spvFSub(_1227, _2146));
                float _1230 = spvFSub(_2146, _1229);
                float _1241 = spvFMul(0.0, _2146);
                float _1243 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1225, _1229), _1222), spvFMul(_1225, _1230)), spvFMul(_1226, _1229)), spvFMul(_1226, _1230)), spvFMul(Mapping.currentCube[0u].z, 0.0)), _1241), _82);
                float _1244 = spvFAdd(_1222, _1243);
                float _1246 = spvFSub(_1243, spvFSub(_1244, _1222));
                float _1247 = spvFAdd(_1219, _1244);
                float _1248 = spvFSub(_1247, _1219);
                float _1253 = spvFAdd(_1221, _1246);
                float _1254 = spvFSub(_1253, _1221);
                float _1259 = spvFAdd(spvFAdd(spvFSub(_1219, spvFSub(_1247, _1248)), spvFSub(_1244, _1248)), _1253);
                float _1260 = spvFAdd(_1247, _1259);
                float _1263 = spvFAdd(spvFSub(_1259, spvFSub(_1260, _1247)), spvFAdd(spvFSub(_1221, spvFSub(_1253, _1254)), spvFSub(_1246, _1254)));
                float _1264 = spvFAdd(_1260, _1263);
                float _1266 = spvFSub(_1263, spvFSub(_1264, _1260));
                float _1267 = spvFMul(Mapping.currentCube[1u].x, _2140);
                float _1268 = spvFMul(Mapping.currentCube[1u].x, 4097.0);
                float _1270 = spvFSub(_1268, spvFSub(_1268, Mapping.currentCube[1u].x));
                float _1271 = spvFSub(Mapping.currentCube[1u].x, _1270);
                float _1283 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1270, _68), _1267), spvFMul(_1270, _69)), spvFMul(_1271, _68)), spvFMul(_1271, _69)), spvFMul(Mapping.currentCube[1u].x, 0.0)), _80), _82);
                float _1284 = spvFAdd(_1267, _1283);
                float _1286 = spvFSub(_1283, spvFSub(_1284, _1267));
                float _1287 = spvFAdd(0.0, _1284);
                float _1288 = spvFSub(_1287, 0.0);
                float _1293 = spvFAdd(0.0, _1286);
                float _1294 = spvFSub(_1293, 0.0);
                float _1299 = spvFAdd(spvFAdd(spvFSub(0.0, spvFSub(_1287, _1288)), spvFSub(_1284, _1288)), _1293);
                float _1300 = spvFAdd(_1287, _1299);
                float _1303 = spvFAdd(spvFSub(_1299, spvFSub(_1300, _1287)), spvFAdd(spvFSub(0.0, spvFSub(_1293, _1294)), spvFSub(_1286, _1294)));
                float _1304 = spvFAdd(_1300, _1303);
                float _1306 = spvFSub(_1303, spvFSub(_1304, _1300));
                float _1307 = spvFMul(Mapping.currentCube[1u].y, _2143);
                float _1308 = spvFMul(Mapping.currentCube[1u].y, 4097.0);
                float _1310 = spvFSub(_1308, spvFSub(_1308, Mapping.currentCube[1u].y));
                float _1311 = spvFSub(Mapping.currentCube[1u].y, _1310);
                float _1323 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1310, _1184), _1307), spvFMul(_1310, _1185)), spvFMul(_1311, _1184)), spvFMul(_1311, _1185)), spvFMul(Mapping.currentCube[1u].y, 0.0)), _1196), _82);
                float _1324 = spvFAdd(_1307, _1323);
                float _1326 = spvFSub(_1323, spvFSub(_1324, _1307));
                float _1327 = spvFAdd(_1304, _1324);
                float _1328 = spvFSub(_1327, _1304);
                float _1333 = spvFAdd(_1306, _1326);
                float _1334 = spvFSub(_1333, _1306);
                float _1339 = spvFAdd(spvFAdd(spvFSub(_1304, spvFSub(_1327, _1328)), spvFSub(_1324, _1328)), _1333);
                float _1340 = spvFAdd(_1327, _1339);
                float _1343 = spvFAdd(spvFSub(_1339, spvFSub(_1340, _1327)), spvFAdd(spvFSub(_1306, spvFSub(_1333, _1334)), spvFSub(_1326, _1334)));
                float _1344 = spvFAdd(_1340, _1343);
                float _1346 = spvFSub(_1343, spvFSub(_1344, _1340));
                float _1347 = spvFMul(Mapping.currentCube[1u].z, _2146);
                float _1348 = spvFMul(Mapping.currentCube[1u].z, 4097.0);
                float _1350 = spvFSub(_1348, spvFSub(_1348, Mapping.currentCube[1u].z));
                float _1351 = spvFSub(Mapping.currentCube[1u].z, _1350);
                float _1363 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1350, _1229), _1347), spvFMul(_1350, _1230)), spvFMul(_1351, _1229)), spvFMul(_1351, _1230)), spvFMul(Mapping.currentCube[1u].z, 0.0)), _1241), _82);
                float _1364 = spvFAdd(_1347, _1363);
                float _1366 = spvFSub(_1363, spvFSub(_1364, _1347));
                float _1367 = spvFAdd(_1344, _1364);
                float _1368 = spvFSub(_1367, _1344);
                float _1373 = spvFAdd(_1346, _1366);
                float _1374 = spvFSub(_1373, _1346);
                float _1379 = spvFAdd(spvFAdd(spvFSub(_1344, spvFSub(_1367, _1368)), spvFSub(_1364, _1368)), _1373);
                float _1380 = spvFAdd(_1367, _1379);
                float _1383 = spvFAdd(spvFSub(_1379, spvFSub(_1380, _1367)), spvFAdd(spvFSub(_1346, spvFSub(_1373, _1374)), spvFSub(_1366, _1374)));
                float _1384 = spvFAdd(_1380, _1383);
                float _1386 = spvFSub(_1383, spvFSub(_1384, _1380));
                float _1387 = spvFMul(Mapping.currentCube[2u].x, _2140);
                float _1388 = spvFMul(Mapping.currentCube[2u].x, 4097.0);
                float _1390 = spvFSub(_1388, spvFSub(_1388, Mapping.currentCube[2u].x));
                float _1391 = spvFSub(Mapping.currentCube[2u].x, _1390);
                float _1403 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1390, _68), _1387), spvFMul(_1390, _69)), spvFMul(_1391, _68)), spvFMul(_1391, _69)), spvFMul(Mapping.currentCube[2u].x, 0.0)), _80), _82);
                float _1404 = spvFAdd(_1387, _1403);
                float _1406 = spvFSub(_1403, spvFSub(_1404, _1387));
                float _1407 = spvFAdd(0.0, _1404);
                float _1408 = spvFSub(_1407, 0.0);
                float _1413 = spvFAdd(0.0, _1406);
                float _1414 = spvFSub(_1413, 0.0);
                float _1419 = spvFAdd(spvFAdd(spvFSub(0.0, spvFSub(_1407, _1408)), spvFSub(_1404, _1408)), _1413);
                float _1420 = spvFAdd(_1407, _1419);
                float _1423 = spvFAdd(spvFSub(_1419, spvFSub(_1420, _1407)), spvFAdd(spvFSub(0.0, spvFSub(_1413, _1414)), spvFSub(_1406, _1414)));
                float _1424 = spvFAdd(_1420, _1423);
                float _1426 = spvFSub(_1423, spvFSub(_1424, _1420));
                float _1427 = spvFMul(Mapping.currentCube[2u].y, _2143);
                float _1428 = spvFMul(Mapping.currentCube[2u].y, 4097.0);
                float _1430 = spvFSub(_1428, spvFSub(_1428, Mapping.currentCube[2u].y));
                float _1431 = spvFSub(Mapping.currentCube[2u].y, _1430);
                float _1443 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1430, _1184), _1427), spvFMul(_1430, _1185)), spvFMul(_1431, _1184)), spvFMul(_1431, _1185)), spvFMul(Mapping.currentCube[2u].y, 0.0)), _1196), _82);
                float _1444 = spvFAdd(_1427, _1443);
                float _1446 = spvFSub(_1443, spvFSub(_1444, _1427));
                float _1447 = spvFAdd(_1424, _1444);
                float _1448 = spvFSub(_1447, _1424);
                float _1453 = spvFAdd(_1426, _1446);
                float _1454 = spvFSub(_1453, _1426);
                float _1459 = spvFAdd(spvFAdd(spvFSub(_1424, spvFSub(_1447, _1448)), spvFSub(_1444, _1448)), _1453);
                float _1460 = spvFAdd(_1447, _1459);
                float _1463 = spvFAdd(spvFSub(_1459, spvFSub(_1460, _1447)), spvFAdd(spvFSub(_1426, spvFSub(_1453, _1454)), spvFSub(_1446, _1454)));
                float _1464 = spvFAdd(_1460, _1463);
                float _1466 = spvFSub(_1463, spvFSub(_1464, _1460));
                float _1467 = spvFMul(Mapping.currentCube[2u].z, _2146);
                float _1468 = spvFMul(Mapping.currentCube[2u].z, 4097.0);
                float _1470 = spvFSub(_1468, spvFSub(_1468, Mapping.currentCube[2u].z));
                float _1471 = spvFSub(Mapping.currentCube[2u].z, _1470);
                float _1483 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1470, _1229), _1467), spvFMul(_1470, _1230)), spvFMul(_1471, _1229)), spvFMul(_1471, _1230)), spvFMul(Mapping.currentCube[2u].z, 0.0)), _1241), _82);
                float _1484 = spvFAdd(_1467, _1483);
                float _1486 = spvFSub(_1483, spvFSub(_1484, _1467));
                float _1487 = spvFAdd(_1464, _1484);
                float _1488 = spvFSub(_1487, _1464);
                float _1493 = spvFAdd(_1466, _1486);
                float _1494 = spvFSub(_1493, _1466);
                float _1499 = spvFAdd(spvFAdd(spvFSub(_1464, spvFSub(_1487, _1488)), spvFSub(_1484, _1488)), _1493);
                float _1500 = spvFAdd(_1487, _1499);
                float _1503 = spvFAdd(spvFSub(_1499, spvFSub(_1500, _1487)), spvFAdd(spvFSub(_1466, spvFSub(_1493, _1494)), spvFSub(_1486, _1494)));
                float _1504 = spvFAdd(_1500, _1503);
                float _1506 = spvFSub(_1503, spvFSub(_1504, _1500));
                float _107 = spvFMul(Mapping.previousCube[0u].x, _1264);
                float _108 = spvFMul(Mapping.previousCube[0u].x, 4097.0);
                float _110 = spvFSub(_108, spvFSub(_108, Mapping.previousCube[0u].x));
                float _111 = spvFSub(Mapping.previousCube[0u].x, _110);
                float _112 = spvFMul(_1264, 4097.0);
                float _114 = spvFSub(_112, spvFSub(_112, _1264));
                float _115 = spvFSub(_1264, _114);
                float _126 = spvFMul(0.0, _1264);
                float _128 = spvFMul(0.0, _1266);
                float _129 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_110, _114), _107), spvFMul(_110, _115)), spvFMul(_111, _114)), spvFMul(_111, _115)), spvFMul(Mapping.previousCube[0u].x, _1266)), _126), _128);
                float _130 = spvFAdd(_107, _129);
                float _132 = spvFSub(_129, spvFSub(_130, _107));
                float _133 = spvFAdd(0.0, _130);
                float _134 = spvFSub(_133, 0.0);
                float _139 = spvFAdd(0.0, _132);
                float _140 = spvFSub(_139, 0.0);
                float _145 = spvFAdd(spvFAdd(spvFSub(0.0, spvFSub(_133, _134)), spvFSub(_130, _134)), _139);
                float _146 = spvFAdd(_133, _145);
                float _149 = spvFAdd(spvFSub(_145, spvFSub(_146, _133)), spvFAdd(spvFSub(0.0, spvFSub(_139, _140)), spvFSub(_132, _140)));
                float _150 = spvFAdd(_146, _149);
                float _152 = spvFSub(_149, spvFSub(_150, _146));
                float _845 = spvFMul(Mapping.previousCube[1u].x, _1384);
                float _846 = spvFMul(Mapping.previousCube[1u].x, 4097.0);
                float _848 = spvFSub(_846, spvFSub(_846, Mapping.previousCube[1u].x));
                float _849 = spvFSub(Mapping.previousCube[1u].x, _848);
                float _850 = spvFMul(_1384, 4097.0);
                float _852 = spvFSub(_850, spvFSub(_850, _1384));
                float _853 = spvFSub(_1384, _852);
                float _864 = spvFMul(0.0, _1384);
                float _866 = spvFMul(0.0, _1386);
                float _867 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_848, _852), _845), spvFMul(_848, _853)), spvFMul(_849, _852)), spvFMul(_849, _853)), spvFMul(Mapping.previousCube[1u].x, _1386)), _864), _866);
                float _868 = spvFAdd(_845, _867);
                float _870 = spvFSub(_867, spvFSub(_868, _845));
                float _871 = spvFAdd(_150, _868);
                float _872 = spvFSub(_871, _150);
                float _877 = spvFAdd(_152, _870);
                float _878 = spvFSub(_877, _152);
                float _883 = spvFAdd(spvFAdd(spvFSub(_150, spvFSub(_871, _872)), spvFSub(_868, _872)), _877);
                float _884 = spvFAdd(_871, _883);
                float _887 = spvFAdd(spvFSub(_883, spvFSub(_884, _871)), spvFAdd(spvFSub(_152, spvFSub(_877, _878)), spvFSub(_870, _878)));
                float _888 = spvFAdd(_884, _887);
                float _890 = spvFSub(_887, spvFSub(_888, _884));
                float _891 = spvFMul(Mapping.previousCube[2u].x, _1504);
                float _892 = spvFMul(Mapping.previousCube[2u].x, 4097.0);
                float _894 = spvFSub(_892, spvFSub(_892, Mapping.previousCube[2u].x));
                float _895 = spvFSub(Mapping.previousCube[2u].x, _894);
                float _896 = spvFMul(_1504, 4097.0);
                float _898 = spvFSub(_896, spvFSub(_896, _1504));
                float _899 = spvFSub(_1504, _898);
                float _910 = spvFMul(0.0, _1504);
                float _912 = spvFMul(0.0, _1506);
                float _913 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_894, _898), _891), spvFMul(_894, _899)), spvFMul(_895, _898)), spvFMul(_895, _899)), spvFMul(Mapping.previousCube[2u].x, _1506)), _910), _912);
                float _914 = spvFAdd(_891, _913);
                float _916 = spvFSub(_913, spvFSub(_914, _891));
                float _917 = spvFAdd(_888, _914);
                float _918 = spvFSub(_917, _888);
                float _923 = spvFAdd(_890, _916);
                float _924 = spvFSub(_923, _890);
                float _929 = spvFAdd(spvFAdd(spvFSub(_888, spvFSub(_917, _918)), spvFSub(_914, _918)), _923);
                float _930 = spvFAdd(_917, _929);
                float _933 = spvFAdd(spvFSub(_929, spvFSub(_930, _917)), spvFAdd(spvFSub(_890, spvFSub(_923, _924)), spvFSub(_916, _924)));
                float _934 = spvFAdd(_930, _933);
                float _936 = spvFSub(_933, spvFSub(_934, _930));
                float _937 = spvFMul(Mapping.previousCube[0u].y, _1264);
                float _938 = spvFMul(Mapping.previousCube[0u].y, 4097.0);
                float _940 = spvFSub(_938, spvFSub(_938, Mapping.previousCube[0u].y));
                float _941 = spvFSub(Mapping.previousCube[0u].y, _940);
                float _953 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_940, _114), _937), spvFMul(_940, _115)), spvFMul(_941, _114)), spvFMul(_941, _115)), spvFMul(Mapping.previousCube[0u].y, _1266)), _126), _128);
                float _954 = spvFAdd(_937, _953);
                float _956 = spvFSub(_953, spvFSub(_954, _937));
                float _957 = spvFAdd(0.0, _954);
                float _958 = spvFSub(_957, 0.0);
                float _963 = spvFAdd(0.0, _956);
                float _964 = spvFSub(_963, 0.0);
                float _969 = spvFAdd(spvFAdd(spvFSub(0.0, spvFSub(_957, _958)), spvFSub(_954, _958)), _963);
                float _970 = spvFAdd(_957, _969);
                float _973 = spvFAdd(spvFSub(_969, spvFSub(_970, _957)), spvFAdd(spvFSub(0.0, spvFSub(_963, _964)), spvFSub(_956, _964)));
                float _974 = spvFAdd(_970, _973);
                float _976 = spvFSub(_973, spvFSub(_974, _970));
                float _977 = spvFMul(Mapping.previousCube[1u].y, _1384);
                float _978 = spvFMul(Mapping.previousCube[1u].y, 4097.0);
                float _980 = spvFSub(_978, spvFSub(_978, Mapping.previousCube[1u].y));
                float _981 = spvFSub(Mapping.previousCube[1u].y, _980);
                float _993 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_980, _852), _977), spvFMul(_980, _853)), spvFMul(_981, _852)), spvFMul(_981, _853)), spvFMul(Mapping.previousCube[1u].y, _1386)), _864), _866);
                float _994 = spvFAdd(_977, _993);
                float _996 = spvFSub(_993, spvFSub(_994, _977));
                float _997 = spvFAdd(_974, _994);
                float _998 = spvFSub(_997, _974);
                float _1003 = spvFAdd(_976, _996);
                float _1004 = spvFSub(_1003, _976);
                float _1009 = spvFAdd(spvFAdd(spvFSub(_974, spvFSub(_997, _998)), spvFSub(_994, _998)), _1003);
                float _1010 = spvFAdd(_997, _1009);
                float _1013 = spvFAdd(spvFSub(_1009, spvFSub(_1010, _997)), spvFAdd(spvFSub(_976, spvFSub(_1003, _1004)), spvFSub(_996, _1004)));
                float _1014 = spvFAdd(_1010, _1013);
                float _1016 = spvFSub(_1013, spvFSub(_1014, _1010));
                float _1017 = spvFMul(Mapping.previousCube[2u].y, _1504);
                float _1018 = spvFMul(Mapping.previousCube[2u].y, 4097.0);
                float _1020 = spvFSub(_1018, spvFSub(_1018, Mapping.previousCube[2u].y));
                float _1021 = spvFSub(Mapping.previousCube[2u].y, _1020);
                float _1033 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1020, _898), _1017), spvFMul(_1020, _899)), spvFMul(_1021, _898)), spvFMul(_1021, _899)), spvFMul(Mapping.previousCube[2u].y, _1506)), _910), _912);
                float _1034 = spvFAdd(_1017, _1033);
                float _1036 = spvFSub(_1033, spvFSub(_1034, _1017));
                float _1037 = spvFAdd(_1014, _1034);
                float _1038 = spvFSub(_1037, _1014);
                float _1043 = spvFAdd(_1016, _1036);
                float _1044 = spvFSub(_1043, _1016);
                float _1049 = spvFAdd(spvFAdd(spvFSub(_1014, spvFSub(_1037, _1038)), spvFSub(_1034, _1038)), _1043);
                float _1050 = spvFAdd(_1037, _1049);
                float _1053 = spvFAdd(spvFSub(_1049, spvFSub(_1050, _1037)), spvFAdd(spvFSub(_1016, spvFSub(_1043, _1044)), spvFSub(_1036, _1044)));
                float _1054 = spvFAdd(_1050, _1053);
                float _1056 = spvFSub(_1053, spvFSub(_1054, _1050));
                float _1057 = spvFMul(Mapping.previousCube[0u].z, _1264);
                float _1058 = spvFMul(Mapping.previousCube[0u].z, 4097.0);
                float _1060 = spvFSub(_1058, spvFSub(_1058, Mapping.previousCube[0u].z));
                float _1061 = spvFSub(Mapping.previousCube[0u].z, _1060);
                float _1073 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1060, _114), _1057), spvFMul(_1060, _115)), spvFMul(_1061, _114)), spvFMul(_1061, _115)), spvFMul(Mapping.previousCube[0u].z, _1266)), _126), _128);
                float _1074 = spvFAdd(_1057, _1073);
                float _1076 = spvFSub(_1073, spvFSub(_1074, _1057));
                float _1077 = spvFAdd(0.0, _1074);
                float _1078 = spvFSub(_1077, 0.0);
                float _1083 = spvFAdd(0.0, _1076);
                float _1084 = spvFSub(_1083, 0.0);
                float _1089 = spvFAdd(spvFAdd(spvFSub(0.0, spvFSub(_1077, _1078)), spvFSub(_1074, _1078)), _1083);
                float _1090 = spvFAdd(_1077, _1089);
                float _1093 = spvFAdd(spvFSub(_1089, spvFSub(_1090, _1077)), spvFAdd(spvFSub(0.0, spvFSub(_1083, _1084)), spvFSub(_1076, _1084)));
                float _1094 = spvFAdd(_1090, _1093);
                float _1096 = spvFSub(_1093, spvFSub(_1094, _1090));
                float _1097 = spvFMul(Mapping.previousCube[1u].z, _1384);
                float _1098 = spvFMul(Mapping.previousCube[1u].z, 4097.0);
                float _1100 = spvFSub(_1098, spvFSub(_1098, Mapping.previousCube[1u].z));
                float _1101 = spvFSub(Mapping.previousCube[1u].z, _1100);
                float _1113 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1100, _852), _1097), spvFMul(_1100, _853)), spvFMul(_1101, _852)), spvFMul(_1101, _853)), spvFMul(Mapping.previousCube[1u].z, _1386)), _864), _866);
                float _1114 = spvFAdd(_1097, _1113);
                float _1116 = spvFSub(_1113, spvFSub(_1114, _1097));
                float _1117 = spvFAdd(_1094, _1114);
                float _1118 = spvFSub(_1117, _1094);
                float _1123 = spvFAdd(_1096, _1116);
                float _1124 = spvFSub(_1123, _1096);
                float _1129 = spvFAdd(spvFAdd(spvFSub(_1094, spvFSub(_1117, _1118)), spvFSub(_1114, _1118)), _1123);
                float _1130 = spvFAdd(_1117, _1129);
                float _1133 = spvFAdd(spvFSub(_1129, spvFSub(_1130, _1117)), spvFAdd(spvFSub(_1096, spvFSub(_1123, _1124)), spvFSub(_1116, _1124)));
                float _1134 = spvFAdd(_1130, _1133);
                float _1136 = spvFSub(_1133, spvFSub(_1134, _1130));
                float _1137 = spvFMul(Mapping.previousCube[2u].z, _1504);
                float _1138 = spvFMul(Mapping.previousCube[2u].z, 4097.0);
                float _1140 = spvFSub(_1138, spvFSub(_1138, Mapping.previousCube[2u].z));
                float _1141 = spvFSub(Mapping.previousCube[2u].z, _1140);
                float _1153 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_1140, _898), _1137), spvFMul(_1140, _899)), spvFMul(_1141, _898)), spvFMul(_1141, _899)), spvFMul(Mapping.previousCube[2u].z, _1506)), _910), _912);
                float _1154 = spvFAdd(_1137, _1153);
                float _1156 = spvFSub(_1153, spvFSub(_1154, _1137));
                float _1157 = spvFAdd(_1134, _1154);
                float _1158 = spvFSub(_1157, _1134);
                float _1163 = spvFAdd(_1136, _1156);
                float _1164 = spvFSub(_1163, _1136);
                float _1169 = spvFAdd(spvFAdd(spvFSub(_1134, spvFSub(_1157, _1158)), spvFSub(_1154, _1158)), _1163);
                float _1170 = spvFAdd(_1157, _1169);
                float _1173 = spvFAdd(spvFSub(_1169, spvFSub(_1170, _1157)), spvFAdd(spvFSub(_1136, spvFSub(_1163, _1164)), spvFSub(_1156, _1164)));
                float _1174 = spvFAdd(_1170, _1173);
                float _1176 = spvFSub(_1173, spvFSub(_1174, _1170));
                float _153 = spvFMul(_934, _934);
                float _154 = spvFMul(_934, 4097.0);
                float _156 = spvFSub(_154, spvFSub(_154, _934));
                float _157 = spvFSub(_934, _156);
                float _160 = spvFMul(_156, _157);
                float _165 = spvFMul(_934, _936);
                float _169 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_156, _156), _153), _160), _160), spvFMul(_157, _157)), _165), _165), spvFMul(_936, _936));
                float _170 = spvFAdd(_153, _169);
                float _172 = spvFSub(_169, spvFSub(_170, _153));
                float _173 = spvFAdd(0.0, _170);
                float _174 = spvFSub(_173, 0.0);
                float _179 = spvFAdd(0.0, _172);
                float _180 = spvFSub(_179, 0.0);
                float _185 = spvFAdd(spvFAdd(spvFSub(0.0, spvFSub(_173, _174)), spvFSub(_170, _174)), _179);
                float _186 = spvFAdd(_173, _185);
                float _189 = spvFAdd(spvFSub(_185, spvFSub(_186, _173)), spvFAdd(spvFSub(0.0, spvFSub(_179, _180)), spvFSub(_172, _180)));
                float _190 = spvFAdd(_186, _189);
                float _192 = spvFSub(_189, spvFSub(_190, _186));
                float _765 = spvFMul(_1054, _1054);
                float _766 = spvFMul(_1054, 4097.0);
                float _768 = spvFSub(_766, spvFSub(_766, _1054));
                float _769 = spvFSub(_1054, _768);
                float _772 = spvFMul(_768, _769);
                float _777 = spvFMul(_1054, _1056);
                float _781 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_768, _768), _765), _772), _772), spvFMul(_769, _769)), _777), _777), spvFMul(_1056, _1056));
                float _782 = spvFAdd(_765, _781);
                float _784 = spvFSub(_781, spvFSub(_782, _765));
                float _785 = spvFAdd(_190, _782);
                float _786 = spvFSub(_785, _190);
                float _791 = spvFAdd(_192, _784);
                float _792 = spvFSub(_791, _192);
                float _797 = spvFAdd(spvFAdd(spvFSub(_190, spvFSub(_785, _786)), spvFSub(_782, _786)), _791);
                float _798 = spvFAdd(_785, _797);
                float _801 = spvFAdd(spvFSub(_797, spvFSub(_798, _785)), spvFAdd(spvFSub(_192, spvFSub(_791, _792)), spvFSub(_784, _792)));
                float _802 = spvFAdd(_798, _801);
                float _804 = spvFSub(_801, spvFSub(_802, _798));
                float _805 = spvFMul(_1174, _1174);
                float _806 = spvFMul(_1174, 4097.0);
                float _808 = spvFSub(_806, spvFSub(_806, _1174));
                float _809 = spvFSub(_1174, _808);
                float _812 = spvFMul(_808, _809);
                float _817 = spvFMul(_1174, _1176);
                float _821 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_808, _808), _805), _812), _812), spvFMul(_809, _809)), _817), _817), spvFMul(_1176, _1176));
                float _822 = spvFAdd(_805, _821);
                float _824 = spvFSub(_821, spvFSub(_822, _805));
                float _825 = spvFAdd(_802, _822);
                float _826 = spvFSub(_825, _802);
                float _831 = spvFAdd(_804, _824);
                float _832 = spvFSub(_831, _804);
                float _837 = spvFAdd(spvFAdd(spvFSub(_802, spvFSub(_825, _826)), spvFSub(_822, _826)), _831);
                float _838 = spvFAdd(_825, _837);
                float _841 = spvFAdd(spvFSub(_837, spvFSub(_838, _825)), spvFAdd(spvFSub(_804, spvFSub(_831, _832)), spvFSub(_824, _832)));
                float _842 = spvFAdd(_838, _841);
                float _844 = spvFSub(_841, spvFSub(_842, _838));
                float2 _2177 = float2(_842, _844);
                bool2 _2178 = isnan(_2177);
                bool2 _2179 = isinf(_2177);
                bool _2195;
                if (all(not(bool2(_2178.x || _2179.x, _2178.y || _2179.y))))
                {
                    bool _2193;
                    if ((isunordered(9.9999996826552253889678874634872e-21, _842) || 9.9999996826552253889678874634872e-21 >= _842))
                    {
                        bool _2192;
                        if (9.9999996826552253889678874634872e-21 == _842)
                        {
                            _2192 = 0.0 < _844;
                        }
                        else
                        {
                            _2192 = false;
                        }
                        _2193 = _2192;
                    }
                    else
                    {
                        _2193 = true;
                    }
                    _2195 = !_2193;
                }
                else
                {
                    _2195 = true;
                }
                if (_2195)
                {
                    _2203 = float3(0.0);
                    break;
                }
                float _2198 = precise::sqrt(_842);
                float _194 = spvFMul(_2198, _2198);
                float _195 = spvFMul(_2198, 4097.0);
                float _197 = spvFSub(_195, spvFSub(_195, _2198));
                float _198 = spvFSub(_2198, _197);
                float _201 = spvFMul(_197, _198);
                float _206 = spvFMul(_2198, 0.0);
                float _209 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_197, _197), _194), _201), _201), spvFMul(_198, _198)), _206), _206), _82);
                float _210 = spvFAdd(_194, _209);
                float2 _213 = -float2(_210, spvFSub(_209, spvFSub(_210, _194)));
                float _2200 = _213.x;
                float _2201 = _213.y;
                float _214 = spvFAdd(_842, _2200);
                float _215 = spvFSub(_214, _842);
                float _220 = spvFAdd(_844, _2201);
                float _221 = spvFSub(_220, _844);
                float _226 = spvFAdd(spvFAdd(spvFSub(_842, spvFSub(_214, _215)), spvFSub(_2200, _215)), _220);
                float _227 = spvFAdd(_214, _226);
                float _230 = spvFAdd(spvFSub(_226, spvFSub(_227, _214)), spvFAdd(spvFSub(_844, spvFSub(_220, _221)), spvFSub(_2201, _221)));
                float _231 = spvFAdd(_227, _230);
                float _233 = spvFSub(_230, spvFSub(_231, _227));
                float _193 = spvFMul(2.0, _2198);
                float _234 = _231 / _193;
                float _242 = spvFMul(_234, _193);
                float _243 = spvFMul(_234, 4097.0);
                float _245 = spvFSub(_243, spvFSub(_243, _234));
                float _246 = spvFSub(_234, _245);
                float _247 = spvFMul(_193, 4097.0);
                float _249 = spvFSub(_247, spvFSub(_247, _193));
                float _250 = spvFSub(_193, _249);
                float _263 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_245, _249), _242), spvFMul(_245, _250)), spvFMul(_246, _249)), spvFMul(_246, _250)), spvFMul(_234, 0.0)), spvFMul(0.0, _193)), _82);
                float _264 = spvFAdd(_242, _263);
                float _235 = -_264;
                float _236 = -spvFSub(_263, spvFSub(_264, _242));
                float _267 = spvFAdd(_231, _235);
                float _268 = spvFSub(_267, _231);
                float _273 = spvFAdd(_233, _236);
                float _274 = spvFSub(_273, _233);
                float _279 = spvFAdd(spvFAdd(spvFSub(_231, spvFSub(_267, _268)), spvFSub(_235, _268)), _273);
                float _280 = spvFAdd(_267, _279);
                float _237 = spvFAdd(_280, spvFAdd(spvFSub(_279, spvFSub(_280, _267)), spvFAdd(spvFSub(_233, spvFSub(_273, _274)), spvFSub(_236, _274)))) / _193;
                float _285 = spvFAdd(_234, _237);
                float _286 = spvFSub(_285, _234);
                float _291 = spvFAdd(0.0, 0.0);
                float _292 = spvFSub(_291, 0.0);
                float _296 = spvFAdd(spvFSub(0.0, spvFSub(_291, _292)), spvFSub(0.0, _292));
                float _297 = spvFAdd(spvFAdd(spvFSub(_234, spvFSub(_285, _286)), spvFSub(_237, _286)), _291);
                float _298 = spvFAdd(_285, _297);
                float _301 = spvFAdd(spvFSub(_297, spvFSub(_298, _285)), _296);
                float _302 = spvFAdd(_298, _301);
                float _304 = spvFSub(_301, spvFSub(_302, _298));
                float _305 = spvFMul(_302, _193);
                float _306 = spvFMul(_302, 4097.0);
                float _308 = spvFSub(_306, spvFSub(_306, _302));
                float _309 = spvFSub(_302, _308);
                float _323 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_308, _249), _305), spvFMul(_308, _250)), spvFMul(_309, _249)), spvFMul(_309, _250)), spvFMul(_302, 0.0)), spvFMul(_304, _193)), spvFMul(_304, 0.0));
                float _324 = spvFAdd(_305, _323);
                float _238 = -_324;
                float _239 = -spvFSub(_323, spvFSub(_324, _305));
                float _327 = spvFAdd(_231, _238);
                float _328 = spvFSub(_327, _231);
                float _333 = spvFAdd(_233, _239);
                float _334 = spvFSub(_333, _233);
                float _339 = spvFAdd(spvFAdd(spvFSub(_231, spvFSub(_327, _328)), spvFSub(_238, _328)), _333);
                float _340 = spvFAdd(_327, _339);
                float _343 = spvFAdd(spvFSub(_339, spvFSub(_340, _327)), spvFAdd(spvFSub(_233, spvFSub(_333, _334)), spvFSub(_239, _334)));
                float _344 = spvFAdd(_340, _343);
                float _241 = spvFAdd(_344, spvFSub(_343, spvFSub(_344, _340))) / _193;
                float _347 = spvFAdd(_302, _241);
                float _348 = spvFSub(_347, _302);
                float _353 = spvFAdd(_304, 0.0);
                float _354 = spvFSub(_353, _304);
                float _359 = spvFAdd(spvFAdd(spvFSub(_302, spvFSub(_347, _348)), spvFSub(_241, _348)), _353);
                float _360 = spvFAdd(_347, _359);
                float _363 = spvFAdd(spvFSub(_359, spvFSub(_360, _347)), spvFAdd(spvFSub(_304, spvFSub(_353, _354)), spvFSub(0.0, _354)));
                float _364 = spvFAdd(_360, _363);
                float _366 = spvFSub(_363, spvFSub(_364, _360));
                float _367 = spvFAdd(_2198, _364);
                float _368 = spvFSub(_367, _2198);
                float _373 = spvFAdd(0.0, _366);
                float _374 = spvFSub(_373, 0.0);
                float _379 = spvFAdd(spvFAdd(spvFSub(_2198, spvFSub(_367, _368)), spvFSub(_364, _368)), _373);
                float _380 = spvFAdd(_367, _379);
                float _383 = spvFAdd(spvFSub(_379, spvFSub(_380, _367)), spvFAdd(spvFSub(0.0, spvFSub(_373, _374)), spvFSub(_366, _374)));
                float _384 = spvFAdd(_380, _383);
                float _386 = spvFSub(_383, spvFSub(_384, _380));
                float _387 = _934 / _384;
                float _395 = spvFMul(_387, _384);
                float _396 = spvFMul(_387, 4097.0);
                float _398 = spvFSub(_396, spvFSub(_396, _387));
                float _399 = spvFSub(_387, _398);
                float _400 = spvFMul(_384, 4097.0);
                float _402 = spvFSub(_400, spvFSub(_400, _384));
                float _403 = spvFSub(_384, _402);
                float _414 = spvFMul(0.0, _384);
                float _416 = spvFMul(0.0, _386);
                float _417 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_398, _402), _395), spvFMul(_398, _403)), spvFMul(_399, _402)), spvFMul(_399, _403)), spvFMul(_387, _386)), _414), _416);
                float _418 = spvFAdd(_395, _417);
                float _388 = -_418;
                float _389 = -spvFSub(_417, spvFSub(_418, _395));
                float _421 = spvFAdd(_934, _388);
                float _422 = spvFSub(_421, _934);
                float _427 = spvFAdd(_936, _389);
                float _428 = spvFSub(_427, _936);
                float _433 = spvFAdd(spvFAdd(spvFSub(_934, spvFSub(_421, _422)), spvFSub(_388, _422)), _427);
                float _434 = spvFAdd(_421, _433);
                float _390 = spvFAdd(_434, spvFAdd(spvFSub(_433, spvFSub(_434, _421)), spvFAdd(spvFSub(_936, spvFSub(_427, _428)), spvFSub(_389, _428)))) / _384;
                float _439 = spvFAdd(_387, _390);
                float _440 = spvFSub(_439, _387);
                float _445 = spvFAdd(spvFAdd(spvFSub(_387, spvFSub(_439, _440)), spvFSub(_390, _440)), _291);
                float _446 = spvFAdd(_439, _445);
                float _449 = spvFAdd(spvFSub(_445, spvFSub(_446, _439)), _296);
                float _450 = spvFAdd(_446, _449);
                float _452 = spvFSub(_449, spvFSub(_450, _446));
                float _453 = spvFMul(_450, _384);
                float _454 = spvFMul(_450, 4097.0);
                float _456 = spvFSub(_454, spvFSub(_454, _450));
                float _457 = spvFSub(_450, _456);
                float _471 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_456, _402), _453), spvFMul(_456, _403)), spvFMul(_457, _402)), spvFMul(_457, _403)), spvFMul(_450, _386)), spvFMul(_452, _384)), spvFMul(_452, _386));
                float _472 = spvFAdd(_453, _471);
                float _391 = -_472;
                float _392 = -spvFSub(_471, spvFSub(_472, _453));
                float _475 = spvFAdd(_934, _391);
                float _476 = spvFSub(_475, _934);
                float _481 = spvFAdd(_936, _392);
                float _482 = spvFSub(_481, _936);
                float _487 = spvFAdd(spvFAdd(spvFSub(_934, spvFSub(_475, _476)), spvFSub(_391, _476)), _481);
                float _488 = spvFAdd(_475, _487);
                float _491 = spvFAdd(spvFSub(_487, spvFSub(_488, _475)), spvFAdd(spvFSub(_936, spvFSub(_481, _482)), spvFSub(_392, _482)));
                float _492 = spvFAdd(_488, _491);
                float _394 = spvFAdd(_492, spvFSub(_491, spvFSub(_492, _488))) / _384;
                float _495 = spvFAdd(_450, _394);
                float _496 = spvFSub(_495, _450);
                float _501 = spvFAdd(_452, 0.0);
                float _502 = spvFSub(_501, _452);
                float _507 = spvFAdd(spvFAdd(spvFSub(_450, spvFSub(_495, _496)), spvFSub(_394, _496)), _501);
                float _508 = spvFAdd(_495, _507);
                float _511 = spvFAdd(spvFSub(_507, spvFSub(_508, _495)), spvFAdd(spvFSub(_452, spvFSub(_501, _502)), spvFSub(0.0, _502)));
                float _512 = spvFAdd(_508, _511);
                float _516 = _1054 / _384;
                float _524 = spvFMul(_516, _384);
                float _525 = spvFMul(_516, 4097.0);
                float _527 = spvFSub(_525, spvFSub(_525, _516));
                float _528 = spvFSub(_516, _527);
                float _540 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_527, _402), _524), spvFMul(_527, _403)), spvFMul(_528, _402)), spvFMul(_528, _403)), spvFMul(_516, _386)), _414), _416);
                float _541 = spvFAdd(_524, _540);
                float _517 = -_541;
                float _518 = -spvFSub(_540, spvFSub(_541, _524));
                float _544 = spvFAdd(_1054, _517);
                float _545 = spvFSub(_544, _1054);
                float _550 = spvFAdd(_1056, _518);
                float _551 = spvFSub(_550, _1056);
                float _556 = spvFAdd(spvFAdd(spvFSub(_1054, spvFSub(_544, _545)), spvFSub(_517, _545)), _550);
                float _557 = spvFAdd(_544, _556);
                float _519 = spvFAdd(_557, spvFAdd(spvFSub(_556, spvFSub(_557, _544)), spvFAdd(spvFSub(_1056, spvFSub(_550, _551)), spvFSub(_518, _551)))) / _384;
                float _562 = spvFAdd(_516, _519);
                float _563 = spvFSub(_562, _516);
                float _568 = spvFAdd(spvFAdd(spvFSub(_516, spvFSub(_562, _563)), spvFSub(_519, _563)), _291);
                float _569 = spvFAdd(_562, _568);
                float _572 = spvFAdd(spvFSub(_568, spvFSub(_569, _562)), _296);
                float _573 = spvFAdd(_569, _572);
                float _575 = spvFSub(_572, spvFSub(_573, _569));
                float _576 = spvFMul(_573, _384);
                float _577 = spvFMul(_573, 4097.0);
                float _579 = spvFSub(_577, spvFSub(_577, _573));
                float _580 = spvFSub(_573, _579);
                float _594 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_579, _402), _576), spvFMul(_579, _403)), spvFMul(_580, _402)), spvFMul(_580, _403)), spvFMul(_573, _386)), spvFMul(_575, _384)), spvFMul(_575, _386));
                float _595 = spvFAdd(_576, _594);
                float _520 = -_595;
                float _521 = -spvFSub(_594, spvFSub(_595, _576));
                float _598 = spvFAdd(_1054, _520);
                float _599 = spvFSub(_598, _1054);
                float _604 = spvFAdd(_1056, _521);
                float _605 = spvFSub(_604, _1056);
                float _610 = spvFAdd(spvFAdd(spvFSub(_1054, spvFSub(_598, _599)), spvFSub(_520, _599)), _604);
                float _611 = spvFAdd(_598, _610);
                float _614 = spvFAdd(spvFSub(_610, spvFSub(_611, _598)), spvFAdd(spvFSub(_1056, spvFSub(_604, _605)), spvFSub(_521, _605)));
                float _615 = spvFAdd(_611, _614);
                float _523 = spvFAdd(_615, spvFSub(_614, spvFSub(_615, _611))) / _384;
                float _618 = spvFAdd(_573, _523);
                float _619 = spvFSub(_618, _573);
                float _624 = spvFAdd(_575, 0.0);
                float _625 = spvFSub(_624, _575);
                float _630 = spvFAdd(spvFAdd(spvFSub(_573, spvFSub(_618, _619)), spvFSub(_523, _619)), _624);
                float _631 = spvFAdd(_618, _630);
                float _634 = spvFAdd(spvFSub(_630, spvFSub(_631, _618)), spvFAdd(spvFSub(_575, spvFSub(_624, _625)), spvFSub(0.0, _625)));
                float _635 = spvFAdd(_631, _634);
                float _639 = _1174 / _384;
                float _647 = spvFMul(_639, _384);
                float _648 = spvFMul(_639, 4097.0);
                float _650 = spvFSub(_648, spvFSub(_648, _639));
                float _651 = spvFSub(_639, _650);
                float _663 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_650, _402), _647), spvFMul(_650, _403)), spvFMul(_651, _402)), spvFMul(_651, _403)), spvFMul(_639, _386)), _414), _416);
                float _664 = spvFAdd(_647, _663);
                float _640 = -_664;
                float _641 = -spvFSub(_663, spvFSub(_664, _647));
                float _667 = spvFAdd(_1174, _640);
                float _668 = spvFSub(_667, _1174);
                float _673 = spvFAdd(_1176, _641);
                float _674 = spvFSub(_673, _1176);
                float _679 = spvFAdd(spvFAdd(spvFSub(_1174, spvFSub(_667, _668)), spvFSub(_640, _668)), _673);
                float _680 = spvFAdd(_667, _679);
                float _642 = spvFAdd(_680, spvFAdd(spvFSub(_679, spvFSub(_680, _667)), spvFAdd(spvFSub(_1176, spvFSub(_673, _674)), spvFSub(_641, _674)))) / _384;
                float _685 = spvFAdd(_639, _642);
                float _686 = spvFSub(_685, _639);
                float _691 = spvFAdd(spvFAdd(spvFSub(_639, spvFSub(_685, _686)), spvFSub(_642, _686)), _291);
                float _692 = spvFAdd(_685, _691);
                float _695 = spvFAdd(spvFSub(_691, spvFSub(_692, _685)), _296);
                float _696 = spvFAdd(_692, _695);
                float _698 = spvFSub(_695, spvFSub(_696, _692));
                float _699 = spvFMul(_696, _384);
                float _700 = spvFMul(_696, 4097.0);
                float _702 = spvFSub(_700, spvFSub(_700, _696));
                float _703 = spvFSub(_696, _702);
                float _717 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_702, _402), _699), spvFMul(_702, _403)), spvFMul(_703, _402)), spvFMul(_703, _403)), spvFMul(_696, _386)), spvFMul(_698, _384)), spvFMul(_698, _386));
                float _718 = spvFAdd(_699, _717);
                float _643 = -_718;
                float _644 = -spvFSub(_717, spvFSub(_718, _699));
                float _721 = spvFAdd(_1174, _643);
                float _722 = spvFSub(_721, _1174);
                float _727 = spvFAdd(_1176, _644);
                float _728 = spvFSub(_727, _1176);
                float _733 = spvFAdd(spvFAdd(spvFSub(_1174, spvFSub(_721, _722)), spvFSub(_643, _722)), _727);
                float _734 = spvFAdd(_721, _733);
                float _737 = spvFAdd(spvFSub(_733, spvFSub(_734, _721)), spvFAdd(spvFSub(_1176, spvFSub(_727, _728)), spvFSub(_644, _728)));
                float _738 = spvFAdd(_734, _737);
                float _646 = spvFAdd(_738, spvFSub(_737, spvFSub(_738, _734))) / _384;
                float _741 = spvFAdd(_696, _646);
                float _742 = spvFSub(_741, _696);
                float _747 = spvFAdd(_698, 0.0);
                float _748 = spvFSub(_747, _698);
                float _753 = spvFAdd(spvFAdd(spvFSub(_696, spvFSub(_741, _742)), spvFSub(_646, _742)), _747);
                float _754 = spvFAdd(_741, _753);
                float _757 = spvFAdd(spvFSub(_753, spvFSub(_754, _741)), spvFAdd(spvFSub(_698, spvFSub(_747, _748)), spvFSub(0.0, _748)));
                float _758 = spvFAdd(_754, _757);
                _2203 = float3(spvFAdd(_512, spvFSub(_511, spvFSub(_512, _508))), spvFAdd(_635, spvFSub(_634, spvFSub(_635, _631))), spvFAdd(_758, spvFSub(_757, spvFSub(_758, _754))));
                break;
            } while(false);
            bool _2243;
            if (_2135)
            {
                bool _2242;
                do
                {
                    bool3 _2208 = isnan(_2203);
                    bool3 _2209 = isinf(_2203);
                    if (!all(not(bool3(_2208.x || _2209.x, _2208.y || _2209.y, _2208.z || _2209.z))))
                    {
                        _2242 = false;
                        break;
                    }
                    if (_2093)
                    {
                        bool _2225;
                        if (_2203.z == 0.0)
                        {
                            _2225 = all(_2203.xy >= float2(0.0));
                        }
                        else
                        {
                            _2225 = false;
                        }
                        bool _2231;
                        if (_2225)
                        {
                            _2231 = spvFAdd(_2203.x, _2203.y) <= 1.0;
                        }
                        else
                        {
                            _2231 = false;
                        }
                        _2242 = _2231;
                        break;
                    }
                    bool _2236;
                    if (_1640 != 2u)
                    {
                        _2236 = _1640 == 3u;
                    }
                    else
                    {
                        _2236 = true;
                    }
                    bool _2241;
                    if (_2236)
                    {
                        _2241 = abs(spvFSub(dot(_2203, _2203), 1.0)) < 0.00010099999781232327222824096679688;
                    }
                    else
                    {
                        _2241 = false;
                    }
                    _2242 = _2241;
                    break;
                } while(false);
                _2243 = _2242;
            }
            else
            {
                _2243 = false;
            }
            _2244 = _2203;
            _2245 = current._m0[_1625];
            _2246 = _2243;
        }
        queries._m0[_17 >> 2u] = _2246 ? _1802 : 4294967295u;
        uint _2250 = (_17 + 4u) >> 2u;
        queries._m0[_2250] = _2091.x;
        queries._m0[_2250 + 1u] = _2091.y;
        queries._m0[_2250 + 2u] = _2091.z;
        queries._m0[_2250 + 3u] = _2091.w;
        uint _2259 = (_17 + 20u) >> 2u;
        queries._m0[_2259] = _1624;
        queries._m0[_2259 + 1u] = _2245;
        uint _2262 = (_17 + 28u) >> 2u;
        uint3 _2263 = as_type<uint3>(_2244);
        queries._m0[_2262] = _2263.x;
        queries._m0[_2262 + 1u] = _2263.y;
        queries._m0[_2262 + 2u] = _2263.z;
        uint _2270 = (_17 + 40u) >> 2u;
        queries._m0[_2270] = 0u;
        queries._m0[_2270 + 1u] = _13 % Mapping.lobes;
        break;
    } while(false);
}

