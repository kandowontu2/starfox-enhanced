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

struct type_WitnessSettings
{
    uint witnessStride;
    uint witnessOffset;
    uint witnessBytes;
    uint witnessReserved;
};

struct type_TargetSettings
{
    float4 targetCurrentCube[3];
    float4 targetPreviousCube[3];
};

struct WitnessQuery
{
    uint primary;
    uint4 mirrors;
    uint control;
    uint terminal;
    uint lobe;
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

struct WitnessTap
{
    float depth;
    float3 feature;
    short same;
    short visible;
};

static inline __attribute__((always_inline))
bool interval_exact_point(thread const Interval& a, thread const float& x)
{
    if (x == 0.0)
    {
        return ((as_type<uint>(a.lo) | as_type<uint>(a.hi)) & 2147483647u) == 0u;
    }
    bool _3826;
    if (as_type<uint>(a.lo) == as_type<uint>(x))
    {
        _3826 = as_type<uint>(a.hi) == as_type<uint>(x);
    }
    else
    {
        _3826 = false;
    }
    return _3826;
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
    uint _3853;
    if (x > 0.0)
    {
        _3853 = 1u;
    }
    else
    {
        _3853 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _3853);
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
    uint _3839;
    if (x < 0.0)
    {
        _3839 = 1u;
    }
    else
    {
        _3839 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _3839);
}

static __attribute__((noinline))
float optical_add_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    uint _3960 = as_type<uint>(a) & 2147483647u;
    uint _3963 = as_type<uint>(b) & 2147483647u;
    bool _3966;
    if (_3960 < 2139095040u)
    {
        _3966 = _3963 < 2139095040u;
    }
    else
    {
        _3966 = false;
    }
    if (_3966)
    {
        if (_3960 == 0u)
        {
            return b;
        }
        if (_3963 == 0u)
        {
            return a;
        }
        bool _3973;
        if (_3960 >= 8388608u)
        {
            _3973 = _3963 < 8388608u;
        }
        else
        {
            _3973 = true;
        }
        if (_3973)
        {
            intervalFailed = true;
        }
        bool _3976;
        if (_3960 >= 562036736u)
        {
            _3976 = _3960 <= 1568669696u;
        }
        else
        {
            _3976 = false;
        }
        bool _3978;
        if (_3976)
        {
            _3978 = _3963 >= 562036736u;
        }
        else
        {
            _3978 = false;
        }
        bool _3980;
        if (_3978)
        {
            _3980 = _3963 <= 1568669696u;
        }
        else
        {
            _3980 = false;
        }
        if (_3980)
        {
            float _3984;
            if (_3960 >= _3963)
            {
                _3984 = a;
            }
            else
            {
                _3984 = b;
            }
            float _3988;
            if (_3960 >= _3963)
            {
                _3988 = b;
            }
            else
            {
                _3988 = a;
            }
            float _773 = spvFAdd(_3984, _3988);
            float _775 = spvFSub(_3988, spvFSub(_773, _3984));
            if (upper)
            {
                float _3992;
                if (_775 > 0.0)
                {
                    float param_var_x = _773;
                    float _3991 = interval_up(param_var_x, intervalFailed);
                    _3992 = _3991;
                }
                else
                {
                    _3992 = _773;
                }
                return _3992;
            }
            float _3995;
            if (_775 < 0.0)
            {
                float param_var_x_1 = _773;
                float _3994 = interval_down(param_var_x_1, intervalFailed);
                _3995 = _3994;
            }
            else
            {
                _3995 = _773;
            }
            return _3995;
        }
    }
    float _776 = spvFAdd(a, b);
    float _4001;
    if (upper)
    {
        float param_var_x_2 = _776;
        float _3999 = interval_up(param_var_x_2, intervalFailed);
        _4001 = _3999;
    }
    else
    {
        float param_var_x_3 = _776;
        float _4000 = interval_down(param_var_x_3, intervalFailed);
        _4001 = _4000;
    }
    return _4001;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _3687;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3687 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3687 = false;
    }
    bool _3691;
    if (_3687)
    {
        _3691 = a.lo <= a.hi;
    }
    else
    {
        _3691 = false;
    }
    bool _3710;
    if (_3691)
    {
        bool _3705;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3705 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3705 = false;
        }
        bool _3709;
        if (_3705)
        {
            _3709 = b.lo <= b.hi;
        }
        else
        {
            _3709 = false;
        }
        _3710 = _3709;
    }
    else
    {
        _3710 = false;
    }
    if (_3710)
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
    float _3721 = optical_add_bound(param_var_a_2, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_3 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _3726 = optical_add_bound(param_var_a_3, param_var_b_1, param_var_upper_1, intervalFailed);
    return Interval{ _3721, _3726 };
}

static __attribute__((noinline))
float optical_product_bounds(thread const float& a, thread const float& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    uint _3856 = as_type<uint>(a);
    uint _3857 = _3856 & 2147483647u;
    uint _3859 = as_type<uint>(b);
    uint _3860 = _3859 & 2147483647u;
    bool _3863;
    if (_3857 < 2139095040u)
    {
        _3863 = _3860 < 2139095040u;
    }
    else
    {
        _3863 = false;
    }
    if (_3863)
    {
        bool _3866;
        if (_3857 != 0u)
        {
            _3866 = _3860 == 0u;
        }
        else
        {
            _3866 = true;
        }
        if (_3866)
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
            float _3883 = as_type<float>(as_type<uint>(b) ^ 2147483648u);
            optical_product_upper = _3883;
            return _3883;
        }
        if (as_type<uint>(b) == 3212836864u)
        {
            float _3890 = as_type<float>(as_type<uint>(a) ^ 2147483648u);
            optical_product_upper = _3890;
            return _3890;
        }
        bool _3893;
        if (_3857 >= 8388608u)
        {
            _3893 = _3860 < 8388608u;
        }
        else
        {
            _3893 = true;
        }
        if (_3893)
        {
            intervalFailed = true;
        }
        bool _3896;
        if (_3857 >= 813694976u)
        {
            _3896 = _3857 <= 1317011456u;
        }
        else
        {
            _3896 = false;
        }
        bool _3898;
        if (_3896)
        {
            _3898 = _3860 >= 813694976u;
        }
        else
        {
            _3898 = false;
        }
        bool _3900;
        if (_3898)
        {
            _3900 = _3860 <= 1317011456u;
        }
        else
        {
            _3900 = false;
        }
        if (_3900)
        {
            float _759 = spvFMul(a, b);
            uint _3907 = _3856 & 65535u;
            uint _3908 = ((_3856 & 8388607u) | 8388608u) >> 16u;
            uint _3909 = _3859 & 65535u;
            uint _3910 = ((_3859 & 8388607u) | 8388608u) >> 16u;
            uint _760 = _3907 * _3909;
            uint _763 = (_3907 * _3910) + (_3908 * _3909);
            uint _764 = _760 + (_763 << 16u);
            uint _3914;
            if (_764 < _760)
            {
                _3914 = 1u;
            }
            else
            {
                _3914 = 0u;
            }
            uint _767 = ((_3908 * _3910) + (_763 >> 16u)) + _3914;
            uint _3915 = as_type<uint>(_759);
            uint _769 = (((_3915 & 2147483647u) >> 23u) - (_3857 >> 23u)) - (_3860 >> 23u);
            uint _770 = _769 + 150u;
            bool _3922;
            if (_770 >= 23u)
            {
                _3922 = _770 > 24u;
            }
            else
            {
                _3922 = true;
            }
            if (_3922)
            {
                intervalFailed = true;
                float param_var_x = _759;
                float _3923 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _3923;
                float param_var_x_1 = _759;
                float _3924 = interval_down(param_var_x_1, intervalFailed);
                return _3924;
            }
            uint _3926 = (_3915 & 8388607u) | 8388608u;
            uint _3928 = _3926 << (_770 & 31u);
            uint _3930 = _3926 >> ((4294967178u - _769) & 31u);
            bool _3935;
            if (_767 <= _3930)
            {
                bool _3934;
                if (_767 == _3930)
                {
                    _3934 = _764 > _3928;
                }
                else
                {
                    _3934 = false;
                }
                _3935 = _3934;
            }
            else
            {
                _3935 = true;
            }
            bool _3940;
            if (_767 >= _3930)
            {
                bool _3939;
                if (_767 == _3930)
                {
                    _3939 = _764 < _3928;
                }
                else
                {
                    _3939 = false;
                }
                _3940 = _3939;
            }
            else
            {
                _3940 = true;
            }
            bool _3947 = ((as_type<uint>(a) ^ as_type<uint>(b)) & 2147483648u) != 0u;
            bool _3948;
            if (_3947)
            {
                _3948 = _3940;
            }
            else
            {
                _3948 = _3935;
            }
            float _3950;
            if (_3948)
            {
                float param_var_x_2 = _759;
                float _3949 = interval_up(param_var_x_2, intervalFailed);
                _3950 = _3949;
            }
            else
            {
                _3950 = _759;
            }
            optical_product_upper = _3950;
            bool _3951;
            if (_3947)
            {
                _3951 = _3935;
            }
            else
            {
                _3951 = _3940;
            }
            float _3953;
            if (_3951)
            {
                float param_var_x_3 = _759;
                float _3952 = interval_down(param_var_x_3, intervalFailed);
                _3953 = _3952;
            }
            else
            {
                _3953 = _759;
            }
            return _3953;
        }
    }
    float _772 = spvFMul(a, b);
    float param_var_x_4 = _772;
    float _3956 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _3956;
    float param_var_x_5 = _772;
    float _3957 = interval_down(param_var_x_5, intervalFailed);
    return _3957;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _3519;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3519 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3519 = false;
    }
    bool _3523;
    if (_3519)
    {
        _3523 = a.lo <= a.hi;
    }
    else
    {
        _3523 = false;
    }
    bool _3542;
    if (_3523)
    {
        bool _3537;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3537 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3537 = false;
        }
        bool _3541;
        if (_3537)
        {
            _3541 = b.lo <= b.hi;
        }
        else
        {
            _3541 = false;
        }
        _3542 = _3541;
    }
    else
    {
        _3542 = false;
    }
    if (_3542)
    {
        Interval param_var_a = a;
        float param_var_x = 0.0;
        bool _3548;
        if (!interval_exact_point(param_var_a, param_var_x))
        {
            Interval param_var_a_1 = b;
            float param_var_x_1 = 0.0;
            _3548 = interval_exact_point(param_var_a_1, param_var_x_1);
        }
        else
        {
            _3548 = true;
        }
        if (_3548)
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
    bool _3592;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3592 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3592 = false;
    }
    bool _3596;
    if (_3592)
    {
        _3596 = a.lo <= a.hi;
    }
    else
    {
        _3596 = false;
    }
    bool _3615;
    if (_3596)
    {
        bool _3610;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3610 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3610 = false;
        }
        bool _3614;
        if (_3610)
        {
            _3614 = b.lo <= b.hi;
        }
        else
        {
            _3614 = false;
        }
        _3615 = _3614;
    }
    else
    {
        _3615 = false;
    }
    bool _3623;
    if (_3615)
    {
        _3623 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    else
    {
        _3623 = false;
    }
    bool _3631;
    if (_3615)
    {
        _3631 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    else
    {
        _3631 = false;
    }
    float param_var_a_6 = a.lo;
    float param_var_b = b.lo;
    float _3636 = optical_product_bounds(param_var_a_6, param_var_b, intervalFailed, optical_product_upper);
    float _3645;
    float _3646;
    if (!_3631)
    {
        float param_var_a_7 = a.lo;
        float param_var_b_1 = b.hi;
        float _3643 = optical_product_bounds(param_var_a_7, param_var_b_1, intervalFailed, optical_product_upper);
        _3645 = optical_product_upper;
        _3646 = _3643;
    }
    else
    {
        _3645 = optical_product_upper;
        _3646 = _3636;
    }
    float _3654;
    float _3655;
    if (!_3623)
    {
        float param_var_a_8 = a.hi;
        float param_var_b_2 = b.lo;
        float _3652 = optical_product_bounds(param_var_a_8, param_var_b_2, intervalFailed, optical_product_upper);
        _3654 = optical_product_upper;
        _3655 = _3652;
    }
    else
    {
        _3654 = optical_product_upper;
        _3655 = _3636;
    }
    float _3665;
    float _3666;
    if (!_3623)
    {
        float _3663;
        float _3664;
        if (_3631)
        {
            _3663 = _3654;
            _3664 = _3655;
        }
        else
        {
            float param_var_a_9 = a.hi;
            float param_var_b_3 = b.hi;
            float _3661 = optical_product_bounds(param_var_a_9, param_var_b_3, intervalFailed, optical_product_upper);
            _3663 = optical_product_upper;
            _3664 = _3661;
        }
        _3665 = _3663;
        _3666 = _3664;
    }
    else
    {
        _3665 = _3645;
        _3666 = _3646;
    }
    return Interval{ precise::min(precise::min(_3636, _3646), precise::min(_3655, _3666)), precise::max(precise::max(optical_product_upper, _3645), precise::max(_3654, _3665)) };
}

static inline __attribute__((always_inline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float _4072;
    _4072 = precise::sqrt(a);
    float _4073;
    for (uint _4074 = 0u; _4074 < 8u; _4072 = _4073, _4074++)
    {
        float _889 = spvFMul(_4072, _4072);
        float _890 = spvFMul(_4072, 4097.0);
        float _892 = spvFSub(_890, spvFSub(_890, _4072));
        float _893 = spvFSub(_4072, _892);
        float _894 = spvFMul(_4072, 4097.0);
        float _896 = spvFSub(_894, spvFSub(_894, _4072));
        float _897 = spvFSub(_4072, _896);
        float _911 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_892, _896), _889), spvFMul(_892, _897)), spvFMul(_893, _896)), spvFMul(_893, _897)), spvFMul(_4072, 0.0)), spvFMul(0.0, _4072)), spvFMul(0.0, 0.0));
        float _912 = spvFAdd(_889, _911);
        float _914 = spvFSub(_911, spvFSub(_912, _889));
        bool _4083;
        if (!(isnan(_912) || isinf(_912)))
        {
            _4083 = isnan(_914) || isinf(_914);
        }
        else
        {
            _4083 = true;
        }
        if (_4083)
        {
            intervalFailed = true;
            return _4072;
        }
        bool _4090;
        if ((isunordered(_912, a) || _912 >= a))
        {
            bool _4089;
            if (_912 == a)
            {
                _4089 = _914 < 0.0;
            }
            else
            {
                _4089 = false;
            }
            _4090 = _4089;
        }
        else
        {
            _4090 = true;
        }
        bool _4097;
        if ((isunordered(a, _912) || a >= _912))
        {
            bool _4096;
            if (a == _912)
            {
                _4096 = 0.0 < _914;
            }
            else
            {
                _4096 = false;
            }
            _4097 = _4096;
        }
        else
        {
            _4097 = true;
        }
        bool _4101;
        if (upper)
        {
            _4101 = !_4090;
        }
        else
        {
            _4101 = !_4097;
        }
        if (_4101)
        {
            return _4072;
        }
        if (upper)
        {
            float param_var_x = _4072;
            float _4103 = interval_up(param_var_x, intervalFailed);
            _4073 = _4103;
        }
        else
        {
            float param_var_x_1 = _4072;
            float _4104 = interval_down(param_var_x_1, intervalFailed);
            _4073 = precise::max(0.0, _4104);
        }
    }
    intervalFailed = true;
    return _4072;
}

static inline __attribute__((always_inline))
Interval isqrt(thread const Interval& a, thread bool& intervalFailed)
{
    bool _4057;
    if ((isunordered(a.hi, 0.0) || a.hi >= 0.0))
    {
        _4057 = a.hi > 1000000015047466219876688855040.0;
    }
    else
    {
        _4057 = true;
    }
    if (_4057)
    {
        intervalFailed = true;
        return Interval{ 0.0, 1000000015047466219876688855040.0 };
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _4061 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _4061;
    float _4062 = interval_down(param_var_x, intervalFailed);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _4067 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _4067;
    float _4068 = interval_up(param_var_x_1, intervalFailed);
    return Interval{ precise::max(0.0, _4062), _4068 };
}

static inline __attribute__((always_inline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float _4004;
    _4004 = a / b;
    float _4005;
    for (uint _4006 = 0u; _4006 < 8u; _4004 = _4005, _4006++)
    {
        bool _4014;
        if (!(isnan(_4004) || isinf(_4004)))
        {
            _4014 = abs(_4004) > 1000000015047466219876688855040.0;
        }
        else
        {
            _4014 = true;
        }
        if (_4014)
        {
            intervalFailed = true;
            return _4004;
        }
        float _863 = spvFMul(_4004, b);
        float _864 = spvFMul(_4004, 4097.0);
        float _866 = spvFSub(_864, spvFSub(_864, _4004));
        float _867 = spvFSub(_4004, _866);
        float _868 = spvFMul(b, 4097.0);
        float _870 = spvFSub(_868, spvFSub(_868, b));
        float _871 = spvFSub(b, _870);
        float _885 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_866, _870), _863), spvFMul(_866, _871)), spvFMul(_867, _870)), spvFMul(_867, _871)), spvFMul(_4004, 0.0)), spvFMul(0.0, b)), spvFMul(0.0, 0.0));
        float _886 = spvFAdd(_863, _885);
        float _888 = spvFSub(_885, spvFSub(_886, _863));
        bool _4023;
        if (!(isnan(_886) || isinf(_886)))
        {
            _4023 = isnan(_888) || isinf(_888);
        }
        else
        {
            _4023 = true;
        }
        if (_4023)
        {
            intervalFailed = true;
            return _4004;
        }
        bool _4030;
        if ((isunordered(_886, a) || _886 >= a))
        {
            bool _4029;
            if (_886 == a)
            {
                _4029 = _888 < 0.0;
            }
            else
            {
                _4029 = false;
            }
            _4030 = _4029;
        }
        else
        {
            _4030 = true;
        }
        bool _4037;
        if ((isunordered(a, _886) || a >= _886))
        {
            bool _4036;
            if (a == _886)
            {
                _4036 = 0.0 < _888;
            }
            else
            {
                _4036 = false;
            }
            _4037 = _4036;
        }
        else
        {
            _4037 = true;
        }
        if (b < 0.0)
        {
            bool _4043;
            if (upper)
            {
                _4043 = !_4037;
            }
            else
            {
                _4043 = !_4030;
            }
            if (_4043)
            {
                return _4004;
            }
        }
        else
        {
            bool _4047;
            if (upper)
            {
                _4047 = !_4030;
            }
            else
            {
                _4047 = !_4037;
            }
            if (_4047)
            {
                return _4004;
            }
        }
        if (upper)
        {
            float param_var_x = _4004;
            float _4049 = interval_up(param_var_x, intervalFailed);
            _4005 = _4049;
        }
        else
        {
            float param_var_x_1 = _4004;
            float _4050 = interval_down(param_var_x_1, intervalFailed);
            _4005 = _4050;
        }
    }
    intervalFailed = true;
    return _4004;
}

static inline __attribute__((always_inline))
Interval idiv(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _3335;
    if (b.lo <= 0.0)
    {
        _3335 = b.hi >= 0.0;
    }
    else
    {
        _3335 = false;
    }
    bool _3345;
    if (!_3335)
    {
        _3345 = precise::max(abs(b.lo), abs(b.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _3345 = true;
    }
    bool _3355;
    if (!_3345)
    {
        _3355 = precise::max(abs(a.lo), abs(a.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _3355 = true;
    }
    if (_3355)
    {
        intervalFailed = true;
        return Interval{ -1000000015047466219876688855040.0, 1000000015047466219876688855040.0 };
    }
    bool _3369;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3369 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3369 = false;
    }
    bool _3373;
    if (_3369)
    {
        _3373 = a.lo <= a.hi;
    }
    else
    {
        _3373 = false;
    }
    bool _3392;
    if (_3373)
    {
        bool _3387;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3387 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3387 = false;
        }
        bool _3391;
        if (_3387)
        {
            _3391 = b.lo <= b.hi;
        }
        else
        {
            _3391 = false;
        }
        _3392 = _3391;
    }
    else
    {
        _3392 = false;
    }
    if (_3392)
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
    bool _3428;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3428 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3428 = false;
    }
    bool _3432;
    if (_3428)
    {
        _3432 = a.lo <= a.hi;
    }
    else
    {
        _3432 = false;
    }
    bool _3450;
    if (_3432)
    {
        bool _3445;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3445 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3445 = false;
        }
        bool _3449;
        if (_3445)
        {
            _3449 = b.lo <= b.hi;
        }
        else
        {
            _3449 = false;
        }
        _3450 = _3449;
    }
    else
    {
        _3450 = false;
    }
    bool _3456;
    if (_3450)
    {
        _3456 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    else
    {
        _3456 = false;
    }
    bool _3462;
    if (_3450)
    {
        _3462 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    else
    {
        _3462 = false;
    }
    float _3304 = a.lo;
    float _3305 = b.lo;
    bool _3306 = false;
    float _3463 = quotient_bound(_3304, _3305, _3306, intervalFailed);
    float _3307 = a.lo;
    float _3308 = b.lo;
    bool _3309 = true;
    float _3464 = quotient_bound(_3307, _3308, _3309, intervalFailed);
    float _3470;
    float _3471;
    if (!_3462)
    {
        float _3310 = a.lo;
        float _3311 = b.hi;
        bool _3312 = false;
        float _3468 = quotient_bound(_3310, _3311, _3312, intervalFailed);
        float _3313 = a.lo;
        float _3314 = b.hi;
        bool _3315 = true;
        float _3469 = quotient_bound(_3313, _3314, _3315, intervalFailed);
        _3470 = _3469;
        _3471 = _3468;
    }
    else
    {
        _3470 = _3464;
        _3471 = _3463;
    }
    float _3477;
    float _3478;
    if (!_3456)
    {
        float _3316 = a.hi;
        float _3317 = b.lo;
        bool _3318 = false;
        float _3475 = quotient_bound(_3316, _3317, _3318, intervalFailed);
        float _3319 = a.hi;
        float _3320 = b.lo;
        bool _3321 = true;
        float _3476 = quotient_bound(_3319, _3320, _3321, intervalFailed);
        _3477 = _3476;
        _3478 = _3475;
    }
    else
    {
        _3477 = _3464;
        _3478 = _3463;
    }
    float _3489;
    float _3490;
    if (!_3456)
    {
        float _3487;
        float _3488;
        if (_3462)
        {
            _3487 = _3477;
            _3488 = _3478;
        }
        else
        {
            float _3322 = a.hi;
            float _3323 = b.hi;
            bool _3324 = false;
            float _3485 = quotient_bound(_3322, _3323, _3324, intervalFailed);
            float _3325 = a.hi;
            float _3326 = b.hi;
            bool _3327 = true;
            float _3486 = quotient_bound(_3325, _3326, _3327, intervalFailed);
            _3487 = _3486;
            _3488 = _3485;
        }
        _3489 = _3487;
        _3490 = _3488;
    }
    else
    {
        _3489 = _3470;
        _3490 = _3471;
    }
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _3464), _3470), _3477), _3489);
    float param_var_x_3 = precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _3463), _3471), _3478), _3490);
    float _3499 = interval_down(param_var_x_3, intervalFailed);
    float param_var_x_4 = interval_divide_upper;
    float _3501 = interval_up(param_var_x_4, intervalFailed);
    return Interval{ _3499, _3501 };
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, device type_ByteAddressBuffer& frames, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        Interval param_var_a = Interval{ feature.x, feature.x };
        Interval param_var_b = Interval{ feature.y, feature.y };
        Interval _2875 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval _2864 = Interval{ 1.0, 1.0 };
        Interval _2865 = Interval{ as_type<float>(as_type<uint>(_2875.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2875.lo) ^ 2147483648u) };
        Interval _2885 = iadd(_2864, _2865, intervalFailed);
        uint _2887 = (at + 432u) >> 2u;
        uint _2889 = frames._m0[_2887];
        uint _2891 = frames._m0[_2887 + 1u];
        uint _2893 = frames._m0[_2887 + 2u];
        uint _2895 = frames._m0[_2887 + 3u];
        float4 _2897 = as_type<float4>(uint4(_2889, _2891, _2893, _2895));
        float _2898 = _2897.x;
        float _2899 = _2897.y;
        float _2900 = _2897.z;
        Interval _2858 = Interval{ _2898, _2898 };
        Interval _2859 = _2885;
        Interval _2902 = imul(_2858, _2859, intervalFailed, optical_product_upper);
        Interval _2860 = Interval{ _2899, _2899 };
        Interval _2861 = _2885;
        Interval _2904 = imul(_2860, _2861, intervalFailed, optical_product_upper);
        Interval _2862 = Interval{ _2900, _2900 };
        Interval _2863 = _2885;
        Interval _2906 = imul(_2862, _2863, intervalFailed, optical_product_upper);
        uint _2908 = (at + 448u) >> 2u;
        uint _2910 = frames._m0[_2908];
        uint _2912 = frames._m0[_2908 + 1u];
        uint _2914 = frames._m0[_2908 + 2u];
        uint _2916 = frames._m0[_2908 + 3u];
        float4 _2918 = as_type<float4>(uint4(_2910, _2912, _2914, _2916));
        float _2919 = _2918.x;
        float _2920 = _2918.y;
        float _2921 = _2918.z;
        Interval _2852 = Interval{ _2919, _2919 };
        Interval _2853 = Interval{ feature.x, feature.x };
        Interval _2924 = imul(_2852, _2853, intervalFailed, optical_product_upper);
        Interval _2854 = Interval{ _2920, _2920 };
        Interval _2855 = Interval{ feature.x, feature.x };
        Interval _2927 = imul(_2854, _2855, intervalFailed, optical_product_upper);
        Interval _2856 = Interval{ _2921, _2921 };
        Interval _2857 = Interval{ feature.x, feature.x };
        Interval _2930 = imul(_2856, _2857, intervalFailed, optical_product_upper);
        Interval _2846 = _2902;
        Interval _2847 = _2924;
        Interval _2931 = iadd(_2846, _2847, intervalFailed);
        Interval _2848 = _2904;
        Interval _2849 = _2927;
        Interval _2932 = iadd(_2848, _2849, intervalFailed);
        Interval _2850 = _2906;
        Interval _2851 = _2930;
        Interval _2933 = iadd(_2850, _2851, intervalFailed);
        uint _2935 = (at + 464u) >> 2u;
        uint _2937 = frames._m0[_2935];
        uint _2939 = frames._m0[_2935 + 1u];
        uint _2941 = frames._m0[_2935 + 2u];
        uint _2943 = frames._m0[_2935 + 3u];
        float4 _2945 = as_type<float4>(uint4(_2937, _2939, _2941, _2943));
        float _2946 = _2945.x;
        float _2947 = _2945.y;
        float _2948 = _2945.z;
        Interval _2840 = Interval{ _2946, _2946 };
        Interval _2841 = Interval{ feature.y, feature.y };
        Interval _2951 = imul(_2840, _2841, intervalFailed, optical_product_upper);
        Interval _2842 = Interval{ _2947, _2947 };
        Interval _2843 = Interval{ feature.y, feature.y };
        Interval _2954 = imul(_2842, _2843, intervalFailed, optical_product_upper);
        Interval _2844 = Interval{ _2948, _2948 };
        Interval _2845 = Interval{ feature.y, feature.y };
        Interval _2957 = imul(_2844, _2845, intervalFailed, optical_product_upper);
        Interval _2834 = _2931;
        Interval _2835 = _2951;
        Interval _2958 = iadd(_2834, _2835, intervalFailed);
        Interval _2836 = _2932;
        Interval _2837 = _2954;
        Interval _2959 = iadd(_2836, _2837, intervalFailed);
        Interval _2838 = _2933;
        Interval _2839 = _2957;
        Interval _2960 = iadd(_2838, _2839, intervalFailed);
        return Interval3{ _2958, _2959, _2960 };
    }
    Interval _2824 = Interval{ TargetSettings.targetCurrentCube[0].x, TargetSettings.targetCurrentCube[0].x };
    Interval _2825 = Interval{ feature.x, feature.x };
    Interval _2974 = imul(_2824, _2825, intervalFailed, optical_product_upper);
    Interval _2826 = _2974;
    Interval _2827 = Interval{ TargetSettings.targetCurrentCube[0].y, TargetSettings.targetCurrentCube[0].y };
    Interval _2828 = Interval{ feature.y, feature.y };
    Interval _2977 = imul(_2827, _2828, intervalFailed, optical_product_upper);
    Interval _2829 = _2977;
    Interval _2978 = iadd(_2826, _2829, intervalFailed);
    Interval _2830 = _2978;
    Interval _2831 = Interval{ TargetSettings.targetCurrentCube[0].z, TargetSettings.targetCurrentCube[0].z };
    Interval _2832 = Interval{ feature.z, feature.z };
    Interval _2981 = imul(_2831, _2832, intervalFailed, optical_product_upper);
    Interval _2833 = _2981;
    Interval _2982 = iadd(_2830, _2833, intervalFailed);
    Interval _2814 = Interval{ TargetSettings.targetCurrentCube[1].x, TargetSettings.targetCurrentCube[1].x };
    Interval _2815 = Interval{ feature.x, feature.x };
    Interval _2991 = imul(_2814, _2815, intervalFailed, optical_product_upper);
    Interval _2816 = _2991;
    Interval _2817 = Interval{ TargetSettings.targetCurrentCube[1].y, TargetSettings.targetCurrentCube[1].y };
    Interval _2818 = Interval{ feature.y, feature.y };
    Interval _2994 = imul(_2817, _2818, intervalFailed, optical_product_upper);
    Interval _2819 = _2994;
    Interval _2995 = iadd(_2816, _2819, intervalFailed);
    Interval _2820 = _2995;
    Interval _2821 = Interval{ TargetSettings.targetCurrentCube[1].z, TargetSettings.targetCurrentCube[1].z };
    Interval _2822 = Interval{ feature.z, feature.z };
    Interval _2998 = imul(_2821, _2822, intervalFailed, optical_product_upper);
    Interval _2823 = _2998;
    Interval _2999 = iadd(_2820, _2823, intervalFailed);
    Interval _2804 = Interval{ TargetSettings.targetCurrentCube[2].x, TargetSettings.targetCurrentCube[2].x };
    Interval _2805 = Interval{ feature.x, feature.x };
    Interval _3008 = imul(_2804, _2805, intervalFailed, optical_product_upper);
    Interval _2806 = _3008;
    Interval _2807 = Interval{ TargetSettings.targetCurrentCube[2].y, TargetSettings.targetCurrentCube[2].y };
    Interval _2808 = Interval{ feature.y, feature.y };
    Interval _3011 = imul(_2807, _2808, intervalFailed, optical_product_upper);
    Interval _2809 = _3011;
    Interval _3012 = iadd(_2806, _2809, intervalFailed);
    Interval _2810 = _3012;
    Interval _2811 = Interval{ TargetSettings.targetCurrentCube[2].z, TargetSettings.targetCurrentCube[2].z };
    Interval _2812 = Interval{ feature.z, feature.z };
    Interval _3015 = imul(_2811, _2812, intervalFailed, optical_product_upper);
    Interval _2813 = _3015;
    Interval _3016 = iadd(_2810, _2813, intervalFailed);
    Interval _2794 = Interval{ TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[0].x };
    Interval _2795 = _2982;
    Interval _3030 = imul(_2794, _2795, intervalFailed, optical_product_upper);
    Interval _2796 = _3030;
    Interval _2797 = Interval{ TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[1].x };
    Interval _2798 = _2999;
    Interval _3032 = imul(_2797, _2798, intervalFailed, optical_product_upper);
    Interval _2799 = _3032;
    Interval _3033 = iadd(_2796, _2799, intervalFailed);
    Interval _2800 = _3033;
    Interval _2801 = Interval{ TargetSettings.targetPreviousCube[2].x, TargetSettings.targetPreviousCube[2].x };
    Interval _2802 = _3016;
    Interval _3035 = imul(_2801, _2802, intervalFailed, optical_product_upper);
    Interval _2803 = _3035;
    Interval _3036 = iadd(_2800, _2803, intervalFailed);
    Interval _2784 = Interval{ TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[0].y };
    Interval _2785 = _2982;
    Interval _3050 = imul(_2784, _2785, intervalFailed, optical_product_upper);
    Interval _2786 = _3050;
    Interval _2787 = Interval{ TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[1].y };
    Interval _2788 = _2999;
    Interval _3052 = imul(_2787, _2788, intervalFailed, optical_product_upper);
    Interval _2789 = _3052;
    Interval _3053 = iadd(_2786, _2789, intervalFailed);
    Interval _2790 = _3053;
    Interval _2791 = Interval{ TargetSettings.targetPreviousCube[2].y, TargetSettings.targetPreviousCube[2].y };
    Interval _2792 = _3016;
    Interval _3055 = imul(_2791, _2792, intervalFailed, optical_product_upper);
    Interval _2793 = _3055;
    Interval _3056 = iadd(_2790, _2793, intervalFailed);
    Interval _2774 = Interval{ TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[0].z };
    Interval _2775 = _2982;
    Interval _3070 = imul(_2774, _2775, intervalFailed, optical_product_upper);
    Interval _2776 = _3070;
    Interval _2777 = Interval{ TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[1].z };
    Interval _2778 = _2999;
    Interval _3072 = imul(_2777, _2778, intervalFailed, optical_product_upper);
    Interval _2779 = _3072;
    Interval _3073 = iadd(_2776, _2779, intervalFailed);
    Interval _2780 = _3073;
    Interval _2781 = Interval{ TargetSettings.targetPreviousCube[2].z, TargetSettings.targetPreviousCube[2].z };
    Interval _2782 = _3016;
    Interval _3075 = imul(_2781, _2782, intervalFailed, optical_product_upper);
    Interval _2783 = _3075;
    Interval _3076 = iadd(_2780, _2783, intervalFailed);
    Interval _2772 = Interval{ 1.0, 1.0 };
    bool _3083;
    if (_3036.lo <= 0.0)
    {
        _3083 = _3036.hi >= 0.0;
    }
    else
    {
        _3083 = false;
    }
    float _3090;
    if (_3083)
    {
        _3090 = 0.0;
    }
    else
    {
        _3090 = precise::min(abs(_3036.lo), abs(_3036.hi));
    }
    float _3093 = precise::max(abs(_3036.lo), abs(_3036.hi));
    float _2765 = spvFMul(_3090, _3090);
    float _3094 = interval_down(_2765, intervalFailed);
    float _2766 = spvFMul(_3093, _3093);
    float _3096 = interval_up(_2766, intervalFailed);
    Interval _2767 = Interval{ precise::max(0.0, _3094), _3096 };
    bool _3104;
    if (_3056.lo <= 0.0)
    {
        _3104 = _3056.hi >= 0.0;
    }
    else
    {
        _3104 = false;
    }
    float _3111;
    if (_3104)
    {
        _3111 = 0.0;
    }
    else
    {
        _3111 = precise::min(abs(_3056.lo), abs(_3056.hi));
    }
    float _3114 = precise::max(abs(_3056.lo), abs(_3056.hi));
    float _2763 = spvFMul(_3111, _3111);
    float _3115 = interval_down(_2763, intervalFailed);
    float _2764 = spvFMul(_3114, _3114);
    float _3117 = interval_up(_2764, intervalFailed);
    Interval _2768 = Interval{ precise::max(0.0, _3115), _3117 };
    Interval _3119 = iadd(_2767, _2768, intervalFailed);
    Interval _2769 = _3119;
    bool _3126;
    if (_3076.lo <= 0.0)
    {
        _3126 = _3076.hi >= 0.0;
    }
    else
    {
        _3126 = false;
    }
    float _3133;
    if (_3126)
    {
        _3133 = 0.0;
    }
    else
    {
        _3133 = precise::min(abs(_3076.lo), abs(_3076.hi));
    }
    float _3136 = precise::max(abs(_3076.lo), abs(_3076.hi));
    float _2761 = spvFMul(_3133, _3133);
    float _3137 = interval_down(_2761, intervalFailed);
    float _2762 = spvFMul(_3136, _3136);
    float _3139 = interval_up(_2762, intervalFailed);
    Interval _2770 = Interval{ precise::max(0.0, _3137), _3139 };
    Interval _3141 = iadd(_2769, _2770, intervalFailed);
    Interval _2771 = _3141;
    Interval _3142 = isqrt(_2771, intervalFailed);
    Interval _2773 = _3142;
    Interval _3143 = idiv(_2772, _2773, intervalFailed, interval_divide_upper);
    Interval _2755 = _3036;
    Interval _2756 = _3143;
    Interval _3144 = imul(_2755, _2756, intervalFailed, optical_product_upper);
    Interval _2757 = _3056;
    Interval _2758 = _3143;
    Interval _3145 = imul(_2757, _2758, intervalFailed, optical_product_upper);
    Interval _2759 = _3076;
    Interval _2760 = _3143;
    Interval _3146 = imul(_2759, _2760, intervalFailed, optical_product_upper);
    return Interval3{ _3144, _3145, _3146 };
}

static inline __attribute__((always_inline))
bool witness_feature(thread const float3& value, thread const uint& kind, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool3 _3741 = isnan(value);
    bool3 _3742 = isinf(value);
    if (!all(not(bool3(_3741.x || _3742.x, _3741.y || _3742.y, _3741.z || _3742.z))))
    {
        return false;
    }
    if (kind == 1u)
    {
        bool _3756;
        if (value.z == 0.0)
        {
            _3756 = all(value.xy >= float2(0.0));
        }
        else
        {
            _3756 = false;
        }
        bool _3762;
        if (_3756)
        {
            _3762 = spvFAdd(value.x, value.y) <= 1.0;
        }
        else
        {
            _3762 = false;
        }
        return _3762;
    }
    Interval _3730 = Interval{ value.x, value.x };
    Interval _3731 = Interval{ value.x, value.x };
    Interval _3773 = imul(_3730, _3731, intervalFailed, optical_product_upper);
    Interval _3732 = _3773;
    Interval _3733 = Interval{ value.y, value.y };
    Interval _3734 = Interval{ value.y, value.y };
    Interval _3776 = imul(_3733, _3734, intervalFailed, optical_product_upper);
    Interval _3735 = _3776;
    Interval _3777 = iadd(_3732, _3735, intervalFailed);
    Interval _3736 = _3777;
    Interval _3737 = Interval{ value.z, value.z };
    Interval _3738 = Interval{ value.z, value.z };
    Interval _3780 = imul(_3737, _3738, intervalFailed, optical_product_upper);
    Interval _3739 = _3780;
    Interval _3781 = iadd(_3736, _3739, intervalFailed);
    bool _3789;
    if (!intervalFailed)
    {
        bool _3788;
        if (kind != 2u)
        {
            _3788 = kind == 3u;
        }
        else
        {
            _3788 = true;
        }
        _3789 = _3788;
    }
    else
    {
        _3789 = false;
    }
    bool _3802;
    if (_3789)
    {
        Interval _3728 = _3781;
        Interval _3729 = Interval{ as_type<float>(3212836864u), as_type<float>(3212836864u) };
        Interval _3793 = iadd(_3728, _3729, intervalFailed);
        _3802 = precise::max(abs(_3793.lo), abs(_3793.hi)) < 9.9999997473787516355514526367188e-05;
    }
    else
    {
        _3802 = false;
    }
    return _3802;
}

static inline __attribute__((always_inline))
WitnessTap witness_tap(thread const int2& pixel, thread const WitnessQuery& q, constant type_Settings& Settings, device type_ByteAddressBuffer& source, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _3162;
    if (!any(pixel < int2(0)))
    {
        _3162 = any(pixel >= int2(int(Settings.width), int(Settings.height)));
    }
    else
    {
        _3162 = true;
    }
    if (_3162)
    {
        return WitnessTap{ 0.0, float3(0.0), short(false), short(false) };
    }
    uint _726 = (uint(pixel.y) * Settings.width) + uint(pixel.x);
    uint _727 = Settings.width * Settings.height;
    if (source._m0[((_727 * Settings.primaryPrefix) + (_726 * 4u)) >> 2u] != q.primary)
    {
        return WitnessTap{ 0.0, float3(0.0), short(false), short(false) };
    }
    uint _735 = (_727 * Settings.recordPrefix) + (((_726 * Settings.lobes) + q.lobe) * Settings.pathStride);
    uint _3191 = _735 >> 2u;
    bool _3212;
    if (!any(uint4(source._m0[_3191], source._m0[_3191 + 1u], source._m0[_3191 + 2u], source._m0[_3191 + 3u]) != q.mirrors))
    {
        _3212 = source._m0[(_735 + 16u) >> 2u] != q.control;
    }
    else
    {
        _3212 = true;
    }
    bool _3220;
    if (!_3212)
    {
        _3220 = source._m0[(_735 + 20u) >> 2u] != q.terminal;
    }
    else
    {
        _3220 = true;
    }
    if (_3220)
    {
        return WitnessTap{ 0.0, float3(0.0), short(false), short(false) };
    }
    uint _3221 = (_735 + 24u) >> 2u;
    uint _3223 = source._m0[_3221];
    uint _3225 = source._m0[_3221 + 1u];
    uint _3227 = source._m0[_3221 + 2u];
    float3 _3229 = as_type<float3>(uint3(_3223, _3225, _3227));
    uint _3230 = (_735 + 40u) >> 2u;
    uint _3232 = source._m0[_3230];
    uint _3234 = source._m0[_3230 + 1u];
    uint _3236 = source._m0[_3230 + 2u];
    float3 _3238 = as_type<float3>(uint3(_3232, _3234, _3236));
    float3 param_var_value = _3229;
    uint param_var_kind = q.control >> 8u;
    bool _3242 = witness_feature(param_var_value, param_var_kind, intervalFailed, optical_product_upper);
    bool _3248;
    if (_3242)
    {
        _3248 = (source._m0[(_735 + 36u) >> 2u] >> 24u) != 255u;
    }
    else
    {
        _3248 = true;
    }
    bool _3256;
    if (!_3248)
    {
        bool3 _3250 = isnan(_3238);
        bool3 _3251 = isinf(_3238);
        _3256 = !all(not(bool3(_3250.x || _3251.x, _3250.y || _3251.y, _3250.z || _3251.z)));
    }
    else
    {
        _3256 = true;
    }
    bool _3260;
    if (!_3256)
    {
        _3260 = any(_3238 < float3(0.0));
    }
    else
    {
        _3260 = true;
    }
    bool _3264;
    if (!_3260)
    {
        _3264 = any(_3238 > float3(1.0));
    }
    else
    {
        _3264 = true;
    }
    if (_3264)
    {
        return WitnessTap{ 0.0, _3229, short(false), short(false) };
    }
    if (Settings.pathStride == 64u)
    {
        uint _3269 = (_735 + 52u) >> 2u;
        float3 _3277 = as_type<float3>(uint3(source._m0[_3269], source._m0[_3269 + 1u], source._m0[_3269 + 2u]));
        bool3 _3278 = isnan(_3277);
        bool3 _3279 = isinf(_3277);
        bool _3285;
        if (all(not(bool3(_3278.x || _3279.x, _3278.y || _3279.y, _3278.z || _3279.z))))
        {
            _3285 = any(_3277 < float3(0.0));
        }
        else
        {
            _3285 = true;
        }
        bool _3289;
        if (!_3285)
        {
            _3289 = any(_3277 > float3(999999995904.0));
        }
        else
        {
            _3289 = true;
        }
        if (_3289)
        {
            return WitnessTap{ 0.0, _3229, short(false), short(false) };
        }
    }
    float _3296 = as_type<float>(source._m0[((_727 * (Settings.primaryPrefix + 4u)) + (_726 * 4u)) >> 2u]);
    bool _3302;
    if (!(isnan(_3296) || isinf(_3296)))
    {
        _3302 = _3296 > 0.0;
    }
    else
    {
        _3302 = false;
    }
    return WitnessTap{ _3296, _3229, short(true), short(_3302) };
}

static inline __attribute__((always_inline))
Interval witness_select(thread const bool& pick, thread const Interval& a, thread const Interval& b)
{
    if (pick)
    {
        return a;
    }
    return b;
}

static inline __attribute__((always_inline))
void src_feature_witness_main(thread const uint3& id, constant type_Settings& Settings, device type_ByteAddressBuffer& frames, device type_ByteAddressBuffer& queries, device type_ByteAddressBuffer& source, device type_RWByteAddressBuffer& results, constant type_WitnessSettings& WitnessSettings, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, constant type_TargetSettings& TargetSettings)
{
    bool _1254;
    if (id.x < Settings.queryCount)
    {
        _1254 = id.x >= 64u;
    }
    else
    {
        _1254 = true;
    }
    if (_1254)
    {
        return;
    }
    uint _640 = id.x * 24576u;
    uint _641 = _640 + 192u;
    uint _642 = id.x * 512u;
    for (uint _1255 = 0u; _1255 < 4u; _1255++)
    {
        uint _1257 = (_641 + (_1255 * 16u)) >> 2u;
        results._m0[_1257] = 0u;
        results._m0[_1257 + 1u] = 0u;
        results._m0[_1257 + 2u] = 0u;
        results._m0[_1257 + 3u] = 0u;
    }
    intervalFailed = false;
    bool _1268;
    if (Settings.queryCount <= 64u)
    {
        _1268 = WitnessSettings.witnessStride != 24576u;
    }
    else
    {
        _1268 = true;
    }
    bool _1273;
    if (!_1268)
    {
        _1273 = WitnessSettings.witnessOffset != 192u;
    }
    else
    {
        _1273 = true;
    }
    bool _1278;
    if (!_1273)
    {
        _1278 = WitnessSettings.witnessBytes != 64u;
    }
    else
    {
        _1278 = true;
    }
    bool _1283;
    if (!_1278)
    {
        _1283 = WitnessSettings.witnessReserved != 0u;
    }
    else
    {
        _1283 = true;
    }
    bool _1292;
    if (!_1283)
    {
        bool _1291;
        if (Settings.pathStride != 52u)
        {
            _1291 = Settings.pathStride != 64u;
        }
        else
        {
            _1291 = false;
        }
        _1292 = _1291;
    }
    else
    {
        _1292 = true;
    }
    bool _1297;
    if (!_1292)
    {
        _1297 = Settings.width == 0u;
    }
    else
    {
        _1297 = true;
    }
    bool _1302;
    if (!_1297)
    {
        _1302 = Settings.height == 0u;
    }
    else
    {
        _1302 = true;
    }
    bool _1307;
    if (!_1302)
    {
        _1307 = Settings.width > 16384u;
    }
    else
    {
        _1307 = true;
    }
    bool _1312;
    if (!_1307)
    {
        _1312 = Settings.height > 16384u;
    }
    else
    {
        _1312 = true;
    }
    bool _1321;
    if (!_1312)
    {
        bool _1320;
        if (Settings.lobes != 1u)
        {
            _1320 = Settings.lobes != 8u;
        }
        else
        {
            _1320 = false;
        }
        _1321 = _1320;
    }
    else
    {
        _1321 = true;
    }
    bool _1335;
    if (!_1321)
    {
        uint _1323 = (_642 + 496u) >> 2u;
        _1335 = any(uint4(frames._m0[_1323], frames._m0[_1323 + 1u], frames._m0[_1323 + 2u], frames._m0[_1323 + 3u]) != uint4(1u, 0u, 0u, 0u));
    }
    else
    {
        _1335 = true;
    }
    if (_1335)
    {
        uint _1336 = _641 >> 2u;
        uint _1341;
        if (intervalFailed)
        {
            _1341 = 6u;
        }
        else
        {
            _1341 = 8u;
        }
        results._m0[_1336] = 0u;
        results._m0[_1336 + 1u] = 0u;
        results._m0[_1336 + 2u] = _1341;
        results._m0[_1336 + 3u] = 0u;
        return;
    }
    uint _1346 = _640 >> 2u;
    if (any(uint4(results._m0[_1346], results._m0[_1346 + 1u], results._m0[_1346 + 2u], results._m0[_1346 + 3u]) != uint4(1u, 1u, results._m0[(_640 + 8u) >> 2u], 0u)))
    {
        uint _1362 = _641 >> 2u;
        uint _1367;
        if (intervalFailed)
        {
            _1367 = 6u;
        }
        else
        {
            _1367 = 1u;
        }
        results._m0[_1362] = 0u;
        results._m0[_1362 + 1u] = 0u;
        results._m0[_1362 + 2u] = _1367;
        results._m0[_1362 + 3u] = 0u;
        return;
    }
    uint _1372 = (_640 + 144u) >> 2u;
    uint _1374 = results._m0[_1372];
    uint _658 = _1372 + 1u;
    uint _1376 = results._m0[_658];
    uint _659 = _1372 + 2u;
    uint _1378 = results._m0[_659];
    uint _660 = _1372 + 3u;
    uint _1380 = results._m0[_660];
    float4 _1382 = as_type<float4>(uint4(_1374, _1376, _1378, _1380));
    uint _1383 = (_640 + 208u) >> 2u;
    results._m0[_1383] = _1374;
    results._m0[_1383 + 1u] = _1376;
    results._m0[_1383 + 2u] = _1378;
    results._m0[_1383 + 3u] = _1380;
    bool4 _1388 = isnan(_1382);
    bool4 _1389 = isinf(_1382);
    bool _1397;
    if (all(not(bool4(_1388.x || _1389.x, _1388.y || _1389.y, _1388.z || _1389.z, _1388.w || _1389.w))))
    {
        _1397 = any(_1382.xy > _1382.zw);
    }
    else
    {
        _1397 = true;
    }
    bool _1402;
    if (!_1397)
    {
        _1402 = any(_1382.xy < float2(0.5));
    }
    else
    {
        _1402 = true;
    }
    bool _1414;
    if (!_1402)
    {
        _1414 = any(_1382.zw > spvFSub(float2(float(Settings.width), float(Settings.height)), float2(0.5)));
    }
    else
    {
        _1414 = true;
    }
    if (_1414)
    {
        uint _1415 = _641 >> 2u;
        uint _1420;
        if (intervalFailed)
        {
            _1420 = 6u;
        }
        else
        {
            _1420 = 2u;
        }
        results._m0[_1415] = 0u;
        results._m0[_1415 + 1u] = 1u;
        results._m0[_1415 + 2u] = _1420;
        results._m0[_1415 + 3u] = 0u;
        return;
    }
    uint _1425 = (id.x * 48u) >> 2u;
    uint _1427 = queries._m0[_1425];
    uint _1428 = ((id.x * 48u) + 4u) >> 2u;
    uint _1430 = queries._m0[_1428];
    uint _1432 = queries._m0[_1428 + 1u];
    uint _1434 = queries._m0[_1428 + 2u];
    uint _1436 = queries._m0[_1428 + 3u];
    uint4 _1437 = uint4(_1430, _1432, _1434, _1436);
    uint _1438 = ((id.x * 48u) + 20u) >> 2u;
    uint _1440 = queries._m0[_1438];
    uint _1441 = ((id.x * 48u) + 24u) >> 2u;
    uint _1443 = queries._m0[_1441];
    uint _1444 = ((id.x * 48u) + 44u) >> 2u;
    uint _1446 = queries._m0[_1444];
    bool _1451;
    if (_1427 != 4294967295u)
    {
        _1451 = _1446 >= Settings.lobes;
    }
    else
    {
        _1451 = true;
    }
    bool _1454;
    if (!_1451)
    {
        _1454 = _1446 > 7u;
    }
    else
    {
        _1454 = true;
    }
    if (_1454)
    {
        uint _1455 = _641 >> 2u;
        uint _1460;
        if (intervalFailed)
        {
            _1460 = 6u;
        }
        else
        {
            _1460 = 8u;
        }
        results._m0[_1455] = 0u;
        results._m0[_1455 + 1u] = 1u;
        results._m0[_1455 + 2u] = _1460;
        results._m0[_1455 + 3u] = 0u;
        return;
    }
    uint _1465 = (_642 + 416u) >> 2u;
    float4 _1475 = as_type<float4>(uint4(frames._m0[_1465], frames._m0[_1465 + 1u], frames._m0[_1465 + 2u], frames._m0[_1465 + 3u]));
    float _1491;
    float _1492;
    float _1493;
    float _1494;
    float _1495;
    float _1496;
    if (_1475.w != 0.0)
    {
        float _1478 = _1475.x;
        float _1479 = _1475.y;
        float _1480 = _1475.z;
        _1491 = _1478;
        _1492 = _1478;
        _1493 = _1479;
        _1494 = _1479;
        _1495 = _1480;
        _1496 = _1480;
    }
    else
    {
        uint param_var_at = _642;
        float4 param_var_feature = _1475;
        Interval3 _1481 = native_target(param_var_at, param_var_feature, frames, intervalFailed, optical_product_upper, interval_divide_upper, TargetSettings);
        _1491 = _1481.x.lo;
        _1492 = _1481.x.hi;
        _1493 = _1481.y.lo;
        _1494 = _1481.y.hi;
        _1495 = _1481.z.lo;
        _1496 = _1481.z.hi;
    }
    uint _1497 = (_642 + 48u) >> 2u;
    uint _1499 = frames._m0[_1497];
    uint _1501 = frames._m0[_1497 + 1u];
    uint _1503 = frames._m0[_1497 + 2u];
    uint _1505 = frames._m0[_1497 + 3u];
    float4 _1507 = as_type<float4>(uint4(_1499, _1501, _1503, _1505));
    float _1699;
    float _1700;
    float _1701;
    float _1702;
    float _1703;
    float _1704;
    float _1705;
    float _1706;
    float _1707;
    float _1708;
    float _1709;
    float _1710;
    if (frames._m0[(_642 + 488u) >> 2u] != 0u)
    {
        uint _1515 = (_642 + 288u) >> 2u;
        float4 _1525 = as_type<float4>(uint4(frames._m0[_1515], frames._m0[_1515 + 1u], frames._m0[_1515 + 2u], frames._m0[_1515 + 3u]));
        float _1526 = _1525.x;
        float _1527 = _1525.y;
        float _1528 = _1525.z;
        uint _1529 = (_642 + 304u) >> 2u;
        float4 _1539 = as_type<float4>(uint4(frames._m0[_1529], frames._m0[_1529 + 1u], frames._m0[_1529 + 2u], frames._m0[_1529 + 3u]));
        float _1540 = _1539.x;
        float _1541 = _1539.y;
        float _1542 = _1539.z;
        _1699 = _1540;
        _1700 = _1540;
        _1701 = _1541;
        _1702 = _1541;
        _1703 = _1542;
        _1704 = _1542;
        _1705 = _1526;
        _1706 = _1526;
        _1707 = _1527;
        _1708 = _1527;
        _1709 = _1528;
        _1710 = _1528;
    }
    else
    {
        uint _1543 = _642 >> 2u;
        uint _1545 = frames._m0[_1543];
        uint _1547 = frames._m0[_1543 + 1u];
        uint _1549 = frames._m0[_1543 + 2u];
        uint _1551 = frames._m0[_1543 + 3u];
        float4 _1553 = as_type<float4>(uint4(_1545, _1547, _1549, _1551));
        uint _1554 = (_642 + 16u) >> 2u;
        uint _1556 = frames._m0[_1554];
        uint _1558 = frames._m0[_1554 + 1u];
        uint _1560 = frames._m0[_1554 + 2u];
        uint _1562 = frames._m0[_1554 + 3u];
        float4 _1564 = as_type<float4>(uint4(_1556, _1558, _1560, _1562));
        uint _1565 = (_642 + 32u) >> 2u;
        uint _1567 = frames._m0[_1565];
        uint _1569 = frames._m0[_1565 + 1u];
        uint _1571 = frames._m0[_1565 + 2u];
        uint _1573 = frames._m0[_1565 + 3u];
        float4 _1575 = as_type<float4>(uint4(_1567, _1569, _1571, _1573));
        float _1576 = _1553.x;
        float _1577 = _1553.y;
        float _1578 = _1553.z;
        float _1693;
        float _1694;
        float _1695;
        float _1696;
        float _1697;
        float _1698;
        if (all(float3(_1553.w, _1564.w, _1575.w) == float3(2.0)))
        {
            float _1588 = _1564.x;
            float _1589 = _1564.y;
            float _1590 = _1564.z;
            _1693 = _1588;
            _1694 = _1588;
            _1695 = _1589;
            _1696 = _1589;
            _1697 = _1590;
            _1698 = _1590;
        }
        else
        {
            float _1591 = _1564.x;
            float _1592 = _1564.y;
            float _1593 = _1564.z;
            Interval _1236 = Interval{ _1591, _1591 };
            Interval _1237 = Interval{ as_type<float>(as_type<uint>(_1576) ^ 2147483648u), as_type<float>(as_type<uint>(_1576) ^ 2147483648u) };
            Interval _1614 = iadd(_1236, _1237, intervalFailed);
            Interval _1238 = Interval{ _1592, _1592 };
            Interval _1239 = Interval{ as_type<float>(as_type<uint>(_1577) ^ 2147483648u), as_type<float>(as_type<uint>(_1577) ^ 2147483648u) };
            Interval _1617 = iadd(_1238, _1239, intervalFailed);
            Interval _1240 = Interval{ _1593, _1593 };
            Interval _1241 = Interval{ as_type<float>(as_type<uint>(_1578) ^ 2147483648u), as_type<float>(as_type<uint>(_1578) ^ 2147483648u) };
            Interval _1620 = iadd(_1240, _1241, intervalFailed);
            float _1621 = _1575.x;
            float _1622 = _1575.y;
            float _1623 = _1575.z;
            Interval _1230 = Interval{ _1621, _1621 };
            Interval _1231 = Interval{ as_type<float>(as_type<uint>(_1576) ^ 2147483648u), as_type<float>(as_type<uint>(_1576) ^ 2147483648u) };
            Interval _1644 = iadd(_1230, _1231, intervalFailed);
            Interval _1232 = Interval{ _1622, _1622 };
            Interval _1233 = Interval{ as_type<float>(as_type<uint>(_1577) ^ 2147483648u), as_type<float>(as_type<uint>(_1577) ^ 2147483648u) };
            Interval _1647 = iadd(_1232, _1233, intervalFailed);
            Interval _1234 = Interval{ _1623, _1623 };
            Interval _1235 = Interval{ as_type<float>(as_type<uint>(_1578) ^ 2147483648u), as_type<float>(as_type<uint>(_1578) ^ 2147483648u) };
            Interval _1650 = iadd(_1234, _1235, intervalFailed);
            Interval _1218 = _1617;
            Interval _1219 = _1650;
            Interval _1651 = imul(_1218, _1219, intervalFailed, optical_product_upper);
            Interval _1220 = _1620;
            Interval _1221 = _1647;
            Interval _1652 = imul(_1220, _1221, intervalFailed, optical_product_upper);
            Interval _1216 = _1651;
            Interval _1217 = Interval{ as_type<float>(as_type<uint>(_1652.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_1652.lo) ^ 2147483648u) };
            Interval _1662 = iadd(_1216, _1217, intervalFailed);
            Interval _1222 = _1620;
            Interval _1223 = _1644;
            Interval _1663 = imul(_1222, _1223, intervalFailed, optical_product_upper);
            Interval _1224 = _1614;
            Interval _1225 = _1650;
            Interval _1664 = imul(_1224, _1225, intervalFailed, optical_product_upper);
            Interval _1214 = _1663;
            Interval _1215 = Interval{ as_type<float>(as_type<uint>(_1664.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_1664.lo) ^ 2147483648u) };
            Interval _1674 = iadd(_1214, _1215, intervalFailed);
            Interval _1226 = _1614;
            Interval _1227 = _1647;
            Interval _1675 = imul(_1226, _1227, intervalFailed, optical_product_upper);
            Interval _1228 = _1617;
            Interval _1229 = _1644;
            Interval _1676 = imul(_1228, _1229, intervalFailed, optical_product_upper);
            Interval _1212 = _1675;
            Interval _1213 = Interval{ as_type<float>(as_type<uint>(_1676.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_1676.lo) ^ 2147483648u) };
            Interval _1686 = iadd(_1212, _1213, intervalFailed);
            _1693 = _1662.lo;
            _1694 = _1662.hi;
            _1695 = _1674.lo;
            _1696 = _1674.hi;
            _1697 = _1686.lo;
            _1698 = _1686.hi;
        }
        _1699 = _1693;
        _1700 = _1694;
        _1701 = _1695;
        _1702 = _1696;
        _1703 = _1697;
        _1704 = _1698;
        _1705 = _1576;
        _1706 = _1576;
        _1707 = _1577;
        _1708 = _1577;
        _1709 = _1578;
        _1710 = _1578;
    }
    float _1713 = _1507.z;
    Interval _1210 = Interval{ _1382.x, _1382.z };
    Interval _1211 = Interval{ as_type<float>(as_type<uint>(_1713) ^ 2147483648u), as_type<float>(as_type<uint>(_1713) ^ 2147483648u) };
    Interval _1722 = iadd(_1210, _1211, intervalFailed);
    Interval _1242 = _1722;
    float _1723 = _1507.x;
    Interval _1243 = Interval{ _1723, _1723 };
    Interval _1725 = idiv(_1242, _1243, intervalFailed, interval_divide_upper);
    float _1728 = _1507.w;
    Interval _1208 = Interval{ _1382.y, _1382.w };
    Interval _1209 = Interval{ as_type<float>(as_type<uint>(_1728) ^ 2147483648u), as_type<float>(as_type<uint>(_1728) ^ 2147483648u) };
    Interval _1737 = iadd(_1208, _1209, intervalFailed);
    Interval _1244 = _1737;
    float _1738 = _1507.y;
    Interval _1245 = Interval{ _1738, _1738 };
    Interval _1740 = idiv(_1244, _1245, intervalFailed, interval_divide_upper);
    Interval _1198 = Interval{ _1705, _1706 };
    Interval _1199 = Interval{ _1699, _1700 };
    Interval _1743 = imul(_1198, _1199, intervalFailed, optical_product_upper);
    Interval _1200 = _1743;
    Interval _1201 = Interval{ _1707, _1708 };
    Interval _1202 = Interval{ _1701, _1702 };
    Interval _1746 = imul(_1201, _1202, intervalFailed, optical_product_upper);
    Interval _1203 = _1746;
    Interval _1747 = iadd(_1200, _1203, intervalFailed);
    Interval _1204 = _1747;
    Interval _1205 = Interval{ _1709, _1710 };
    Interval _1206 = Interval{ _1703, _1704 };
    Interval _1750 = imul(_1205, _1206, intervalFailed, optical_product_upper);
    Interval _1207 = _1750;
    Interval _1751 = iadd(_1204, _1207, intervalFailed);
    Interval _1246 = _1751;
    Interval _1188 = _1725;
    Interval _1189 = Interval{ _1699, _1700 };
    Interval _1753 = imul(_1188, _1189, intervalFailed, optical_product_upper);
    Interval _1190 = _1753;
    Interval _1191 = _1740;
    Interval _1192 = Interval{ _1701, _1702 };
    Interval _1755 = imul(_1191, _1192, intervalFailed, optical_product_upper);
    Interval _1193 = _1755;
    Interval _1756 = iadd(_1190, _1193, intervalFailed);
    Interval _1194 = _1756;
    Interval _1195 = Interval{ 1.0, 1.0 };
    Interval _1196 = Interval{ _1703, _1704 };
    Interval _1758 = imul(_1195, _1196, intervalFailed, optical_product_upper);
    Interval _1197 = _1758;
    Interval _1759 = iadd(_1194, _1197, intervalFailed);
    Interval _1247 = _1759;
    Interval _1760 = idiv(_1246, _1247, intervalFailed, interval_divide_upper);
    uint _1763 = (_642 + 64u) >> 2u;
    float4 _1773 = as_type<float4>(uint4(frames._m0[_1763], frames._m0[_1763 + 1u], frames._m0[_1763 + 2u], frames._m0[_1763 + 3u]));
    bool _1777;
    if (!intervalFailed)
    {
        _1777 = _1760.lo <= 0.0;
    }
    else
    {
        _1777 = true;
    }
    bool _1781;
    if (!_1777)
    {
        _1781 = _1760.lo < _1773.z;
    }
    else
    {
        _1781 = true;
    }
    bool _1785;
    if (!_1781)
    {
        _1785 = _1760.hi > _1773.w;
    }
    else
    {
        _1785 = true;
    }
    if (_1785)
    {
        uint _1786 = _641 >> 2u;
        uint _1791;
        if (intervalFailed)
        {
            _1791 = 6u;
        }
        else
        {
            _1791 = 4u;
        }
        results._m0[_1786] = 0u;
        results._m0[_1786 + 1u] = 3u;
        results._m0[_1786 + 2u] = _1791;
        results._m0[_1786 + 3u] = 0u;
        return;
    }
    uint _1796 = (_640 + 224u) >> 2u;
    uint2 _1798 = as_type<uint2>(float2(_1760.lo, _1760.hi));
    results._m0[_1796] = _1798.x;
    results._m0[_1796 + 1u] = _1798.y;
    Interval _1186 = Interval{ _1382.x, _1382.z };
    Interval _1187 = Interval{ as_type<float>(3204448256u), as_type<float>(3204448256u) };
    Interval _1809 = iadd(_1186, _1187, intervalFailed);
    Interval _1184 = Interval{ _1382.y, _1382.w };
    Interval _1185 = Interval{ as_type<float>(3204448256u), as_type<float>(3204448256u) };
    Interval _1818 = iadd(_1184, _1185, intervalFailed);
    int2 _1823 = int2(floor(float2(_1809.lo, _1818.lo)));
    int2 _1826 = int2(floor(float2(_1809.hi, _1818.hi)));
    bool _1839;
    if (!any(_1823 < int2(0)))
    {
        _1839 = any(_1826 >= int2(int(Settings.width), int(Settings.height)));
    }
    else
    {
        _1839 = true;
    }
    if (_1839)
    {
        uint _1840 = _641 >> 2u;
        uint _1845;
        if (intervalFailed)
        {
            _1845 = 6u;
        }
        else
        {
            _1845 = 2u;
        }
        results._m0[_1840] = 0u;
        results._m0[_1840 + 1u] = 3u;
        results._m0[_1840 + 2u] = _1845;
        results._m0[_1840 + 3u] = 0u;
        return;
    }
    if (any((_1826 - _1823) > int2(2)))
    {
        uint _1852 = _641 >> 2u;
        uint _1857;
        if (intervalFailed)
        {
            _1857 = 6u;
        }
        else
        {
            _1857 = 7u;
        }
        results._m0[_1852] = 0u;
        results._m0[_1852 + 1u] = 3u;
        results._m0[_1852 + 2u] = _1857;
        results._m0[_1852 + 3u] = 0u;
        return;
    }
    int _1862 = _1823.y;
    uint _1863;
    _1863 = 0u;
    spvUnsafeArray<Interval, 4> depths;
    uint _1864;
    for (int _1865 = _1862; _1865 <= _1826.y; _1863 = _1864, _1865++)
    {
        int _1868 = _1823.x;
        _1864 = _1863;
        uint _707;
        for (int _1869 = _1868; _1869 <= _1826.x; _1864 = _707, _1869++)
        {
            float _1873 = precise::max(_1809.lo, float(_1869));
            float _1875 = precise::min(_1809.hi, float(_1869 + 1));
            float _1877 = precise::max(_1818.lo, float(_1865));
            float _1879 = precise::min(_1818.hi, float(_1865 + 1));
            float _1880 = float(_1869);
            Interval _1182 = Interval{ _1873, _1875 };
            Interval _1183 = Interval{ as_type<float>(as_type<uint>(_1880) ^ 2147483648u), as_type<float>(as_type<uint>(_1880) ^ 2147483648u) };
            Interval _1889 = iadd(_1182, _1183, intervalFailed);
            float _1894 = precise::min(precise::max(_1889.lo, 0.0), 1.0);
            float _1895 = precise::min(precise::max(_1889.hi, 0.0), 1.0);
            float _1896 = float(_1865);
            Interval _1180 = Interval{ _1877, _1879 };
            Interval _1181 = Interval{ as_type<float>(as_type<uint>(_1896) ^ 2147483648u), as_type<float>(as_type<uint>(_1896) ^ 2147483648u) };
            Interval _1905 = iadd(_1180, _1181, intervalFailed);
            float _1910 = precise::min(precise::max(_1905.lo, 0.0), 1.0);
            float _1911 = precise::min(precise::max(_1905.hi, 0.0), 1.0);
            float _1912;
            float _1914;
            bool _1916;
            _1912 = 0.0;
            _1914 = 0.0;
            _1916 = true;
            float _1913;
            float _1915;
            bool _1917;
            for (uint _1918 = 0u; _1918 < 2u; _1912 = _1913, _1914 = _1915, _1916 = _1917, _1918++)
            {
                _1913 = _1912;
                _1915 = _1914;
                _1917 = _1916;
                float _1920;
                float _1921;
                bool _1922;
                for (uint _1923 = 0u; _1923 < 2u; _1913 = _1920, _1915 = _1921, _1917 = _1922, _1923++)
                {
                    uint _692 = (_1918 * 2u) + _1923;
                    depths[_692] = Interval{ 0.0, 0.0 };
                    int2 _1937 = min((int2(_1869, _1865) + int2(int(_1923), int(_1918))), int2(int(Settings.width - 1u), int(Settings.height - 1u)));
                    int2 pixel = _1937;
                    int2 param_var_pixel = _1937;
                    WitnessQuery param_var_q = WitnessQuery{ _1427, _1437, _1440, _1443, _1446 };
                    WitnessTap _1939 = witness_tap(param_var_pixel, param_var_q, Settings, source, intervalFailed, optical_product_upper);
                    if (_1917)
                    {
                        _1922 = _1939.visible;
                    }
                    else
                    {
                        _1922 = false;
                    }
                    if (_1939.visible)
                    {
                        Interval param_var_a = Interval{ 1.0, 1.0 };
                        Interval param_var_b = Interval{ _1939.depth, _1939.depth };
                        Interval _1944 = idiv(param_var_a, param_var_b, intervalFailed, interval_divide_upper);
                        depths[_692] = _1944;
                    }
                    bool param_var_pick = _1923 != 0u;
                    Interval param_var_a_1 = Interval{ _1894, _1895 };
                    Interval _1178 = Interval{ 1.0, 1.0 };
                    Interval _1179 = Interval{ as_type<float>(as_type<uint>(_1895) ^ 2147483648u), as_type<float>(as_type<uint>(_1894) ^ 2147483648u) };
                    Interval _1955 = iadd(_1178, _1179, intervalFailed);
                    Interval param_var_b_1 = _1955;
                    Interval param_var_a_2 = witness_select(param_var_pick, param_var_a_1, param_var_b_1);
                    bool param_var_pick_1 = _1918 != 0u;
                    Interval param_var_a_3 = Interval{ _1910, _1911 };
                    Interval _1176 = Interval{ 1.0, 1.0 };
                    Interval _1177 = Interval{ as_type<float>(as_type<uint>(_1911) ^ 2147483648u), as_type<float>(as_type<uint>(_1910) ^ 2147483648u) };
                    Interval _1966 = iadd(_1176, _1177, intervalFailed);
                    Interval param_var_b_2 = _1966;
                    Interval param_var_b_3 = witness_select(param_var_pick_1, param_var_a_3, param_var_b_2);
                    Interval _1968 = imul(param_var_a_2, param_var_b_3, intervalFailed, optical_product_upper);
                    if (_1968.hi <= 0.0)
                    {
                        _1920 = _1913;
                        _1921 = _1915;
                        continue;
                    }
                    if (!_1939.visible)
                    {
                        uint _1972 = _641 >> 2u;
                        uint _1977;
                        if (intervalFailed)
                        {
                            _1977 = 6u;
                        }
                        else
                        {
                            _1977 = 3u;
                        }
                        results._m0[_1972] = 0u;
                        results._m0[_1972 + 1u] = 3u;
                        results._m0[_1972 + 2u] = _1977;
                        results._m0[_1972 + 3u] = _1864;
                        return;
                    }
                    Interval param_var_a_4 = Interval{ _1913, _1915 };
                    Interval param_var_a_5 = _1968;
                    Interval param_var_b_4 = depths[_692];
                    Interval _1985 = imul(param_var_a_5, param_var_b_4, intervalFailed, optical_product_upper);
                    Interval param_var_b_5 = _1985;
                    Interval _1986 = iadd(param_var_a_4, param_var_b_5, intervalFailed);
                    float _1995;
                    float _1997;
                    float _1999;
                    _1995 = 1.9999999949504854157567024230957e-06;
                    _1997 = 1.9999999949504854157567024230957e-06;
                    _1999 = 1.9999999949504854157567024230957e-06;
                    float _1990;
                    float _1992;
                    float _1994;
                    float _1996;
                    float _1998;
                    float _2000;
                    float _1989 = 1.9999999949504854157567024230957e-06;
                    float _1991 = 1.9999999949504854157567024230957e-06;
                    float _1993 = 1.9999999949504854157567024230957e-06;
                    uint _2001 = 0u;
                    for (; _2001 < 2u; _1989 = _1990, _1991 = _1992, _1993 = _1994, _1995 = _1996, _1997 = _1998, _1999 = _2000, _2001++)
                    {
                        float _2003;
                        float _2005;
                        float _2007;
                        float _2009;
                        float _2011;
                        float _2013;
                        _2003 = 0.0;
                        _2005 = 0.0;
                        _2007 = 0.0;
                        _2009 = 0.0;
                        _2011 = 0.0;
                        _2013 = 0.0;
                        float _2004;
                        float _2006;
                        float _2008;
                        float _2010;
                        float _2012;
                        float _2014;
                        for (int _2015 = -1; _2015 <= 1; _2003 = _2004, _2005 = _2006, _2007 = _2008, _2009 = _2010, _2011 = _2012, _2013 = _2014, _2015 += 2)
                        {
                            int _2018;
                            if (_2001 == 0u)
                            {
                                _2018 = _2015;
                            }
                            else
                            {
                                _2018 = 0;
                            }
                            int _2020;
                            if (_2001 == 1u)
                            {
                                _2020 = _2015;
                            }
                            else
                            {
                                _2020 = 0;
                            }
                            int2 param_var_pixel_1 = _1937 + int2(_2018, _2020);
                            WitnessQuery param_var_q_1 = WitnessQuery{ _1427, _1437, _1440, _1443, _1446 };
                            WitnessTap _2023 = witness_tap(param_var_pixel_1, param_var_q_1, Settings, source, intervalFailed, optical_product_upper);
                            if (_2023.same)
                            {
                                Interval _1170 = Interval{ _2023.feature.x, _2023.feature.x };
                                Interval _1171 = Interval{ as_type<float>(as_type<uint>(_1939.feature.x) ^ 2147483648u), as_type<float>(as_type<uint>(_1939.feature.x) ^ 2147483648u) };
                                Interval _2052 = iadd(_1170, _1171, intervalFailed);
                                Interval _1172 = Interval{ _2023.feature.y, _2023.feature.y };
                                Interval _1173 = Interval{ as_type<float>(as_type<uint>(_1939.feature.y) ^ 2147483648u), as_type<float>(as_type<uint>(_1939.feature.y) ^ 2147483648u) };
                                Interval _2055 = iadd(_1172, _1173, intervalFailed);
                                Interval _1174 = Interval{ _2023.feature.z, _2023.feature.z };
                                Interval _1175 = Interval{ as_type<float>(as_type<uint>(_1939.feature.z) ^ 2147483648u), as_type<float>(as_type<uint>(_1939.feature.z) ^ 2147483648u) };
                                Interval _2058 = iadd(_1174, _1175, intervalFailed);
                                bool _2065;
                                if (_2052.lo <= 0.0)
                                {
                                    _2065 = _2052.hi >= 0.0;
                                }
                                else
                                {
                                    _2065 = false;
                                }
                                float _2072;
                                if (_2065)
                                {
                                    _2072 = 0.0;
                                }
                                else
                                {
                                    _2072 = precise::min(abs(_2052.lo), abs(_2052.hi));
                                }
                                bool _2082;
                                if (_2055.lo <= 0.0)
                                {
                                    _2082 = _2055.hi >= 0.0;
                                }
                                else
                                {
                                    _2082 = false;
                                }
                                float _2089;
                                if (_2082)
                                {
                                    _2089 = 0.0;
                                }
                                else
                                {
                                    _2089 = precise::min(abs(_2055.lo), abs(_2055.hi));
                                }
                                bool _2099;
                                if (_2058.lo <= 0.0)
                                {
                                    _2099 = _2058.hi >= 0.0;
                                }
                                else
                                {
                                    _2099 = false;
                                }
                                float _2106;
                                if (_2099)
                                {
                                    _2106 = 0.0;
                                }
                                else
                                {
                                    _2106 = precise::min(abs(_2058.lo), abs(_2058.hi));
                                }
                                Interval _1164 = Interval{ _2072, precise::max(abs(_2052.lo), abs(_2052.hi)) };
                                Interval _1165 = Interval{ 1.5, 1.5 };
                                Interval _2111 = imul(_1164, _1165, intervalFailed, optical_product_upper);
                                Interval _1166 = Interval{ _2089, precise::max(abs(_2055.lo), abs(_2055.hi)) };
                                Interval _1167 = Interval{ 1.5, 1.5 };
                                Interval _2113 = imul(_1166, _1167, intervalFailed, optical_product_upper);
                                Interval _1168 = Interval{ _2106, precise::max(abs(_2058.lo), abs(_2058.hi)) };
                                Interval _1169 = Interval{ 1.5, 1.5 };
                                Interval _2115 = imul(_1168, _1169, intervalFailed, optical_product_upper);
                                _2004 = precise::max(_2003, _2111.lo);
                                _2006 = precise::max(_2005, _2111.hi);
                                _2008 = precise::max(_2007, _2113.lo);
                                _2010 = precise::max(_2009, _2113.hi);
                                _2012 = precise::max(_2011, _2115.lo);
                                _2014 = precise::max(_2013, _2115.hi);
                            }
                            else
                            {
                                _2004 = _2003;
                                _2006 = _2005;
                                _2008 = _2007;
                                _2010 = _2009;
                                _2012 = _2011;
                                _2014 = _2013;
                            }
                        }
                        bool param_var_pick_2 = _2001 == 0u;
                        Interval param_var_a_6 = Interval{ _1873, _1875 };
                        Interval param_var_b_6 = Interval{ _1877, _1879 };
                        float _2134 = float(pixel[_2001]);
                        Interval _1162 = witness_select(param_var_pick_2, param_var_a_6, param_var_b_6);
                        Interval _1163 = Interval{ as_type<float>(as_type<uint>(_2134) ^ 2147483648u), as_type<float>(as_type<uint>(_2134) ^ 2147483648u) };
                        Interval _2142 = iadd(_1162, _1163, intervalFailed);
                        bool _2149;
                        if (_2142.lo <= 0.0)
                        {
                            _2149 = _2142.hi >= 0.0;
                        }
                        else
                        {
                            _2149 = false;
                        }
                        float _2156;
                        if (_2149)
                        {
                            _2156 = 0.0;
                        }
                        else
                        {
                            _2156 = precise::min(abs(_2142.lo), abs(_2142.hi));
                        }
                        float _2159 = precise::max(abs(_2142.lo), abs(_2142.hi));
                        Interval _1156 = Interval{ _2003, _2005 };
                        Interval _1157 = Interval{ _2156, _2159 };
                        Interval _2162 = imul(_1156, _1157, intervalFailed, optical_product_upper);
                        Interval _1158 = Interval{ _2007, _2009 };
                        Interval _1159 = Interval{ _2156, _2159 };
                        Interval _2165 = imul(_1158, _1159, intervalFailed, optical_product_upper);
                        Interval _1160 = Interval{ _2011, _2013 };
                        Interval _1161 = Interval{ _2156, _2159 };
                        Interval _2168 = imul(_1160, _1161, intervalFailed, optical_product_upper);
                        Interval _1150 = Interval{ _1999, _1989 };
                        Interval _1151 = _2162;
                        Interval _2170 = iadd(_1150, _1151, intervalFailed);
                        Interval _1152 = Interval{ _1997, _1991 };
                        Interval _1153 = _2165;
                        Interval _2172 = iadd(_1152, _1153, intervalFailed);
                        Interval _1154 = Interval{ _1995, _1993 };
                        Interval _1155 = _2168;
                        Interval _2174 = iadd(_1154, _1155, intervalFailed);
                        _2000 = _2170.lo;
                        _1990 = _2170.hi;
                        _1998 = _2172.lo;
                        _1992 = _2172.hi;
                        _1996 = _2174.lo;
                        _1994 = _2174.hi;
                    }
                    Interval _1144 = Interval{ _1939.feature.x, _1939.feature.x };
                    Interval _1145 = Interval{ as_type<float>(as_type<uint>(_1492) ^ 2147483648u), as_type<float>(as_type<uint>(_1491) ^ 2147483648u) };
                    Interval _2198 = iadd(_1144, _1145, intervalFailed);
                    Interval _1146 = Interval{ _1939.feature.y, _1939.feature.y };
                    Interval _1147 = Interval{ as_type<float>(as_type<uint>(_1494) ^ 2147483648u), as_type<float>(as_type<uint>(_1493) ^ 2147483648u) };
                    Interval _2201 = iadd(_1146, _1147, intervalFailed);
                    Interval _1148 = Interval{ _1939.feature.z, _1939.feature.z };
                    Interval _1149 = Interval{ as_type<float>(as_type<uint>(_1496) ^ 2147483648u), as_type<float>(as_type<uint>(_1495) ^ 2147483648u) };
                    Interval _2204 = iadd(_1148, _1149, intervalFailed);
                    float _2228;
                    if ((_1440 >> 8u) == 1u)
                    {
                        _2228 = 0.04999999701976776123046875;
                    }
                    else
                    {
                        _2228 = 0.0199999995529651641845703125;
                    }
                    bool _2233;
                    if (!intervalFailed)
                    {
                        _2233 = precise::max(abs(_2198.lo), abs(_2198.hi)) > precise::min(_1999, _2228);
                    }
                    else
                    {
                        _2233 = true;
                    }
                    bool _2237;
                    if (!_2233)
                    {
                        _2237 = precise::max(abs(_2201.lo), abs(_2201.hi)) > precise::min(_1997, _2228);
                    }
                    else
                    {
                        _2237 = true;
                    }
                    bool _2241;
                    if (!_2237)
                    {
                        _2241 = precise::max(abs(_2204.lo), abs(_2204.hi)) > precise::min(_1995, _2228);
                    }
                    else
                    {
                        _2241 = true;
                    }
                    if (_2241)
                    {
                        uint _2242 = _641 >> 2u;
                        uint _2247;
                        if (intervalFailed)
                        {
                            _2247 = 6u;
                        }
                        else
                        {
                            _2247 = 5u;
                        }
                        results._m0[_2242] = 0u;
                        results._m0[_2242 + 1u] = 7u;
                        results._m0[_2242 + 2u] = _2247;
                        results._m0[_2242 + 3u] = _1864;
                        return;
                    }
                    _1920 = _1986.lo;
                    _1921 = _1986.hi;
                }
            }
            float _2305;
            float _2306;
            if (_1916)
            {
                Interval param_var_a_7 = depths[0];
                Interval param_var_a_8 = Interval{ _1894, _1895 };
                Interval _1142 = depths[1];
                Interval _1143 = Interval{ as_type<float>(as_type<uint>(depths[0].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[0].lo) ^ 2147483648u) };
                Interval _2268 = iadd(_1142, _1143, intervalFailed);
                Interval param_var_b_7 = _2268;
                Interval _2269 = imul(param_var_a_8, param_var_b_7, intervalFailed, optical_product_upper);
                Interval param_var_b_8 = _2269;
                Interval _2270 = iadd(param_var_a_7, param_var_b_8, intervalFailed);
                Interval param_var_a_9 = depths[2];
                Interval param_var_a_10 = Interval{ _1894, _1895 };
                Interval _1140 = depths[3];
                Interval _1141 = Interval{ as_type<float>(as_type<uint>(depths[2].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[2].lo) ^ 2147483648u) };
                Interval _2287 = iadd(_1140, _1141, intervalFailed);
                Interval param_var_b_9 = _2287;
                Interval _2288 = imul(param_var_a_10, param_var_b_9, intervalFailed, optical_product_upper);
                Interval param_var_b_10 = _2288;
                Interval _2289 = iadd(param_var_a_9, param_var_b_10, intervalFailed);
                Interval param_var_a_11 = _2270;
                Interval param_var_a_12 = Interval{ _1910, _1911 };
                Interval _1138 = _2289;
                Interval _1139 = Interval{ as_type<float>(as_type<uint>(_2270.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2270.lo) ^ 2147483648u) };
                Interval _2300 = iadd(_1138, _1139, intervalFailed);
                Interval param_var_b_11 = _2300;
                Interval _2301 = imul(param_var_a_12, param_var_b_11, intervalFailed, optical_product_upper);
                Interval param_var_b_12 = _2301;
                Interval _2302 = iadd(param_var_a_11, param_var_b_12, intervalFailed);
                _2305 = _2302.lo;
                _2306 = _2302.hi;
            }
            else
            {
                _2305 = _1912;
                _2306 = _1914;
            }
            Interval param_var_a_13 = Interval{ 1.0, 1.0 };
            Interval param_var_b_13 = Interval{ _2305, _2306 };
            Interval _2308 = idiv(param_var_a_13, param_var_b_13, intervalFailed, interval_divide_upper);
            Interval _1136 = _2308;
            Interval _1137 = Interval{ as_type<float>(as_type<uint>(_1760.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_1760.lo) ^ 2147483648u) };
            Interval _2318 = iadd(_1136, _1137, intervalFailed);
            float _2719;
            if (_1916)
            {
                float _2517;
                float _2518;
                float _2519;
                float _2520;
                float _2521;
                float _2522;
                float _2523;
                float _2524;
                float _2525;
                float _2526;
                float _2527;
                float _2528;
                if (frames._m0[(_642 + 488u) >> 2u] != 0u)
                {
                    uint _2333 = (_642 + 288u) >> 2u;
                    float4 _2343 = as_type<float4>(uint4(frames._m0[_2333], frames._m0[_2333 + 1u], frames._m0[_2333 + 2u], frames._m0[_2333 + 3u]));
                    float _2344 = _2343.x;
                    float _2345 = _2343.y;
                    float _2346 = _2343.z;
                    uint _2347 = (_642 + 304u) >> 2u;
                    float4 _2357 = as_type<float4>(uint4(frames._m0[_2347], frames._m0[_2347 + 1u], frames._m0[_2347 + 2u], frames._m0[_2347 + 3u]));
                    float _2358 = _2357.x;
                    float _2359 = _2357.y;
                    float _2360 = _2357.z;
                    _2517 = _2358;
                    _2518 = _2358;
                    _2519 = _2359;
                    _2520 = _2359;
                    _2521 = _2360;
                    _2522 = _2360;
                    _2523 = _2344;
                    _2524 = _2344;
                    _2525 = _2345;
                    _2526 = _2345;
                    _2527 = _2346;
                    _2528 = _2346;
                }
                else
                {
                    uint _2361 = _642 >> 2u;
                    uint _2363 = frames._m0[_2361];
                    uint _2365 = frames._m0[_2361 + 1u];
                    uint _2367 = frames._m0[_2361 + 2u];
                    uint _2369 = frames._m0[_2361 + 3u];
                    float4 _2371 = as_type<float4>(uint4(_2363, _2365, _2367, _2369));
                    uint _2372 = (_642 + 16u) >> 2u;
                    uint _2374 = frames._m0[_2372];
                    uint _2376 = frames._m0[_2372 + 1u];
                    uint _2378 = frames._m0[_2372 + 2u];
                    uint _2380 = frames._m0[_2372 + 3u];
                    float4 _2382 = as_type<float4>(uint4(_2374, _2376, _2378, _2380));
                    uint _2383 = (_642 + 32u) >> 2u;
                    uint _2385 = frames._m0[_2383];
                    uint _2387 = frames._m0[_2383 + 1u];
                    uint _2389 = frames._m0[_2383 + 2u];
                    uint _2391 = frames._m0[_2383 + 3u];
                    float4 _2393 = as_type<float4>(uint4(_2385, _2387, _2389, _2391));
                    float _2394 = _2371.x;
                    float _2395 = _2371.y;
                    float _2396 = _2371.z;
                    float _2511;
                    float _2512;
                    float _2513;
                    float _2514;
                    float _2515;
                    float _2516;
                    if (all(float3(_2371.w, _2382.w, _2393.w) == float3(2.0)))
                    {
                        float _2406 = _2382.x;
                        float _2407 = _2382.y;
                        float _2408 = _2382.z;
                        _2511 = _2406;
                        _2512 = _2406;
                        _2513 = _2407;
                        _2514 = _2407;
                        _2515 = _2408;
                        _2516 = _2408;
                    }
                    else
                    {
                        float _2409 = _2382.x;
                        float _2410 = _2382.y;
                        float _2411 = _2382.z;
                        Interval _1130 = Interval{ _2409, _2409 };
                        Interval _1131 = Interval{ as_type<float>(as_type<uint>(_2394) ^ 2147483648u), as_type<float>(as_type<uint>(_2394) ^ 2147483648u) };
                        Interval _2432 = iadd(_1130, _1131, intervalFailed);
                        Interval _1132 = Interval{ _2410, _2410 };
                        Interval _1133 = Interval{ as_type<float>(as_type<uint>(_2395) ^ 2147483648u), as_type<float>(as_type<uint>(_2395) ^ 2147483648u) };
                        Interval _2435 = iadd(_1132, _1133, intervalFailed);
                        Interval _1134 = Interval{ _2411, _2411 };
                        Interval _1135 = Interval{ as_type<float>(as_type<uint>(_2396) ^ 2147483648u), as_type<float>(as_type<uint>(_2396) ^ 2147483648u) };
                        Interval _2438 = iadd(_1134, _1135, intervalFailed);
                        float _2439 = _2393.x;
                        float _2440 = _2393.y;
                        float _2441 = _2393.z;
                        Interval _1124 = Interval{ _2439, _2439 };
                        Interval _1125 = Interval{ as_type<float>(as_type<uint>(_2394) ^ 2147483648u), as_type<float>(as_type<uint>(_2394) ^ 2147483648u) };
                        Interval _2462 = iadd(_1124, _1125, intervalFailed);
                        Interval _1126 = Interval{ _2440, _2440 };
                        Interval _1127 = Interval{ as_type<float>(as_type<uint>(_2395) ^ 2147483648u), as_type<float>(as_type<uint>(_2395) ^ 2147483648u) };
                        Interval _2465 = iadd(_1126, _1127, intervalFailed);
                        Interval _1128 = Interval{ _2441, _2441 };
                        Interval _1129 = Interval{ as_type<float>(as_type<uint>(_2396) ^ 2147483648u), as_type<float>(as_type<uint>(_2396) ^ 2147483648u) };
                        Interval _2468 = iadd(_1128, _1129, intervalFailed);
                        Interval _1112 = _2435;
                        Interval _1113 = _2468;
                        Interval _2469 = imul(_1112, _1113, intervalFailed, optical_product_upper);
                        Interval _1114 = _2438;
                        Interval _1115 = _2465;
                        Interval _2470 = imul(_1114, _1115, intervalFailed, optical_product_upper);
                        Interval _1110 = _2469;
                        Interval _1111 = Interval{ as_type<float>(as_type<uint>(_2470.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2470.lo) ^ 2147483648u) };
                        Interval _2480 = iadd(_1110, _1111, intervalFailed);
                        Interval _1116 = _2438;
                        Interval _1117 = _2462;
                        Interval _2481 = imul(_1116, _1117, intervalFailed, optical_product_upper);
                        Interval _1118 = _2432;
                        Interval _1119 = _2468;
                        Interval _2482 = imul(_1118, _1119, intervalFailed, optical_product_upper);
                        Interval _1108 = _2481;
                        Interval _1109 = Interval{ as_type<float>(as_type<uint>(_2482.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2482.lo) ^ 2147483648u) };
                        Interval _2492 = iadd(_1108, _1109, intervalFailed);
                        Interval _1120 = _2432;
                        Interval _1121 = _2465;
                        Interval _2493 = imul(_1120, _1121, intervalFailed, optical_product_upper);
                        Interval _1122 = _2435;
                        Interval _1123 = _2462;
                        Interval _2494 = imul(_1122, _1123, intervalFailed, optical_product_upper);
                        Interval _1106 = _2493;
                        Interval _1107 = Interval{ as_type<float>(as_type<uint>(_2494.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2494.lo) ^ 2147483648u) };
                        Interval _2504 = iadd(_1106, _1107, intervalFailed);
                        _2511 = _2480.lo;
                        _2512 = _2480.hi;
                        _2513 = _2492.lo;
                        _2514 = _2492.hi;
                        _2515 = _2504.lo;
                        _2516 = _2504.hi;
                    }
                    _2517 = _2511;
                    _2518 = _2512;
                    _2519 = _2513;
                    _2520 = _2514;
                    _2521 = _2515;
                    _2522 = _2516;
                    _2523 = _2394;
                    _2524 = _2394;
                    _2525 = _2395;
                    _2526 = _2395;
                    _2527 = _2396;
                    _2528 = _2396;
                }
                Interval _1096 = Interval{ _2523, _2524 };
                Interval _1097 = Interval{ _2517, _2518 };
                Interval _2531 = imul(_1096, _1097, intervalFailed, optical_product_upper);
                Interval _1098 = _2531;
                Interval _1099 = Interval{ _2525, _2526 };
                Interval _1100 = Interval{ _2519, _2520 };
                Interval _2534 = imul(_1099, _1100, intervalFailed, optical_product_upper);
                Interval _1101 = _2534;
                Interval _2535 = iadd(_1098, _1101, intervalFailed);
                Interval _1102 = _2535;
                Interval _1103 = Interval{ _2527, _2528 };
                Interval _1104 = Interval{ _2521, _2522 };
                Interval _2538 = imul(_1103, _1104, intervalFailed, optical_product_upper);
                Interval _1105 = _2538;
                Interval _2539 = iadd(_1102, _1105, intervalFailed);
                uint _2540 = (_642 + 48u) >> 2u;
                uint _2542 = frames._m0[_2540];
                uint _2544 = frames._m0[_2540 + 1u];
                uint _2546 = frames._m0[_2540 + 2u];
                uint _2548 = frames._m0[_2540 + 3u];
                float4 _2550 = as_type<float4>(uint4(_2542, _2544, _2546, _2548));
                float _705 = spvFAdd(float(_1869), 0.5);
                float _2552 = _2550.z;
                Interval _1094 = Interval{ _705, _705 };
                Interval _1095 = Interval{ as_type<float>(as_type<uint>(_2552) ^ 2147483648u), as_type<float>(as_type<uint>(_2552) ^ 2147483648u) };
                Interval _2561 = iadd(_1094, _1095, intervalFailed);
                Interval param_var_a_14 = _2561;
                float _2562 = _2550.x;
                Interval param_var_b_14 = Interval{ _2562, _2562 };
                Interval _2564 = idiv(param_var_a_14, param_var_b_14, intervalFailed, interval_divide_upper);
                float _706 = spvFAdd(float(_1865), 0.5);
                float _2566 = _2550.w;
                Interval _1092 = Interval{ _706, _706 };
                Interval _1093 = Interval{ as_type<float>(as_type<uint>(_2566) ^ 2147483648u), as_type<float>(as_type<uint>(_2566) ^ 2147483648u) };
                Interval _2575 = iadd(_1092, _1093, intervalFailed);
                Interval param_var_a_15 = _2575;
                float _2576 = _2550.y;
                Interval param_var_b_15 = Interval{ _2576, _2576 };
                Interval _2578 = idiv(param_var_a_15, param_var_b_15, intervalFailed, interval_divide_upper);
                Interval _1082 = _2564;
                Interval _1083 = Interval{ _2517, _2518 };
                Interval _2580 = imul(_1082, _1083, intervalFailed, optical_product_upper);
                Interval _1084 = _2580;
                Interval _1085 = _2578;
                Interval _1086 = Interval{ _2519, _2520 };
                Interval _2582 = imul(_1085, _1086, intervalFailed, optical_product_upper);
                Interval _1087 = _2582;
                Interval _2583 = iadd(_1084, _1087, intervalFailed);
                Interval _1088 = _2583;
                Interval _1089 = Interval{ 1.0, 1.0 };
                Interval _1090 = Interval{ _2521, _2522 };
                Interval _2585 = imul(_1089, _1090, intervalFailed, optical_product_upper);
                Interval _1091 = _2585;
                Interval _2586 = iadd(_1088, _1091, intervalFailed);
                Interval param_var_a_16 = _2586;
                Interval param_var_b_16 = _2539;
                Interval _2587 = idiv(param_var_a_16, param_var_b_16, intervalFailed, interval_divide_upper);
                Interval param_var_a_17 = Interval{ _2517, _2518 };
                Interval param_var_a_18 = _2539;
                float _2589 = _2550.x;
                Interval param_var_b_17 = Interval{ _2589, _2589 };
                Interval _2591 = imul(param_var_a_18, param_var_b_17, intervalFailed, optical_product_upper);
                Interval param_var_b_18 = _2591;
                Interval _2592 = idiv(param_var_a_17, param_var_b_18, intervalFailed, interval_divide_upper);
                Interval param_var_a_19 = Interval{ _2519, _2520 };
                Interval param_var_a_20 = _2539;
                float _2594 = _2550.y;
                Interval param_var_b_19 = Interval{ _2594, _2594 };
                Interval _2596 = imul(param_var_a_20, param_var_b_19, intervalFailed, optical_product_upper);
                Interval param_var_b_20 = _2596;
                Interval _2597 = idiv(param_var_a_19, param_var_b_20, intervalFailed, interval_divide_upper);
                Interval _1080 = depths[0];
                Interval _1081 = Interval{ as_type<float>(as_type<uint>(_2587.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2587.lo) ^ 2147483648u) };
                Interval _2609 = iadd(_1080, _1081, intervalFailed);
                Interval _1078 = depths[1];
                Interval _1079 = Interval{ as_type<float>(as_type<uint>(depths[0].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[0].lo) ^ 2147483648u) };
                Interval _2623 = iadd(_1078, _1079, intervalFailed);
                Interval _1076 = _2623;
                Interval _1077 = Interval{ as_type<float>(as_type<uint>(_2592.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2592.lo) ^ 2147483648u) };
                Interval _2633 = iadd(_1076, _1077, intervalFailed);
                Interval _1074 = depths[2];
                Interval _1075 = Interval{ as_type<float>(as_type<uint>(depths[0].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[0].lo) ^ 2147483648u) };
                Interval _2647 = iadd(_1074, _1075, intervalFailed);
                Interval _1072 = _2647;
                Interval _1073 = Interval{ as_type<float>(as_type<uint>(_2597.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2597.lo) ^ 2147483648u) };
                Interval _2657 = iadd(_1072, _1073, intervalFailed);
                Interval _1070 = depths[3];
                Interval _1071 = Interval{ as_type<float>(as_type<uint>(depths[1].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[1].lo) ^ 2147483648u) };
                Interval _2671 = iadd(_1070, _1071, intervalFailed);
                Interval _1068 = _2671;
                Interval _1069 = Interval{ as_type<float>(as_type<uint>(depths[2].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[2].lo) ^ 2147483648u) };
                Interval _2683 = iadd(_1068, _1069, intervalFailed);
                Interval param_var_a_21 = _2683;
                Interval param_var_b_21 = depths[0];
                Interval _2686 = iadd(param_var_a_21, param_var_b_21, intervalFailed);
                Interval param_var_a_22 = _2609;
                Interval param_var_a_23 = Interval{ _1894, _1895 };
                Interval param_var_b_22 = _2633;
                Interval _2688 = imul(param_var_a_23, param_var_b_22, intervalFailed, optical_product_upper);
                Interval param_var_b_23 = _2688;
                Interval _2689 = iadd(param_var_a_22, param_var_b_23, intervalFailed);
                Interval param_var_a_24 = _2689;
                Interval param_var_a_25 = Interval{ _1910, _1911 };
                Interval param_var_a_26 = _2657;
                Interval param_var_a_27 = Interval{ _1894, _1895 };
                Interval param_var_b_24 = _2686;
                Interval _2692 = imul(param_var_a_27, param_var_b_24, intervalFailed, optical_product_upper);
                Interval param_var_b_25 = _2692;
                Interval _2693 = iadd(param_var_a_26, param_var_b_25, intervalFailed);
                Interval param_var_b_26 = _2693;
                Interval _2694 = imul(param_var_a_25, param_var_b_26, intervalFailed, optical_product_upper);
                Interval param_var_b_27 = _2694;
                Interval _2695 = iadd(param_var_a_24, param_var_b_27, intervalFailed);
                bool _2702;
                if (_2695.lo <= 0.0)
                {
                    _2702 = _2695.hi >= 0.0;
                }
                else
                {
                    _2702 = false;
                }
                float _2709;
                if (_2702)
                {
                    _2709 = 0.0;
                }
                else
                {
                    _2709 = precise::min(abs(_2695.lo), abs(_2695.hi));
                }
                Interval param_var_a_28 = Interval{ _2709, precise::max(abs(_2695.lo), abs(_2695.hi)) };
                Interval param_var_a_29 = Interval{ _2305, _2306 };
                Interval param_var_a_30 = Interval{ 1.0, 1.0 };
                Interval param_var_b_28 = _1760;
                Interval _2715 = idiv(param_var_a_30, param_var_b_28, intervalFailed, interval_divide_upper);
                Interval param_var_b_29 = _2715;
                Interval _2716 = imul(param_var_a_29, param_var_b_29, intervalFailed, optical_product_upper);
                Interval param_var_b_30 = _2716;
                Interval _2717 = idiv(param_var_a_28, param_var_b_30, intervalFailed, interval_divide_upper);
                _2719 = _2717.hi;
            }
            else
            {
                _2719 = precise::max(abs(_2318.lo), abs(_2318.hi));
            }
            Interval param_var_a_31 = _1760;
            Interval param_var_b_31 = Interval{ 0.004999999888241291046142578125, 0.004999999888241291046142578125 };
            Interval _2720 = imul(param_var_a_31, param_var_b_31, intervalFailed, optical_product_upper);
            bool _2726;
            if (!intervalFailed)
            {
                _2726 = _2305 <= 0.0;
            }
            else
            {
                _2726 = true;
            }
            bool _2729;
            if (!_2726)
            {
                _2729 = _2719 > precise::max(0.00999999977648258209228515625, _2720.lo);
            }
            else
            {
                _2729 = true;
            }
            if (_2729)
            {
                uint _2730 = _641 >> 2u;
                uint _2735;
                if (intervalFailed)
                {
                    _2735 = 6u;
                }
                else
                {
                    _2735 = 4u;
                }
                results._m0[_2730] = 0u;
                results._m0[_2730 + 1u] = 23u;
                results._m0[_2730 + 2u] = _2735;
                results._m0[_2730 + 3u] = _1864;
                return;
            }
            _707 = _1864 + 1u;
        }
    }
    bool _2743;
    if (!intervalFailed)
    {
        _2743 = _1863 == 0u;
    }
    else
    {
        _2743 = true;
    }
    if (_2743)
    {
        uint _2744 = _641 >> 2u;
        results._m0[_2744] = 0u;
        results._m0[_2744 + 1u] = 0u;
        results._m0[_2744 + 2u] = 6u;
        results._m0[_2744 + 3u] = _1863;
        return;
    }
    uint _2750 = _641 >> 2u;
    results._m0[_2750] = 1u;
    results._m0[_2750 + 1u] = 31u;
    results._m0[_2750 + 2u] = 0u;
    results._m0[_2750 + 3u] = _1863;
}

kernel void feature_witness_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& frames [[buffer(3)]], device type_ByteAddressBuffer& queries [[buffer(4)]], device type_ByteAddressBuffer& source [[buffer(5)]], device type_RWByteAddressBuffer& results [[buffer(6)]], constant type_WitnessSettings& WitnessSettings [[buffer(1)]], constant type_TargetSettings& TargetSettings [[buffer(2)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    bool intervalFailed = false;
    float optical_product_upper = 0.0;
    float interval_divide_upper = 0.0;
    uint3 param_var_id = gl_GlobalInvocationID;
    src_feature_witness_main(param_var_id, Settings, frames, queries, source, results, WitnessSettings, intervalFailed, optical_product_upper, interval_divide_upper, TargetSettings);
}

