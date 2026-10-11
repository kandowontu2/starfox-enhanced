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
    bool _3954;
    if (as_type<uint>(a.lo) == as_type<uint>(x))
    {
        _3954 = as_type<uint>(a.hi) == as_type<uint>(x);
    }
    else
    {
        _3954 = false;
    }
    return _3954;
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
    uint _3981;
    if (x > 0.0)
    {
        _3981 = 1u;
    }
    else
    {
        _3981 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _3981);
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
    uint _3967;
    if (x < 0.0)
    {
        _3967 = 1u;
    }
    else
    {
        _3967 = 4294967295u;
    }
    return as_type<float>(as_type<uint>(x) + _3967);
}

static __attribute__((noinline))
float optical_add_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    uint _4147 = as_type<uint>(a) & 2147483647u;
    uint _4150 = as_type<uint>(b) & 2147483647u;
    bool _4153;
    if (_4147 < 2139095040u)
    {
        _4153 = _4150 < 2139095040u;
    }
    else
    {
        _4153 = false;
    }
    if (_4153)
    {
        if (_4147 == 0u)
        {
            return b;
        }
        if (_4150 == 0u)
        {
            return a;
        }
        bool _4160;
        if (_4147 >= 8388608u)
        {
            _4160 = _4150 < 8388608u;
        }
        else
        {
            _4160 = true;
        }
        if (_4160)
        {
            intervalFailed = true;
        }
        bool _4163;
        if (_4147 >= 562036736u)
        {
            _4163 = _4147 <= 1568669696u;
        }
        else
        {
            _4163 = false;
        }
        bool _4165;
        if (_4163)
        {
            _4165 = _4150 >= 562036736u;
        }
        else
        {
            _4165 = false;
        }
        bool _4167;
        if (_4165)
        {
            _4167 = _4150 <= 1568669696u;
        }
        else
        {
            _4167 = false;
        }
        if (_4167)
        {
            float _4171;
            if (_4147 >= _4150)
            {
                _4171 = a;
            }
            else
            {
                _4171 = b;
            }
            float _4175;
            if (_4147 >= _4150)
            {
                _4175 = b;
            }
            else
            {
                _4175 = a;
            }
            float _844 = spvFAdd(_4171, _4175);
            float _846 = spvFSub(_4175, spvFSub(_844, _4171));
            if (upper)
            {
                float _4179;
                if (_846 > 0.0)
                {
                    float param_var_x = _844;
                    float _4178 = interval_up(param_var_x, intervalFailed);
                    _4179 = _4178;
                }
                else
                {
                    _4179 = _844;
                }
                return _4179;
            }
            float _4182;
            if (_846 < 0.0)
            {
                float param_var_x_1 = _844;
                float _4181 = interval_down(param_var_x_1, intervalFailed);
                _4182 = _4181;
            }
            else
            {
                _4182 = _844;
            }
            return _4182;
        }
    }
    float _847 = spvFAdd(a, b);
    float _4188;
    if (upper)
    {
        float param_var_x_2 = _847;
        float _4186 = interval_up(param_var_x_2, intervalFailed);
        _4188 = _4186;
    }
    else
    {
        float param_var_x_3 = _847;
        float _4187 = interval_down(param_var_x_3, intervalFailed);
        _4188 = _4187;
    }
    return _4188;
}

static inline __attribute__((always_inline))
Interval iadd(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed)
{
    bool _3815;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3815 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3815 = false;
    }
    bool _3819;
    if (_3815)
    {
        _3819 = a.lo <= a.hi;
    }
    else
    {
        _3819 = false;
    }
    bool _3838;
    if (_3819)
    {
        bool _3833;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3833 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3833 = false;
        }
        bool _3837;
        if (_3833)
        {
            _3837 = b.lo <= b.hi;
        }
        else
        {
            _3837 = false;
        }
        _3838 = _3837;
    }
    else
    {
        _3838 = false;
    }
    if (_3838)
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
    float _3849 = optical_add_bound(param_var_a_2, param_var_b, param_var_upper, intervalFailed);
    float param_var_a_3 = a.hi;
    float param_var_b_1 = b.hi;
    bool param_var_upper_1 = true;
    float _3854 = optical_add_bound(param_var_a_3, param_var_b_1, param_var_upper_1, intervalFailed);
    return Interval{ _3849, _3854 };
}

static inline __attribute__((always_inline))
uint interval_product_extrema(thread const float& alo, thread const float& ahi, thread const float& blo, thread const float& bhi)
{
    uint _3985 = as_type<uint>(alo) & 2147483647u;
    uint _3988 = as_type<uint>(ahi) & 2147483647u;
    uint _3991 = as_type<uint>(blo) & 2147483647u;
    uint _3994 = as_type<uint>(bhi) & 2147483647u;
    bool _3997;
    if (_3985 >= 813694976u)
    {
        _3997 = _3985 > 1317011456u;
    }
    else
    {
        _3997 = true;
    }
    bool _4000;
    if (!_3997)
    {
        _4000 = _3988 < 813694976u;
    }
    else
    {
        _4000 = true;
    }
    bool _4003;
    if (!_4000)
    {
        _4003 = _3988 > 1317011456u;
    }
    else
    {
        _4003 = true;
    }
    bool _4006;
    if (!_4003)
    {
        _4006 = _3991 < 813694976u;
    }
    else
    {
        _4006 = true;
    }
    bool _4009;
    if (!_4006)
    {
        _4009 = _3991 > 1317011456u;
    }
    else
    {
        _4009 = true;
    }
    bool _4012;
    if (!_4009)
    {
        _4012 = _3994 < 813694976u;
    }
    else
    {
        _4012 = true;
    }
    bool _4015;
    if (!_4012)
    {
        _4015 = _3994 > 1317011456u;
    }
    else
    {
        _4015 = true;
    }
    bool _4020;
    if (!_4015)
    {
        _4020 = alo > ahi;
    }
    else
    {
        _4020 = true;
    }
    bool _4025;
    if (!_4020)
    {
        _4025 = blo > bhi;
    }
    else
    {
        _4025 = true;
    }
    if (_4025)
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
    uint _4043 = as_type<uint>(a);
    uint _4044 = _4043 & 2147483647u;
    uint _4046 = as_type<uint>(b);
    uint _4047 = _4046 & 2147483647u;
    bool _4050;
    if (_4044 < 2139095040u)
    {
        _4050 = _4047 < 2139095040u;
    }
    else
    {
        _4050 = false;
    }
    if (_4050)
    {
        bool _4053;
        if (_4044 != 0u)
        {
            _4053 = _4047 == 0u;
        }
        else
        {
            _4053 = true;
        }
        if (_4053)
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
            float _4070 = as_type<float>(as_type<uint>(b) ^ 2147483648u);
            optical_product_upper = _4070;
            return _4070;
        }
        if (as_type<uint>(b) == 3212836864u)
        {
            float _4077 = as_type<float>(as_type<uint>(a) ^ 2147483648u);
            optical_product_upper = _4077;
            return _4077;
        }
        bool _4080;
        if (_4044 >= 8388608u)
        {
            _4080 = _4047 < 8388608u;
        }
        else
        {
            _4080 = true;
        }
        if (_4080)
        {
            intervalFailed = true;
        }
        bool _4083;
        if (_4044 >= 813694976u)
        {
            _4083 = _4044 <= 1317011456u;
        }
        else
        {
            _4083 = false;
        }
        bool _4085;
        if (_4083)
        {
            _4085 = _4047 >= 813694976u;
        }
        else
        {
            _4085 = false;
        }
        bool _4087;
        if (_4085)
        {
            _4087 = _4047 <= 1317011456u;
        }
        else
        {
            _4087 = false;
        }
        if (_4087)
        {
            float _830 = spvFMul(a, b);
            uint _4094 = _4043 & 65535u;
            uint _4095 = ((_4043 & 8388607u) | 8388608u) >> 16u;
            uint _4096 = _4046 & 65535u;
            uint _4097 = ((_4046 & 8388607u) | 8388608u) >> 16u;
            uint _831 = _4094 * _4096;
            uint _834 = (_4094 * _4097) + (_4095 * _4096);
            uint _835 = _831 + (_834 << 16u);
            uint _4101;
            if (_835 < _831)
            {
                _4101 = 1u;
            }
            else
            {
                _4101 = 0u;
            }
            uint _838 = ((_4095 * _4097) + (_834 >> 16u)) + _4101;
            uint _4102 = as_type<uint>(_830);
            uint _840 = (((_4102 & 2147483647u) >> 23u) - (_4044 >> 23u)) - (_4047 >> 23u);
            uint _841 = _840 + 150u;
            bool _4109;
            if (_841 >= 23u)
            {
                _4109 = _841 > 24u;
            }
            else
            {
                _4109 = true;
            }
            if (_4109)
            {
                intervalFailed = true;
                float param_var_x = _830;
                float _4110 = interval_up(param_var_x, intervalFailed);
                optical_product_upper = _4110;
                float param_var_x_1 = _830;
                float _4111 = interval_down(param_var_x_1, intervalFailed);
                return _4111;
            }
            uint _4113 = (_4102 & 8388607u) | 8388608u;
            uint _4115 = _4113 << (_841 & 31u);
            uint _4117 = _4113 >> ((4294967178u - _840) & 31u);
            bool _4122;
            if (_838 <= _4117)
            {
                bool _4121;
                if (_838 == _4117)
                {
                    _4121 = _835 > _4115;
                }
                else
                {
                    _4121 = false;
                }
                _4122 = _4121;
            }
            else
            {
                _4122 = true;
            }
            bool _4127;
            if (_838 >= _4117)
            {
                bool _4126;
                if (_838 == _4117)
                {
                    _4126 = _835 < _4115;
                }
                else
                {
                    _4126 = false;
                }
                _4127 = _4126;
            }
            else
            {
                _4127 = true;
            }
            bool _4134 = ((as_type<uint>(a) ^ as_type<uint>(b)) & 2147483648u) != 0u;
            bool _4135;
            if (_4134)
            {
                _4135 = _4127;
            }
            else
            {
                _4135 = _4122;
            }
            float _4137;
            if (_4135)
            {
                float param_var_x_2 = _830;
                float _4136 = interval_up(param_var_x_2, intervalFailed);
                _4137 = _4136;
            }
            else
            {
                _4137 = _830;
            }
            optical_product_upper = _4137;
            bool _4138;
            if (_4134)
            {
                _4138 = _4122;
            }
            else
            {
                _4138 = _4127;
            }
            float _4140;
            if (_4138)
            {
                float param_var_x_3 = _830;
                float _4139 = interval_down(param_var_x_3, intervalFailed);
                _4140 = _4139;
            }
            else
            {
                _4140 = _830;
            }
            return _4140;
        }
    }
    float _843 = spvFMul(a, b);
    float param_var_x_4 = _843;
    float _4143 = interval_up(param_var_x_4, intervalFailed);
    optical_product_upper = _4143;
    float param_var_x_5 = _843;
    float _4144 = interval_down(param_var_x_5, intervalFailed);
    return _4144;
}

static inline __attribute__((always_inline))
Interval imul(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _3599;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3599 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3599 = false;
    }
    bool _3603;
    if (_3599)
    {
        _3603 = a.lo <= a.hi;
    }
    else
    {
        _3603 = false;
    }
    bool _3622;
    if (_3603)
    {
        bool _3617;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3617 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3617 = false;
        }
        bool _3621;
        if (_3617)
        {
            _3621 = b.lo <= b.hi;
        }
        else
        {
            _3621 = false;
        }
        _3622 = _3621;
    }
    else
    {
        _3622 = false;
    }
    if (_3622)
    {
        Interval param_var_a = a;
        float param_var_x = 0.0;
        bool _3628;
        if (!interval_exact_point(param_var_a, param_var_x))
        {
            Interval param_var_a_1 = b;
            float param_var_x_1 = 0.0;
            _3628 = interval_exact_point(param_var_a_1, param_var_x_1);
        }
        else
        {
            _3628 = true;
        }
        if (_3628)
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
    bool _3672;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3672 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3672 = false;
    }
    bool _3676;
    if (_3672)
    {
        _3676 = a.lo <= a.hi;
    }
    else
    {
        _3676 = false;
    }
    bool _3695;
    if (_3676)
    {
        bool _3690;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3690 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3690 = false;
        }
        bool _3694;
        if (_3690)
        {
            _3694 = b.lo <= b.hi;
        }
        else
        {
            _3694 = false;
        }
        _3695 = _3694;
    }
    else
    {
        _3695 = false;
    }
    bool _3703;
    if (_3695)
    {
        _3703 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    else
    {
        _3703 = false;
    }
    bool _3711;
    if (_3695)
    {
        _3711 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    else
    {
        _3711 = false;
    }
    bool _3713;
    if (_3695)
    {
        _3713 = !_3703;
    }
    else
    {
        _3713 = false;
    }
    bool _3715;
    if (_3713)
    {
        _3715 = !_3711;
    }
    else
    {
        _3715 = false;
    }
    uint _3725;
    if (_3715)
    {
        float param_var_alo = a.lo;
        float param_var_ahi = a.hi;
        float param_var_blo = b.lo;
        float param_var_bhi = b.hi;
        _3725 = interval_product_extrema(param_var_alo, param_var_ahi, param_var_blo, param_var_bhi);
    }
    else
    {
        _3725 = 0u;
    }
    if (_3725 != 0u)
    {
        uint _3727 = _3725 >> 2u;
        float _3734;
        if ((_3725 & 2u) != 0u)
        {
            _3734 = a.hi;
        }
        else
        {
            _3734 = a.lo;
        }
        float param_var_a_6 = _3734;
        float _3741;
        if ((_3725 & 1u) != 0u)
        {
            _3741 = b.hi;
        }
        else
        {
            _3741 = b.lo;
        }
        float param_var_b = _3741;
        float _3742 = optical_product_bounds(param_var_a_6, param_var_b, intervalFailed, optical_product_upper);
        float _3749;
        if ((_3727 & 2u) != 0u)
        {
            _3749 = a.hi;
        }
        else
        {
            _3749 = a.lo;
        }
        float param_var_a_7 = _3749;
        float _3756;
        if ((_3727 & 1u) != 0u)
        {
            _3756 = b.hi;
        }
        else
        {
            _3756 = b.lo;
        }
        float param_var_b_1 = _3756;
        __attribute__((unused)) float _3757 = optical_product_bounds(param_var_a_7, param_var_b_1, intervalFailed, optical_product_upper);
        return Interval{ _3742, optical_product_upper };
    }
    float param_var_a_8 = a.lo;
    float param_var_b_2 = b.lo;
    float _3764 = optical_product_bounds(param_var_a_8, param_var_b_2, intervalFailed, optical_product_upper);
    float _3773;
    float _3774;
    if (!_3711)
    {
        float param_var_a_9 = a.lo;
        float param_var_b_3 = b.hi;
        float _3771 = optical_product_bounds(param_var_a_9, param_var_b_3, intervalFailed, optical_product_upper);
        _3773 = optical_product_upper;
        _3774 = _3771;
    }
    else
    {
        _3773 = optical_product_upper;
        _3774 = _3764;
    }
    float _3782;
    float _3783;
    if (!_3703)
    {
        float param_var_a_10 = a.hi;
        float param_var_b_4 = b.lo;
        float _3780 = optical_product_bounds(param_var_a_10, param_var_b_4, intervalFailed, optical_product_upper);
        _3782 = optical_product_upper;
        _3783 = _3780;
    }
    else
    {
        _3782 = optical_product_upper;
        _3783 = _3764;
    }
    float _3793;
    float _3794;
    if (!_3703)
    {
        float _3791;
        float _3792;
        if (_3711)
        {
            _3791 = _3782;
            _3792 = _3783;
        }
        else
        {
            float param_var_a_11 = a.hi;
            float param_var_b_5 = b.hi;
            float _3789 = optical_product_bounds(param_var_a_11, param_var_b_5, intervalFailed, optical_product_upper);
            _3791 = optical_product_upper;
            _3792 = _3789;
        }
        _3793 = _3791;
        _3794 = _3792;
    }
    else
    {
        _3793 = _3773;
        _3794 = _3774;
    }
    return Interval{ precise::min(precise::min(_3764, _3774), precise::min(_3783, _3794)), precise::max(precise::max(optical_product_upper, _3773), precise::max(_3782, _3793)) };
}

static inline __attribute__((always_inline))
float sqrt_bound(thread const float& a, thread const bool& upper, thread bool& intervalFailed)
{
    float _4259;
    _4259 = precise::sqrt(a);
    float _4260;
    for (uint _4261 = 0u; _4261 < 8u; _4259 = _4260, _4261++)
    {
        float _960 = spvFMul(_4259, _4259);
        float _961 = spvFMul(_4259, 4097.0);
        float _963 = spvFSub(_961, spvFSub(_961, _4259));
        float _964 = spvFSub(_4259, _963);
        float _965 = spvFMul(_4259, 4097.0);
        float _967 = spvFSub(_965, spvFSub(_965, _4259));
        float _968 = spvFSub(_4259, _967);
        float _982 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_963, _967), _960), spvFMul(_963, _968)), spvFMul(_964, _967)), spvFMul(_964, _968)), spvFMul(_4259, 0.0)), spvFMul(0.0, _4259)), spvFMul(0.0, 0.0));
        float _983 = spvFAdd(_960, _982);
        float _985 = spvFSub(_982, spvFSub(_983, _960));
        bool _4270;
        if (!(isnan(_983) || isinf(_983)))
        {
            _4270 = isnan(_985) || isinf(_985);
        }
        else
        {
            _4270 = true;
        }
        if (_4270)
        {
            intervalFailed = true;
            return _4259;
        }
        bool _4277;
        if ((isunordered(_983, a) || _983 >= a))
        {
            bool _4276;
            if (_983 == a)
            {
                _4276 = _985 < 0.0;
            }
            else
            {
                _4276 = false;
            }
            _4277 = _4276;
        }
        else
        {
            _4277 = true;
        }
        bool _4284;
        if ((isunordered(a, _983) || a >= _983))
        {
            bool _4283;
            if (a == _983)
            {
                _4283 = 0.0 < _985;
            }
            else
            {
                _4283 = false;
            }
            _4284 = _4283;
        }
        else
        {
            _4284 = true;
        }
        bool _4288;
        if (upper)
        {
            _4288 = !_4277;
        }
        else
        {
            _4288 = !_4284;
        }
        if (_4288)
        {
            return _4259;
        }
        if (upper)
        {
            float param_var_x = _4259;
            float _4290 = interval_up(param_var_x, intervalFailed);
            _4260 = _4290;
        }
        else
        {
            float param_var_x_1 = _4259;
            float _4291 = interval_down(param_var_x_1, intervalFailed);
            _4260 = precise::max(0.0, _4291);
        }
    }
    intervalFailed = true;
    return _4259;
}

static inline __attribute__((always_inline))
Interval isqrt(thread const Interval& a, thread bool& intervalFailed)
{
    bool _4244;
    if ((isunordered(a.hi, 0.0) || a.hi >= 0.0))
    {
        _4244 = a.hi > 1000000015047466219876688855040.0;
    }
    else
    {
        _4244 = true;
    }
    if (_4244)
    {
        intervalFailed = true;
        return Interval{ 0.0, 1000000015047466219876688855040.0 };
    }
    float param_var_a = precise::max(0.0, a.lo);
    bool param_var_upper = false;
    float _4248 = sqrt_bound(param_var_a, param_var_upper, intervalFailed);
    float param_var_x = _4248;
    float _4249 = interval_down(param_var_x, intervalFailed);
    float param_var_a_1 = precise::max(0.0, a.hi);
    bool param_var_upper_1 = true;
    float _4254 = sqrt_bound(param_var_a_1, param_var_upper_1, intervalFailed);
    float param_var_x_1 = _4254;
    float _4255 = interval_up(param_var_x_1, intervalFailed);
    return Interval{ precise::max(0.0, _4249), _4255 };
}

static inline __attribute__((always_inline))
float quotient_bound(thread const float& a, thread const float& b, thread const bool& upper, thread bool& intervalFailed)
{
    float _4191;
    _4191 = a / b;
    float _4192;
    for (uint _4193 = 0u; _4193 < 8u; _4191 = _4192, _4193++)
    {
        bool _4201;
        if (!(isnan(_4191) || isinf(_4191)))
        {
            _4201 = abs(_4191) > 1000000015047466219876688855040.0;
        }
        else
        {
            _4201 = true;
        }
        if (_4201)
        {
            intervalFailed = true;
            return _4191;
        }
        float _934 = spvFMul(_4191, b);
        float _935 = spvFMul(_4191, 4097.0);
        float _937 = spvFSub(_935, spvFSub(_935, _4191));
        float _938 = spvFSub(_4191, _937);
        float _939 = spvFMul(b, 4097.0);
        float _941 = spvFSub(_939, spvFSub(_939, b));
        float _942 = spvFSub(b, _941);
        float _956 = spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFAdd(spvFSub(spvFMul(_937, _941), _934), spvFMul(_937, _942)), spvFMul(_938, _941)), spvFMul(_938, _942)), spvFMul(_4191, 0.0)), spvFMul(0.0, b)), spvFMul(0.0, 0.0));
        float _957 = spvFAdd(_934, _956);
        float _959 = spvFSub(_956, spvFSub(_957, _934));
        bool _4210;
        if (!(isnan(_957) || isinf(_957)))
        {
            _4210 = isnan(_959) || isinf(_959);
        }
        else
        {
            _4210 = true;
        }
        if (_4210)
        {
            intervalFailed = true;
            return _4191;
        }
        bool _4217;
        if ((isunordered(_957, a) || _957 >= a))
        {
            bool _4216;
            if (_957 == a)
            {
                _4216 = _959 < 0.0;
            }
            else
            {
                _4216 = false;
            }
            _4217 = _4216;
        }
        else
        {
            _4217 = true;
        }
        bool _4224;
        if ((isunordered(a, _957) || a >= _957))
        {
            bool _4223;
            if (a == _957)
            {
                _4223 = 0.0 < _959;
            }
            else
            {
                _4223 = false;
            }
            _4224 = _4223;
        }
        else
        {
            _4224 = true;
        }
        if (b < 0.0)
        {
            bool _4230;
            if (upper)
            {
                _4230 = !_4224;
            }
            else
            {
                _4230 = !_4217;
            }
            if (_4230)
            {
                return _4191;
            }
        }
        else
        {
            bool _4234;
            if (upper)
            {
                _4234 = !_4217;
            }
            else
            {
                _4234 = !_4224;
            }
            if (_4234)
            {
                return _4191;
            }
        }
        if (upper)
        {
            float param_var_x = _4191;
            float _4236 = interval_up(param_var_x, intervalFailed);
            _4192 = _4236;
        }
        else
        {
            float param_var_x_1 = _4191;
            float _4237 = interval_down(param_var_x_1, intervalFailed);
            _4192 = _4237;
        }
    }
    intervalFailed = true;
    return _4191;
}

static inline __attribute__((always_inline))
Interval idiv(thread const Interval& a, thread const Interval& b, thread bool& intervalFailed, thread float& interval_divide_upper)
{
    bool _3415;
    if (b.lo <= 0.0)
    {
        _3415 = b.hi >= 0.0;
    }
    else
    {
        _3415 = false;
    }
    bool _3425;
    if (!_3415)
    {
        _3425 = precise::max(abs(b.lo), abs(b.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _3425 = true;
    }
    bool _3435;
    if (!_3425)
    {
        _3435 = precise::max(abs(a.lo), abs(a.hi)) > 1000000015047466219876688855040.0;
    }
    else
    {
        _3435 = true;
    }
    if (_3435)
    {
        intervalFailed = true;
        return Interval{ -1000000015047466219876688855040.0, 1000000015047466219876688855040.0 };
    }
    bool _3449;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3449 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3449 = false;
    }
    bool _3453;
    if (_3449)
    {
        _3453 = a.lo <= a.hi;
    }
    else
    {
        _3453 = false;
    }
    bool _3472;
    if (_3453)
    {
        bool _3467;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3467 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3467 = false;
        }
        bool _3471;
        if (_3467)
        {
            _3471 = b.lo <= b.hi;
        }
        else
        {
            _3471 = false;
        }
        _3472 = _3471;
    }
    else
    {
        _3472 = false;
    }
    if (_3472)
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
    bool _3508;
    if (!(isnan(a.lo) || isinf(a.lo)))
    {
        _3508 = !(isnan(a.hi) || isinf(a.hi));
    }
    else
    {
        _3508 = false;
    }
    bool _3512;
    if (_3508)
    {
        _3512 = a.lo <= a.hi;
    }
    else
    {
        _3512 = false;
    }
    bool _3530;
    if (_3512)
    {
        bool _3525;
        if (!(isnan(b.lo) || isinf(b.lo)))
        {
            _3525 = !(isnan(b.hi) || isinf(b.hi));
        }
        else
        {
            _3525 = false;
        }
        bool _3529;
        if (_3525)
        {
            _3529 = b.lo <= b.hi;
        }
        else
        {
            _3529 = false;
        }
        _3530 = _3529;
    }
    else
    {
        _3530 = false;
    }
    bool _3536;
    if (_3530)
    {
        _3536 = as_type<uint>(a.lo) == as_type<uint>(a.hi);
    }
    else
    {
        _3536 = false;
    }
    bool _3542;
    if (_3530)
    {
        _3542 = as_type<uint>(b.lo) == as_type<uint>(b.hi);
    }
    else
    {
        _3542 = false;
    }
    float _3384 = a.lo;
    float _3385 = b.lo;
    bool _3386 = false;
    float _3543 = quotient_bound(_3384, _3385, _3386, intervalFailed);
    float _3387 = a.lo;
    float _3388 = b.lo;
    bool _3389 = true;
    float _3544 = quotient_bound(_3387, _3388, _3389, intervalFailed);
    float _3550;
    float _3551;
    if (!_3542)
    {
        float _3390 = a.lo;
        float _3391 = b.hi;
        bool _3392 = false;
        float _3548 = quotient_bound(_3390, _3391, _3392, intervalFailed);
        float _3393 = a.lo;
        float _3394 = b.hi;
        bool _3395 = true;
        float _3549 = quotient_bound(_3393, _3394, _3395, intervalFailed);
        _3550 = _3549;
        _3551 = _3548;
    }
    else
    {
        _3550 = _3544;
        _3551 = _3543;
    }
    float _3557;
    float _3558;
    if (!_3536)
    {
        float _3396 = a.hi;
        float _3397 = b.lo;
        bool _3398 = false;
        float _3555 = quotient_bound(_3396, _3397, _3398, intervalFailed);
        float _3399 = a.hi;
        float _3400 = b.lo;
        bool _3401 = true;
        float _3556 = quotient_bound(_3399, _3400, _3401, intervalFailed);
        _3557 = _3556;
        _3558 = _3555;
    }
    else
    {
        _3557 = _3544;
        _3558 = _3543;
    }
    float _3569;
    float _3570;
    if (!_3536)
    {
        float _3567;
        float _3568;
        if (_3542)
        {
            _3567 = _3557;
            _3568 = _3558;
        }
        else
        {
            float _3402 = a.hi;
            float _3403 = b.hi;
            bool _3404 = false;
            float _3565 = quotient_bound(_3402, _3403, _3404, intervalFailed);
            float _3405 = a.hi;
            float _3406 = b.hi;
            bool _3407 = true;
            float _3566 = quotient_bound(_3405, _3406, _3407, intervalFailed);
            _3567 = _3566;
            _3568 = _3565;
        }
        _3569 = _3567;
        _3570 = _3568;
    }
    else
    {
        _3569 = _3550;
        _3570 = _3551;
    }
    interval_divide_upper = precise::max(precise::max(precise::max(precise::max(-1000000015047466219876688855040.0, _3544), _3550), _3557), _3569);
    float param_var_x_3 = precise::min(precise::min(precise::min(precise::min(1000000015047466219876688855040.0, _3543), _3551), _3558), _3570);
    float _3579 = interval_down(param_var_x_3, intervalFailed);
    float param_var_x_4 = interval_divide_upper;
    float _3581 = interval_up(param_var_x_4, intervalFailed);
    return Interval{ _3579, _3581 };
}

static inline __attribute__((always_inline))
Interval3 native_target(thread const uint& at, thread const float4& feature, device type_ByteAddressBuffer& frames, thread bool& intervalFailed, thread float& optical_product_upper, thread float& interval_divide_upper, constant type_TargetSettings& TargetSettings)
{
    if (feature.w != 0.0)
    {
        Interval param_var_a = Interval{ feature.x, feature.x };
        Interval param_var_b = Interval{ feature.y, feature.y };
        Interval _2955 = iadd(param_var_a, param_var_b, intervalFailed);
        Interval _2944 = Interval{ 1.0, 1.0 };
        Interval _2945 = Interval{ as_type<float>(as_type<uint>(_2955.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2955.lo) ^ 2147483648u) };
        Interval _2965 = iadd(_2944, _2945, intervalFailed);
        uint _2967 = (at + 432u) >> 2u;
        uint _2969 = frames._m0[_2967];
        uint _2971 = frames._m0[_2967 + 1u];
        uint _2973 = frames._m0[_2967 + 2u];
        uint _2975 = frames._m0[_2967 + 3u];
        float4 _2977 = as_type<float4>(uint4(_2969, _2971, _2973, _2975));
        float _2978 = _2977.x;
        float _2979 = _2977.y;
        float _2980 = _2977.z;
        Interval _2938 = Interval{ _2978, _2978 };
        Interval _2939 = _2965;
        Interval _2982 = imul(_2938, _2939, intervalFailed, optical_product_upper);
        Interval _2940 = Interval{ _2979, _2979 };
        Interval _2941 = _2965;
        Interval _2984 = imul(_2940, _2941, intervalFailed, optical_product_upper);
        Interval _2942 = Interval{ _2980, _2980 };
        Interval _2943 = _2965;
        Interval _2986 = imul(_2942, _2943, intervalFailed, optical_product_upper);
        uint _2988 = (at + 448u) >> 2u;
        uint _2990 = frames._m0[_2988];
        uint _2992 = frames._m0[_2988 + 1u];
        uint _2994 = frames._m0[_2988 + 2u];
        uint _2996 = frames._m0[_2988 + 3u];
        float4 _2998 = as_type<float4>(uint4(_2990, _2992, _2994, _2996));
        float _2999 = _2998.x;
        float _3000 = _2998.y;
        float _3001 = _2998.z;
        Interval _2932 = Interval{ _2999, _2999 };
        Interval _2933 = Interval{ feature.x, feature.x };
        Interval _3004 = imul(_2932, _2933, intervalFailed, optical_product_upper);
        Interval _2934 = Interval{ _3000, _3000 };
        Interval _2935 = Interval{ feature.x, feature.x };
        Interval _3007 = imul(_2934, _2935, intervalFailed, optical_product_upper);
        Interval _2936 = Interval{ _3001, _3001 };
        Interval _2937 = Interval{ feature.x, feature.x };
        Interval _3010 = imul(_2936, _2937, intervalFailed, optical_product_upper);
        Interval _2926 = _2982;
        Interval _2927 = _3004;
        Interval _3011 = iadd(_2926, _2927, intervalFailed);
        Interval _2928 = _2984;
        Interval _2929 = _3007;
        Interval _3012 = iadd(_2928, _2929, intervalFailed);
        Interval _2930 = _2986;
        Interval _2931 = _3010;
        Interval _3013 = iadd(_2930, _2931, intervalFailed);
        uint _3015 = (at + 464u) >> 2u;
        uint _3017 = frames._m0[_3015];
        uint _3019 = frames._m0[_3015 + 1u];
        uint _3021 = frames._m0[_3015 + 2u];
        uint _3023 = frames._m0[_3015 + 3u];
        float4 _3025 = as_type<float4>(uint4(_3017, _3019, _3021, _3023));
        float _3026 = _3025.x;
        float _3027 = _3025.y;
        float _3028 = _3025.z;
        Interval _2920 = Interval{ _3026, _3026 };
        Interval _2921 = Interval{ feature.y, feature.y };
        Interval _3031 = imul(_2920, _2921, intervalFailed, optical_product_upper);
        Interval _2922 = Interval{ _3027, _3027 };
        Interval _2923 = Interval{ feature.y, feature.y };
        Interval _3034 = imul(_2922, _2923, intervalFailed, optical_product_upper);
        Interval _2924 = Interval{ _3028, _3028 };
        Interval _2925 = Interval{ feature.y, feature.y };
        Interval _3037 = imul(_2924, _2925, intervalFailed, optical_product_upper);
        Interval _2914 = _3011;
        Interval _2915 = _3031;
        Interval _3038 = iadd(_2914, _2915, intervalFailed);
        Interval _2916 = _3012;
        Interval _2917 = _3034;
        Interval _3039 = iadd(_2916, _2917, intervalFailed);
        Interval _2918 = _3013;
        Interval _2919 = _3037;
        Interval _3040 = iadd(_2918, _2919, intervalFailed);
        return Interval3{ _3038, _3039, _3040 };
    }
    Interval _2904 = Interval{ TargetSettings.targetCurrentCube[0].x, TargetSettings.targetCurrentCube[0].x };
    Interval _2905 = Interval{ feature.x, feature.x };
    Interval _3054 = imul(_2904, _2905, intervalFailed, optical_product_upper);
    Interval _2906 = _3054;
    Interval _2907 = Interval{ TargetSettings.targetCurrentCube[0].y, TargetSettings.targetCurrentCube[0].y };
    Interval _2908 = Interval{ feature.y, feature.y };
    Interval _3057 = imul(_2907, _2908, intervalFailed, optical_product_upper);
    Interval _2909 = _3057;
    Interval _3058 = iadd(_2906, _2909, intervalFailed);
    Interval _2910 = _3058;
    Interval _2911 = Interval{ TargetSettings.targetCurrentCube[0].z, TargetSettings.targetCurrentCube[0].z };
    Interval _2912 = Interval{ feature.z, feature.z };
    Interval _3061 = imul(_2911, _2912, intervalFailed, optical_product_upper);
    Interval _2913 = _3061;
    Interval _3062 = iadd(_2910, _2913, intervalFailed);
    Interval _2894 = Interval{ TargetSettings.targetCurrentCube[1].x, TargetSettings.targetCurrentCube[1].x };
    Interval _2895 = Interval{ feature.x, feature.x };
    Interval _3071 = imul(_2894, _2895, intervalFailed, optical_product_upper);
    Interval _2896 = _3071;
    Interval _2897 = Interval{ TargetSettings.targetCurrentCube[1].y, TargetSettings.targetCurrentCube[1].y };
    Interval _2898 = Interval{ feature.y, feature.y };
    Interval _3074 = imul(_2897, _2898, intervalFailed, optical_product_upper);
    Interval _2899 = _3074;
    Interval _3075 = iadd(_2896, _2899, intervalFailed);
    Interval _2900 = _3075;
    Interval _2901 = Interval{ TargetSettings.targetCurrentCube[1].z, TargetSettings.targetCurrentCube[1].z };
    Interval _2902 = Interval{ feature.z, feature.z };
    Interval _3078 = imul(_2901, _2902, intervalFailed, optical_product_upper);
    Interval _2903 = _3078;
    Interval _3079 = iadd(_2900, _2903, intervalFailed);
    Interval _2884 = Interval{ TargetSettings.targetCurrentCube[2].x, TargetSettings.targetCurrentCube[2].x };
    Interval _2885 = Interval{ feature.x, feature.x };
    Interval _3088 = imul(_2884, _2885, intervalFailed, optical_product_upper);
    Interval _2886 = _3088;
    Interval _2887 = Interval{ TargetSettings.targetCurrentCube[2].y, TargetSettings.targetCurrentCube[2].y };
    Interval _2888 = Interval{ feature.y, feature.y };
    Interval _3091 = imul(_2887, _2888, intervalFailed, optical_product_upper);
    Interval _2889 = _3091;
    Interval _3092 = iadd(_2886, _2889, intervalFailed);
    Interval _2890 = _3092;
    Interval _2891 = Interval{ TargetSettings.targetCurrentCube[2].z, TargetSettings.targetCurrentCube[2].z };
    Interval _2892 = Interval{ feature.z, feature.z };
    Interval _3095 = imul(_2891, _2892, intervalFailed, optical_product_upper);
    Interval _2893 = _3095;
    Interval _3096 = iadd(_2890, _2893, intervalFailed);
    Interval _2874 = Interval{ TargetSettings.targetPreviousCube[0].x, TargetSettings.targetPreviousCube[0].x };
    Interval _2875 = _3062;
    Interval _3110 = imul(_2874, _2875, intervalFailed, optical_product_upper);
    Interval _2876 = _3110;
    Interval _2877 = Interval{ TargetSettings.targetPreviousCube[1].x, TargetSettings.targetPreviousCube[1].x };
    Interval _2878 = _3079;
    Interval _3112 = imul(_2877, _2878, intervalFailed, optical_product_upper);
    Interval _2879 = _3112;
    Interval _3113 = iadd(_2876, _2879, intervalFailed);
    Interval _2880 = _3113;
    Interval _2881 = Interval{ TargetSettings.targetPreviousCube[2].x, TargetSettings.targetPreviousCube[2].x };
    Interval _2882 = _3096;
    Interval _3115 = imul(_2881, _2882, intervalFailed, optical_product_upper);
    Interval _2883 = _3115;
    Interval _3116 = iadd(_2880, _2883, intervalFailed);
    Interval _2864 = Interval{ TargetSettings.targetPreviousCube[0].y, TargetSettings.targetPreviousCube[0].y };
    Interval _2865 = _3062;
    Interval _3130 = imul(_2864, _2865, intervalFailed, optical_product_upper);
    Interval _2866 = _3130;
    Interval _2867 = Interval{ TargetSettings.targetPreviousCube[1].y, TargetSettings.targetPreviousCube[1].y };
    Interval _2868 = _3079;
    Interval _3132 = imul(_2867, _2868, intervalFailed, optical_product_upper);
    Interval _2869 = _3132;
    Interval _3133 = iadd(_2866, _2869, intervalFailed);
    Interval _2870 = _3133;
    Interval _2871 = Interval{ TargetSettings.targetPreviousCube[2].y, TargetSettings.targetPreviousCube[2].y };
    Interval _2872 = _3096;
    Interval _3135 = imul(_2871, _2872, intervalFailed, optical_product_upper);
    Interval _2873 = _3135;
    Interval _3136 = iadd(_2870, _2873, intervalFailed);
    Interval _2854 = Interval{ TargetSettings.targetPreviousCube[0].z, TargetSettings.targetPreviousCube[0].z };
    Interval _2855 = _3062;
    Interval _3150 = imul(_2854, _2855, intervalFailed, optical_product_upper);
    Interval _2856 = _3150;
    Interval _2857 = Interval{ TargetSettings.targetPreviousCube[1].z, TargetSettings.targetPreviousCube[1].z };
    Interval _2858 = _3079;
    Interval _3152 = imul(_2857, _2858, intervalFailed, optical_product_upper);
    Interval _2859 = _3152;
    Interval _3153 = iadd(_2856, _2859, intervalFailed);
    Interval _2860 = _3153;
    Interval _2861 = Interval{ TargetSettings.targetPreviousCube[2].z, TargetSettings.targetPreviousCube[2].z };
    Interval _2862 = _3096;
    Interval _3155 = imul(_2861, _2862, intervalFailed, optical_product_upper);
    Interval _2863 = _3155;
    Interval _3156 = iadd(_2860, _2863, intervalFailed);
    Interval _2852 = Interval{ 1.0, 1.0 };
    bool _3163;
    if (_3116.lo <= 0.0)
    {
        _3163 = _3116.hi >= 0.0;
    }
    else
    {
        _3163 = false;
    }
    float _3170;
    if (_3163)
    {
        _3170 = 0.0;
    }
    else
    {
        _3170 = precise::min(abs(_3116.lo), abs(_3116.hi));
    }
    float _3173 = precise::max(abs(_3116.lo), abs(_3116.hi));
    float _2845 = spvFMul(_3170, _3170);
    float _3174 = interval_down(_2845, intervalFailed);
    float _2846 = spvFMul(_3173, _3173);
    float _3176 = interval_up(_2846, intervalFailed);
    Interval _2847 = Interval{ precise::max(0.0, _3174), _3176 };
    bool _3184;
    if (_3136.lo <= 0.0)
    {
        _3184 = _3136.hi >= 0.0;
    }
    else
    {
        _3184 = false;
    }
    float _3191;
    if (_3184)
    {
        _3191 = 0.0;
    }
    else
    {
        _3191 = precise::min(abs(_3136.lo), abs(_3136.hi));
    }
    float _3194 = precise::max(abs(_3136.lo), abs(_3136.hi));
    float _2843 = spvFMul(_3191, _3191);
    float _3195 = interval_down(_2843, intervalFailed);
    float _2844 = spvFMul(_3194, _3194);
    float _3197 = interval_up(_2844, intervalFailed);
    Interval _2848 = Interval{ precise::max(0.0, _3195), _3197 };
    Interval _3199 = iadd(_2847, _2848, intervalFailed);
    Interval _2849 = _3199;
    bool _3206;
    if (_3156.lo <= 0.0)
    {
        _3206 = _3156.hi >= 0.0;
    }
    else
    {
        _3206 = false;
    }
    float _3213;
    if (_3206)
    {
        _3213 = 0.0;
    }
    else
    {
        _3213 = precise::min(abs(_3156.lo), abs(_3156.hi));
    }
    float _3216 = precise::max(abs(_3156.lo), abs(_3156.hi));
    float _2841 = spvFMul(_3213, _3213);
    float _3217 = interval_down(_2841, intervalFailed);
    float _2842 = spvFMul(_3216, _3216);
    float _3219 = interval_up(_2842, intervalFailed);
    Interval _2850 = Interval{ precise::max(0.0, _3217), _3219 };
    Interval _3221 = iadd(_2849, _2850, intervalFailed);
    Interval _2851 = _3221;
    Interval _3222 = isqrt(_2851, intervalFailed);
    Interval _2853 = _3222;
    Interval _3223 = idiv(_2852, _2853, intervalFailed, interval_divide_upper);
    Interval _2835 = _3116;
    Interval _2836 = _3223;
    Interval _3224 = imul(_2835, _2836, intervalFailed, optical_product_upper);
    Interval _2837 = _3136;
    Interval _2838 = _3223;
    Interval _3225 = imul(_2837, _2838, intervalFailed, optical_product_upper);
    Interval _2839 = _3156;
    Interval _2840 = _3223;
    Interval _3226 = imul(_2839, _2840, intervalFailed, optical_product_upper);
    return Interval3{ _3224, _3225, _3226 };
}

static inline __attribute__((always_inline))
bool witness_feature(thread const float3& value, thread const uint& kind, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool3 _3869 = isnan(value);
    bool3 _3870 = isinf(value);
    if (!all(not(bool3(_3869.x || _3870.x, _3869.y || _3870.y, _3869.z || _3870.z))))
    {
        return false;
    }
    if (kind == 1u)
    {
        bool _3884;
        if (value.z == 0.0)
        {
            _3884 = all(value.xy >= float2(0.0));
        }
        else
        {
            _3884 = false;
        }
        bool _3890;
        if (_3884)
        {
            _3890 = spvFAdd(value.x, value.y) <= 1.0;
        }
        else
        {
            _3890 = false;
        }
        return _3890;
    }
    Interval _3858 = Interval{ value.x, value.x };
    Interval _3859 = Interval{ value.x, value.x };
    Interval _3901 = imul(_3858, _3859, intervalFailed, optical_product_upper);
    Interval _3860 = _3901;
    Interval _3861 = Interval{ value.y, value.y };
    Interval _3862 = Interval{ value.y, value.y };
    Interval _3904 = imul(_3861, _3862, intervalFailed, optical_product_upper);
    Interval _3863 = _3904;
    Interval _3905 = iadd(_3860, _3863, intervalFailed);
    Interval _3864 = _3905;
    Interval _3865 = Interval{ value.z, value.z };
    Interval _3866 = Interval{ value.z, value.z };
    Interval _3908 = imul(_3865, _3866, intervalFailed, optical_product_upper);
    Interval _3867 = _3908;
    Interval _3909 = iadd(_3864, _3867, intervalFailed);
    bool _3917;
    if (!intervalFailed)
    {
        bool _3916;
        if (kind != 2u)
        {
            _3916 = kind == 3u;
        }
        else
        {
            _3916 = true;
        }
        _3917 = _3916;
    }
    else
    {
        _3917 = false;
    }
    bool _3930;
    if (_3917)
    {
        Interval _3856 = _3909;
        Interval _3857 = Interval{ as_type<float>(3212836864u), as_type<float>(3212836864u) };
        Interval _3921 = iadd(_3856, _3857, intervalFailed);
        _3930 = precise::max(abs(_3921.lo), abs(_3921.hi)) < 9.9999997473787516355514526367188e-05;
    }
    else
    {
        _3930 = false;
    }
    return _3930;
}

static inline __attribute__((always_inline))
WitnessTap witness_tap(thread const int2& pixel, thread const WitnessQuery& q, constant type_Settings& Settings, device type_ByteAddressBuffer& source, thread bool& intervalFailed, thread float& optical_product_upper)
{
    bool _3242;
    if (!any(pixel < int2(0)))
    {
        _3242 = any(pixel >= int2(int(Settings.width), int(Settings.height)));
    }
    else
    {
        _3242 = true;
    }
    if (_3242)
    {
        return WitnessTap{ 0.0, float3(0.0), short(false), short(false) };
    }
    uint _797 = (uint(pixel.y) * Settings.width) + uint(pixel.x);
    uint _798 = Settings.width * Settings.height;
    if (source._m0[((_798 * Settings.primaryPrefix) + (_797 * 4u)) >> 2u] != q.primary)
    {
        return WitnessTap{ 0.0, float3(0.0), short(false), short(false) };
    }
    uint _806 = (_798 * Settings.recordPrefix) + (((_797 * Settings.lobes) + q.lobe) * Settings.pathStride);
    uint _3271 = _806 >> 2u;
    bool _3292;
    if (!any(uint4(source._m0[_3271], source._m0[_3271 + 1u], source._m0[_3271 + 2u], source._m0[_3271 + 3u]) != q.mirrors))
    {
        _3292 = source._m0[(_806 + 16u) >> 2u] != q.control;
    }
    else
    {
        _3292 = true;
    }
    bool _3300;
    if (!_3292)
    {
        _3300 = source._m0[(_806 + 20u) >> 2u] != q.terminal;
    }
    else
    {
        _3300 = true;
    }
    if (_3300)
    {
        return WitnessTap{ 0.0, float3(0.0), short(false), short(false) };
    }
    uint _3301 = (_806 + 24u) >> 2u;
    uint _3303 = source._m0[_3301];
    uint _3305 = source._m0[_3301 + 1u];
    uint _3307 = source._m0[_3301 + 2u];
    float3 _3309 = as_type<float3>(uint3(_3303, _3305, _3307));
    uint _3310 = (_806 + 40u) >> 2u;
    uint _3312 = source._m0[_3310];
    uint _3314 = source._m0[_3310 + 1u];
    uint _3316 = source._m0[_3310 + 2u];
    float3 _3318 = as_type<float3>(uint3(_3312, _3314, _3316));
    float3 param_var_value = _3309;
    uint param_var_kind = q.control >> 8u;
    bool _3322 = witness_feature(param_var_value, param_var_kind, intervalFailed, optical_product_upper);
    bool _3328;
    if (_3322)
    {
        _3328 = (source._m0[(_806 + 36u) >> 2u] >> 24u) != 255u;
    }
    else
    {
        _3328 = true;
    }
    bool _3336;
    if (!_3328)
    {
        bool3 _3330 = isnan(_3318);
        bool3 _3331 = isinf(_3318);
        _3336 = !all(not(bool3(_3330.x || _3331.x, _3330.y || _3331.y, _3330.z || _3331.z)));
    }
    else
    {
        _3336 = true;
    }
    bool _3340;
    if (!_3336)
    {
        _3340 = any(_3318 < float3(0.0));
    }
    else
    {
        _3340 = true;
    }
    bool _3344;
    if (!_3340)
    {
        _3344 = any(_3318 > float3(1.0));
    }
    else
    {
        _3344 = true;
    }
    if (_3344)
    {
        return WitnessTap{ 0.0, _3309, short(false), short(false) };
    }
    if (Settings.pathStride == 64u)
    {
        uint _3349 = (_806 + 52u) >> 2u;
        float3 _3357 = as_type<float3>(uint3(source._m0[_3349], source._m0[_3349 + 1u], source._m0[_3349 + 2u]));
        bool3 _3358 = isnan(_3357);
        bool3 _3359 = isinf(_3357);
        bool _3365;
        if (all(not(bool3(_3358.x || _3359.x, _3358.y || _3359.y, _3358.z || _3359.z))))
        {
            _3365 = any(_3357 < float3(0.0));
        }
        else
        {
            _3365 = true;
        }
        bool _3369;
        if (!_3365)
        {
            _3369 = any(_3357 > float3(999999995904.0));
        }
        else
        {
            _3369 = true;
        }
        if (_3369)
        {
            return WitnessTap{ 0.0, _3309, short(false), short(false) };
        }
    }
    float _3376 = as_type<float>(source._m0[((_798 * (Settings.primaryPrefix + 4u)) + (_797 * 4u)) >> 2u]);
    bool _3382;
    if (!(isnan(_3376) || isinf(_3376)))
    {
        _3382 = _3376 > 0.0;
    }
    else
    {
        _3382 = false;
    }
    return WitnessTap{ _3376, _3309, short(true), short(_3382) };
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
    bool _1334;
    if (id.x < Settings.queryCount)
    {
        _1334 = id.x >= 64u;
    }
    else
    {
        _1334 = true;
    }
    if (_1334)
    {
        return;
    }
    uint _711 = id.x * 24576u;
    uint _712 = _711 + 192u;
    uint _713 = id.x * 512u;
    for (uint _1335 = 0u; _1335 < 4u; _1335++)
    {
        uint _1337 = (_712 + (_1335 * 16u)) >> 2u;
        results._m0[_1337] = 0u;
        results._m0[_1337 + 1u] = 0u;
        results._m0[_1337 + 2u] = 0u;
        results._m0[_1337 + 3u] = 0u;
    }
    intervalFailed = false;
    bool _1348;
    if (Settings.queryCount <= 64u)
    {
        _1348 = WitnessSettings.witnessStride != 24576u;
    }
    else
    {
        _1348 = true;
    }
    bool _1353;
    if (!_1348)
    {
        _1353 = WitnessSettings.witnessOffset != 192u;
    }
    else
    {
        _1353 = true;
    }
    bool _1358;
    if (!_1353)
    {
        _1358 = WitnessSettings.witnessBytes != 64u;
    }
    else
    {
        _1358 = true;
    }
    bool _1363;
    if (!_1358)
    {
        _1363 = WitnessSettings.witnessReserved != 0u;
    }
    else
    {
        _1363 = true;
    }
    bool _1372;
    if (!_1363)
    {
        bool _1371;
        if (Settings.pathStride != 52u)
        {
            _1371 = Settings.pathStride != 64u;
        }
        else
        {
            _1371 = false;
        }
        _1372 = _1371;
    }
    else
    {
        _1372 = true;
    }
    bool _1377;
    if (!_1372)
    {
        _1377 = Settings.width == 0u;
    }
    else
    {
        _1377 = true;
    }
    bool _1382;
    if (!_1377)
    {
        _1382 = Settings.height == 0u;
    }
    else
    {
        _1382 = true;
    }
    bool _1387;
    if (!_1382)
    {
        _1387 = Settings.width > 16384u;
    }
    else
    {
        _1387 = true;
    }
    bool _1392;
    if (!_1387)
    {
        _1392 = Settings.height > 16384u;
    }
    else
    {
        _1392 = true;
    }
    bool _1401;
    if (!_1392)
    {
        bool _1400;
        if (Settings.lobes != 1u)
        {
            _1400 = Settings.lobes != 8u;
        }
        else
        {
            _1400 = false;
        }
        _1401 = _1400;
    }
    else
    {
        _1401 = true;
    }
    bool _1415;
    if (!_1401)
    {
        uint _1403 = (_713 + 496u) >> 2u;
        _1415 = any(uint4(frames._m0[_1403], frames._m0[_1403 + 1u], frames._m0[_1403 + 2u], frames._m0[_1403 + 3u]) != uint4(1u, 0u, 0u, 0u));
    }
    else
    {
        _1415 = true;
    }
    if (_1415)
    {
        uint _1416 = _712 >> 2u;
        uint _1421;
        if (intervalFailed)
        {
            _1421 = 6u;
        }
        else
        {
            _1421 = 8u;
        }
        results._m0[_1416] = 0u;
        results._m0[_1416 + 1u] = 0u;
        results._m0[_1416 + 2u] = _1421;
        results._m0[_1416 + 3u] = 0u;
        return;
    }
    uint _1426 = _711 >> 2u;
    if (any(uint4(results._m0[_1426], results._m0[_1426 + 1u], results._m0[_1426 + 2u], results._m0[_1426 + 3u]) != uint4(1u, 1u, results._m0[(_711 + 8u) >> 2u], 0u)))
    {
        uint _1442 = _712 >> 2u;
        uint _1447;
        if (intervalFailed)
        {
            _1447 = 6u;
        }
        else
        {
            _1447 = 1u;
        }
        results._m0[_1442] = 0u;
        results._m0[_1442 + 1u] = 0u;
        results._m0[_1442 + 2u] = _1447;
        results._m0[_1442 + 3u] = 0u;
        return;
    }
    uint _1452 = (_711 + 144u) >> 2u;
    uint _1454 = results._m0[_1452];
    uint _729 = _1452 + 1u;
    uint _1456 = results._m0[_729];
    uint _730 = _1452 + 2u;
    uint _1458 = results._m0[_730];
    uint _731 = _1452 + 3u;
    uint _1460 = results._m0[_731];
    float4 _1462 = as_type<float4>(uint4(_1454, _1456, _1458, _1460));
    uint _1463 = (_711 + 208u) >> 2u;
    results._m0[_1463] = _1454;
    results._m0[_1463 + 1u] = _1456;
    results._m0[_1463 + 2u] = _1458;
    results._m0[_1463 + 3u] = _1460;
    bool4 _1468 = isnan(_1462);
    bool4 _1469 = isinf(_1462);
    bool _1477;
    if (all(not(bool4(_1468.x || _1469.x, _1468.y || _1469.y, _1468.z || _1469.z, _1468.w || _1469.w))))
    {
        _1477 = any(_1462.xy > _1462.zw);
    }
    else
    {
        _1477 = true;
    }
    bool _1482;
    if (!_1477)
    {
        _1482 = any(_1462.xy < float2(0.5));
    }
    else
    {
        _1482 = true;
    }
    bool _1494;
    if (!_1482)
    {
        _1494 = any(_1462.zw > spvFSub(float2(float(Settings.width), float(Settings.height)), float2(0.5)));
    }
    else
    {
        _1494 = true;
    }
    if (_1494)
    {
        uint _1495 = _712 >> 2u;
        uint _1500;
        if (intervalFailed)
        {
            _1500 = 6u;
        }
        else
        {
            _1500 = 2u;
        }
        results._m0[_1495] = 0u;
        results._m0[_1495 + 1u] = 1u;
        results._m0[_1495 + 2u] = _1500;
        results._m0[_1495 + 3u] = 0u;
        return;
    }
    uint _1505 = (id.x * 48u) >> 2u;
    uint _1507 = queries._m0[_1505];
    uint _1508 = ((id.x * 48u) + 4u) >> 2u;
    uint _1510 = queries._m0[_1508];
    uint _1512 = queries._m0[_1508 + 1u];
    uint _1514 = queries._m0[_1508 + 2u];
    uint _1516 = queries._m0[_1508 + 3u];
    uint4 _1517 = uint4(_1510, _1512, _1514, _1516);
    uint _1518 = ((id.x * 48u) + 20u) >> 2u;
    uint _1520 = queries._m0[_1518];
    uint _1521 = ((id.x * 48u) + 24u) >> 2u;
    uint _1523 = queries._m0[_1521];
    uint _1524 = ((id.x * 48u) + 44u) >> 2u;
    uint _1526 = queries._m0[_1524];
    bool _1531;
    if (_1507 != 4294967295u)
    {
        _1531 = _1526 >= Settings.lobes;
    }
    else
    {
        _1531 = true;
    }
    bool _1534;
    if (!_1531)
    {
        _1534 = _1526 > 7u;
    }
    else
    {
        _1534 = true;
    }
    if (_1534)
    {
        uint _1535 = _712 >> 2u;
        uint _1540;
        if (intervalFailed)
        {
            _1540 = 6u;
        }
        else
        {
            _1540 = 8u;
        }
        results._m0[_1535] = 0u;
        results._m0[_1535 + 1u] = 1u;
        results._m0[_1535 + 2u] = _1540;
        results._m0[_1535 + 3u] = 0u;
        return;
    }
    uint _1545 = (_713 + 416u) >> 2u;
    float4 _1555 = as_type<float4>(uint4(frames._m0[_1545], frames._m0[_1545 + 1u], frames._m0[_1545 + 2u], frames._m0[_1545 + 3u]));
    float _1571;
    float _1572;
    float _1573;
    float _1574;
    float _1575;
    float _1576;
    if (_1555.w != 0.0)
    {
        float _1558 = _1555.x;
        float _1559 = _1555.y;
        float _1560 = _1555.z;
        _1571 = _1558;
        _1572 = _1558;
        _1573 = _1559;
        _1574 = _1559;
        _1575 = _1560;
        _1576 = _1560;
    }
    else
    {
        uint param_var_at = _713;
        float4 param_var_feature = _1555;
        Interval3 _1561 = native_target(param_var_at, param_var_feature, frames, intervalFailed, optical_product_upper, interval_divide_upper, TargetSettings);
        _1571 = _1561.x.lo;
        _1572 = _1561.x.hi;
        _1573 = _1561.y.lo;
        _1574 = _1561.y.hi;
        _1575 = _1561.z.lo;
        _1576 = _1561.z.hi;
    }
    uint _1577 = (_713 + 48u) >> 2u;
    uint _1579 = frames._m0[_1577];
    uint _1581 = frames._m0[_1577 + 1u];
    uint _1583 = frames._m0[_1577 + 2u];
    uint _1585 = frames._m0[_1577 + 3u];
    float4 _1587 = as_type<float4>(uint4(_1579, _1581, _1583, _1585));
    float _1779;
    float _1780;
    float _1781;
    float _1782;
    float _1783;
    float _1784;
    float _1785;
    float _1786;
    float _1787;
    float _1788;
    float _1789;
    float _1790;
    if (frames._m0[(_713 + 488u) >> 2u] != 0u)
    {
        uint _1595 = (_713 + 288u) >> 2u;
        float4 _1605 = as_type<float4>(uint4(frames._m0[_1595], frames._m0[_1595 + 1u], frames._m0[_1595 + 2u], frames._m0[_1595 + 3u]));
        float _1606 = _1605.x;
        float _1607 = _1605.y;
        float _1608 = _1605.z;
        uint _1609 = (_713 + 304u) >> 2u;
        float4 _1619 = as_type<float4>(uint4(frames._m0[_1609], frames._m0[_1609 + 1u], frames._m0[_1609 + 2u], frames._m0[_1609 + 3u]));
        float _1620 = _1619.x;
        float _1621 = _1619.y;
        float _1622 = _1619.z;
        _1779 = _1620;
        _1780 = _1620;
        _1781 = _1621;
        _1782 = _1621;
        _1783 = _1622;
        _1784 = _1622;
        _1785 = _1606;
        _1786 = _1606;
        _1787 = _1607;
        _1788 = _1607;
        _1789 = _1608;
        _1790 = _1608;
    }
    else
    {
        uint _1623 = _713 >> 2u;
        uint _1625 = frames._m0[_1623];
        uint _1627 = frames._m0[_1623 + 1u];
        uint _1629 = frames._m0[_1623 + 2u];
        uint _1631 = frames._m0[_1623 + 3u];
        float4 _1633 = as_type<float4>(uint4(_1625, _1627, _1629, _1631));
        uint _1634 = (_713 + 16u) >> 2u;
        uint _1636 = frames._m0[_1634];
        uint _1638 = frames._m0[_1634 + 1u];
        uint _1640 = frames._m0[_1634 + 2u];
        uint _1642 = frames._m0[_1634 + 3u];
        float4 _1644 = as_type<float4>(uint4(_1636, _1638, _1640, _1642));
        uint _1645 = (_713 + 32u) >> 2u;
        uint _1647 = frames._m0[_1645];
        uint _1649 = frames._m0[_1645 + 1u];
        uint _1651 = frames._m0[_1645 + 2u];
        uint _1653 = frames._m0[_1645 + 3u];
        float4 _1655 = as_type<float4>(uint4(_1647, _1649, _1651, _1653));
        float _1656 = _1633.x;
        float _1657 = _1633.y;
        float _1658 = _1633.z;
        float _1773;
        float _1774;
        float _1775;
        float _1776;
        float _1777;
        float _1778;
        if (all(float3(_1633.w, _1644.w, _1655.w) == float3(2.0)))
        {
            float _1668 = _1644.x;
            float _1669 = _1644.y;
            float _1670 = _1644.z;
            _1773 = _1668;
            _1774 = _1668;
            _1775 = _1669;
            _1776 = _1669;
            _1777 = _1670;
            _1778 = _1670;
        }
        else
        {
            float _1671 = _1644.x;
            float _1672 = _1644.y;
            float _1673 = _1644.z;
            Interval _1316 = Interval{ _1671, _1671 };
            Interval _1317 = Interval{ as_type<float>(as_type<uint>(_1656) ^ 2147483648u), as_type<float>(as_type<uint>(_1656) ^ 2147483648u) };
            Interval _1694 = iadd(_1316, _1317, intervalFailed);
            Interval _1318 = Interval{ _1672, _1672 };
            Interval _1319 = Interval{ as_type<float>(as_type<uint>(_1657) ^ 2147483648u), as_type<float>(as_type<uint>(_1657) ^ 2147483648u) };
            Interval _1697 = iadd(_1318, _1319, intervalFailed);
            Interval _1320 = Interval{ _1673, _1673 };
            Interval _1321 = Interval{ as_type<float>(as_type<uint>(_1658) ^ 2147483648u), as_type<float>(as_type<uint>(_1658) ^ 2147483648u) };
            Interval _1700 = iadd(_1320, _1321, intervalFailed);
            float _1701 = _1655.x;
            float _1702 = _1655.y;
            float _1703 = _1655.z;
            Interval _1310 = Interval{ _1701, _1701 };
            Interval _1311 = Interval{ as_type<float>(as_type<uint>(_1656) ^ 2147483648u), as_type<float>(as_type<uint>(_1656) ^ 2147483648u) };
            Interval _1724 = iadd(_1310, _1311, intervalFailed);
            Interval _1312 = Interval{ _1702, _1702 };
            Interval _1313 = Interval{ as_type<float>(as_type<uint>(_1657) ^ 2147483648u), as_type<float>(as_type<uint>(_1657) ^ 2147483648u) };
            Interval _1727 = iadd(_1312, _1313, intervalFailed);
            Interval _1314 = Interval{ _1703, _1703 };
            Interval _1315 = Interval{ as_type<float>(as_type<uint>(_1658) ^ 2147483648u), as_type<float>(as_type<uint>(_1658) ^ 2147483648u) };
            Interval _1730 = iadd(_1314, _1315, intervalFailed);
            Interval _1298 = _1697;
            Interval _1299 = _1730;
            Interval _1731 = imul(_1298, _1299, intervalFailed, optical_product_upper);
            Interval _1300 = _1700;
            Interval _1301 = _1727;
            Interval _1732 = imul(_1300, _1301, intervalFailed, optical_product_upper);
            Interval _1296 = _1731;
            Interval _1297 = Interval{ as_type<float>(as_type<uint>(_1732.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_1732.lo) ^ 2147483648u) };
            Interval _1742 = iadd(_1296, _1297, intervalFailed);
            Interval _1302 = _1700;
            Interval _1303 = _1724;
            Interval _1743 = imul(_1302, _1303, intervalFailed, optical_product_upper);
            Interval _1304 = _1694;
            Interval _1305 = _1730;
            Interval _1744 = imul(_1304, _1305, intervalFailed, optical_product_upper);
            Interval _1294 = _1743;
            Interval _1295 = Interval{ as_type<float>(as_type<uint>(_1744.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_1744.lo) ^ 2147483648u) };
            Interval _1754 = iadd(_1294, _1295, intervalFailed);
            Interval _1306 = _1694;
            Interval _1307 = _1727;
            Interval _1755 = imul(_1306, _1307, intervalFailed, optical_product_upper);
            Interval _1308 = _1697;
            Interval _1309 = _1724;
            Interval _1756 = imul(_1308, _1309, intervalFailed, optical_product_upper);
            Interval _1292 = _1755;
            Interval _1293 = Interval{ as_type<float>(as_type<uint>(_1756.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_1756.lo) ^ 2147483648u) };
            Interval _1766 = iadd(_1292, _1293, intervalFailed);
            _1773 = _1742.lo;
            _1774 = _1742.hi;
            _1775 = _1754.lo;
            _1776 = _1754.hi;
            _1777 = _1766.lo;
            _1778 = _1766.hi;
        }
        _1779 = _1773;
        _1780 = _1774;
        _1781 = _1775;
        _1782 = _1776;
        _1783 = _1777;
        _1784 = _1778;
        _1785 = _1656;
        _1786 = _1656;
        _1787 = _1657;
        _1788 = _1657;
        _1789 = _1658;
        _1790 = _1658;
    }
    float _1793 = _1587.z;
    Interval _1290 = Interval{ _1462.x, _1462.z };
    Interval _1291 = Interval{ as_type<float>(as_type<uint>(_1793) ^ 2147483648u), as_type<float>(as_type<uint>(_1793) ^ 2147483648u) };
    Interval _1802 = iadd(_1290, _1291, intervalFailed);
    Interval _1322 = _1802;
    float _1803 = _1587.x;
    Interval _1323 = Interval{ _1803, _1803 };
    Interval _1805 = idiv(_1322, _1323, intervalFailed, interval_divide_upper);
    float _1808 = _1587.w;
    Interval _1288 = Interval{ _1462.y, _1462.w };
    Interval _1289 = Interval{ as_type<float>(as_type<uint>(_1808) ^ 2147483648u), as_type<float>(as_type<uint>(_1808) ^ 2147483648u) };
    Interval _1817 = iadd(_1288, _1289, intervalFailed);
    Interval _1324 = _1817;
    float _1818 = _1587.y;
    Interval _1325 = Interval{ _1818, _1818 };
    Interval _1820 = idiv(_1324, _1325, intervalFailed, interval_divide_upper);
    Interval _1278 = Interval{ _1785, _1786 };
    Interval _1279 = Interval{ _1779, _1780 };
    Interval _1823 = imul(_1278, _1279, intervalFailed, optical_product_upper);
    Interval _1280 = _1823;
    Interval _1281 = Interval{ _1787, _1788 };
    Interval _1282 = Interval{ _1781, _1782 };
    Interval _1826 = imul(_1281, _1282, intervalFailed, optical_product_upper);
    Interval _1283 = _1826;
    Interval _1827 = iadd(_1280, _1283, intervalFailed);
    Interval _1284 = _1827;
    Interval _1285 = Interval{ _1789, _1790 };
    Interval _1286 = Interval{ _1783, _1784 };
    Interval _1830 = imul(_1285, _1286, intervalFailed, optical_product_upper);
    Interval _1287 = _1830;
    Interval _1831 = iadd(_1284, _1287, intervalFailed);
    Interval _1326 = _1831;
    Interval _1268 = _1805;
    Interval _1269 = Interval{ _1779, _1780 };
    Interval _1833 = imul(_1268, _1269, intervalFailed, optical_product_upper);
    Interval _1270 = _1833;
    Interval _1271 = _1820;
    Interval _1272 = Interval{ _1781, _1782 };
    Interval _1835 = imul(_1271, _1272, intervalFailed, optical_product_upper);
    Interval _1273 = _1835;
    Interval _1836 = iadd(_1270, _1273, intervalFailed);
    Interval _1274 = _1836;
    Interval _1275 = Interval{ 1.0, 1.0 };
    Interval _1276 = Interval{ _1783, _1784 };
    Interval _1838 = imul(_1275, _1276, intervalFailed, optical_product_upper);
    Interval _1277 = _1838;
    Interval _1839 = iadd(_1274, _1277, intervalFailed);
    Interval _1327 = _1839;
    Interval _1840 = idiv(_1326, _1327, intervalFailed, interval_divide_upper);
    uint _1843 = (_713 + 64u) >> 2u;
    float4 _1853 = as_type<float4>(uint4(frames._m0[_1843], frames._m0[_1843 + 1u], frames._m0[_1843 + 2u], frames._m0[_1843 + 3u]));
    bool _1857;
    if (!intervalFailed)
    {
        _1857 = _1840.lo <= 0.0;
    }
    else
    {
        _1857 = true;
    }
    bool _1861;
    if (!_1857)
    {
        _1861 = _1840.lo < _1853.z;
    }
    else
    {
        _1861 = true;
    }
    bool _1865;
    if (!_1861)
    {
        _1865 = _1840.hi > _1853.w;
    }
    else
    {
        _1865 = true;
    }
    if (_1865)
    {
        uint _1866 = _712 >> 2u;
        uint _1871;
        if (intervalFailed)
        {
            _1871 = 6u;
        }
        else
        {
            _1871 = 4u;
        }
        results._m0[_1866] = 0u;
        results._m0[_1866 + 1u] = 3u;
        results._m0[_1866 + 2u] = _1871;
        results._m0[_1866 + 3u] = 0u;
        return;
    }
    uint _1876 = (_711 + 224u) >> 2u;
    uint2 _1878 = as_type<uint2>(float2(_1840.lo, _1840.hi));
    results._m0[_1876] = _1878.x;
    results._m0[_1876 + 1u] = _1878.y;
    Interval _1266 = Interval{ _1462.x, _1462.z };
    Interval _1267 = Interval{ as_type<float>(3204448256u), as_type<float>(3204448256u) };
    Interval _1889 = iadd(_1266, _1267, intervalFailed);
    Interval _1264 = Interval{ _1462.y, _1462.w };
    Interval _1265 = Interval{ as_type<float>(3204448256u), as_type<float>(3204448256u) };
    Interval _1898 = iadd(_1264, _1265, intervalFailed);
    int2 _1903 = int2(floor(float2(_1889.lo, _1898.lo)));
    int2 _1906 = int2(floor(float2(_1889.hi, _1898.hi)));
    bool _1919;
    if (!any(_1903 < int2(0)))
    {
        _1919 = any(_1906 >= int2(int(Settings.width), int(Settings.height)));
    }
    else
    {
        _1919 = true;
    }
    if (_1919)
    {
        uint _1920 = _712 >> 2u;
        uint _1925;
        if (intervalFailed)
        {
            _1925 = 6u;
        }
        else
        {
            _1925 = 2u;
        }
        results._m0[_1920] = 0u;
        results._m0[_1920 + 1u] = 3u;
        results._m0[_1920 + 2u] = _1925;
        results._m0[_1920 + 3u] = 0u;
        return;
    }
    if (any((_1906 - _1903) > int2(2)))
    {
        uint _1932 = _712 >> 2u;
        uint _1937;
        if (intervalFailed)
        {
            _1937 = 6u;
        }
        else
        {
            _1937 = 7u;
        }
        results._m0[_1932] = 0u;
        results._m0[_1932 + 1u] = 3u;
        results._m0[_1932 + 2u] = _1937;
        results._m0[_1932 + 3u] = 0u;
        return;
    }
    int _1942 = _1903.y;
    uint _1943;
    _1943 = 0u;
    spvUnsafeArray<Interval, 4> depths;
    uint _1944;
    for (int _1945 = _1942; _1945 <= _1906.y; _1943 = _1944, _1945++)
    {
        int _1948 = _1903.x;
        _1944 = _1943;
        uint _778;
        for (int _1949 = _1948; _1949 <= _1906.x; _1944 = _778, _1949++)
        {
            float _1953 = precise::max(_1889.lo, float(_1949));
            float _1955 = precise::min(_1889.hi, float(_1949 + 1));
            float _1957 = precise::max(_1898.lo, float(_1945));
            float _1959 = precise::min(_1898.hi, float(_1945 + 1));
            float _1960 = float(_1949);
            Interval _1262 = Interval{ _1953, _1955 };
            Interval _1263 = Interval{ as_type<float>(as_type<uint>(_1960) ^ 2147483648u), as_type<float>(as_type<uint>(_1960) ^ 2147483648u) };
            Interval _1969 = iadd(_1262, _1263, intervalFailed);
            float _1974 = precise::min(precise::max(_1969.lo, 0.0), 1.0);
            float _1975 = precise::min(precise::max(_1969.hi, 0.0), 1.0);
            float _1976 = float(_1945);
            Interval _1260 = Interval{ _1957, _1959 };
            Interval _1261 = Interval{ as_type<float>(as_type<uint>(_1976) ^ 2147483648u), as_type<float>(as_type<uint>(_1976) ^ 2147483648u) };
            Interval _1985 = iadd(_1260, _1261, intervalFailed);
            float _1990 = precise::min(precise::max(_1985.lo, 0.0), 1.0);
            float _1991 = precise::min(precise::max(_1985.hi, 0.0), 1.0);
            float _1992;
            float _1994;
            bool _1996;
            _1992 = 0.0;
            _1994 = 0.0;
            _1996 = true;
            float _1993;
            float _1995;
            bool _1997;
            for (uint _1998 = 0u; _1998 < 2u; _1992 = _1993, _1994 = _1995, _1996 = _1997, _1998++)
            {
                _1993 = _1992;
                _1995 = _1994;
                _1997 = _1996;
                float _2000;
                float _2001;
                bool _2002;
                for (uint _2003 = 0u; _2003 < 2u; _1993 = _2000, _1995 = _2001, _1997 = _2002, _2003++)
                {
                    uint _763 = (_1998 * 2u) + _2003;
                    depths[_763] = Interval{ 0.0, 0.0 };
                    int2 _2017 = min((int2(_1949, _1945) + int2(int(_2003), int(_1998))), int2(int(Settings.width - 1u), int(Settings.height - 1u)));
                    int2 pixel = _2017;
                    int2 param_var_pixel = _2017;
                    WitnessQuery param_var_q = WitnessQuery{ _1507, _1517, _1520, _1523, _1526 };
                    WitnessTap _2019 = witness_tap(param_var_pixel, param_var_q, Settings, source, intervalFailed, optical_product_upper);
                    if (_1997)
                    {
                        _2002 = _2019.visible;
                    }
                    else
                    {
                        _2002 = false;
                    }
                    if (_2019.visible)
                    {
                        Interval param_var_a = Interval{ 1.0, 1.0 };
                        Interval param_var_b = Interval{ _2019.depth, _2019.depth };
                        Interval _2024 = idiv(param_var_a, param_var_b, intervalFailed, interval_divide_upper);
                        depths[_763] = _2024;
                    }
                    bool param_var_pick = _2003 != 0u;
                    Interval param_var_a_1 = Interval{ _1974, _1975 };
                    Interval _1258 = Interval{ 1.0, 1.0 };
                    Interval _1259 = Interval{ as_type<float>(as_type<uint>(_1975) ^ 2147483648u), as_type<float>(as_type<uint>(_1974) ^ 2147483648u) };
                    Interval _2035 = iadd(_1258, _1259, intervalFailed);
                    Interval param_var_b_1 = _2035;
                    Interval param_var_a_2 = witness_select(param_var_pick, param_var_a_1, param_var_b_1);
                    bool param_var_pick_1 = _1998 != 0u;
                    Interval param_var_a_3 = Interval{ _1990, _1991 };
                    Interval _1256 = Interval{ 1.0, 1.0 };
                    Interval _1257 = Interval{ as_type<float>(as_type<uint>(_1991) ^ 2147483648u), as_type<float>(as_type<uint>(_1990) ^ 2147483648u) };
                    Interval _2046 = iadd(_1256, _1257, intervalFailed);
                    Interval param_var_b_2 = _2046;
                    Interval param_var_b_3 = witness_select(param_var_pick_1, param_var_a_3, param_var_b_2);
                    Interval _2048 = imul(param_var_a_2, param_var_b_3, intervalFailed, optical_product_upper);
                    if (_2048.hi <= 0.0)
                    {
                        _2000 = _1993;
                        _2001 = _1995;
                        continue;
                    }
                    if (!_2019.visible)
                    {
                        uint _2052 = _712 >> 2u;
                        uint _2057;
                        if (intervalFailed)
                        {
                            _2057 = 6u;
                        }
                        else
                        {
                            _2057 = 3u;
                        }
                        results._m0[_2052] = 0u;
                        results._m0[_2052 + 1u] = 3u;
                        results._m0[_2052 + 2u] = _2057;
                        results._m0[_2052 + 3u] = _1944;
                        return;
                    }
                    Interval param_var_a_4 = Interval{ _1993, _1995 };
                    Interval param_var_a_5 = _2048;
                    Interval param_var_b_4 = depths[_763];
                    Interval _2065 = imul(param_var_a_5, param_var_b_4, intervalFailed, optical_product_upper);
                    Interval param_var_b_5 = _2065;
                    Interval _2066 = iadd(param_var_a_4, param_var_b_5, intervalFailed);
                    float _2075;
                    float _2077;
                    float _2079;
                    _2075 = 1.9999999949504854157567024230957e-06;
                    _2077 = 1.9999999949504854157567024230957e-06;
                    _2079 = 1.9999999949504854157567024230957e-06;
                    float _2070;
                    float _2072;
                    float _2074;
                    float _2076;
                    float _2078;
                    float _2080;
                    float _2069 = 1.9999999949504854157567024230957e-06;
                    float _2071 = 1.9999999949504854157567024230957e-06;
                    float _2073 = 1.9999999949504854157567024230957e-06;
                    uint _2081 = 0u;
                    for (; _2081 < 2u; _2069 = _2070, _2071 = _2072, _2073 = _2074, _2075 = _2076, _2077 = _2078, _2079 = _2080, _2081++)
                    {
                        float _2083;
                        float _2085;
                        float _2087;
                        float _2089;
                        float _2091;
                        float _2093;
                        _2083 = 0.0;
                        _2085 = 0.0;
                        _2087 = 0.0;
                        _2089 = 0.0;
                        _2091 = 0.0;
                        _2093 = 0.0;
                        float _2084;
                        float _2086;
                        float _2088;
                        float _2090;
                        float _2092;
                        float _2094;
                        for (int _2095 = -1; _2095 <= 1; _2083 = _2084, _2085 = _2086, _2087 = _2088, _2089 = _2090, _2091 = _2092, _2093 = _2094, _2095 += 2)
                        {
                            int _2098;
                            if (_2081 == 0u)
                            {
                                _2098 = _2095;
                            }
                            else
                            {
                                _2098 = 0;
                            }
                            int _2100;
                            if (_2081 == 1u)
                            {
                                _2100 = _2095;
                            }
                            else
                            {
                                _2100 = 0;
                            }
                            int2 param_var_pixel_1 = _2017 + int2(_2098, _2100);
                            WitnessQuery param_var_q_1 = WitnessQuery{ _1507, _1517, _1520, _1523, _1526 };
                            WitnessTap _2103 = witness_tap(param_var_pixel_1, param_var_q_1, Settings, source, intervalFailed, optical_product_upper);
                            if (_2103.same)
                            {
                                Interval _1250 = Interval{ _2103.feature.x, _2103.feature.x };
                                Interval _1251 = Interval{ as_type<float>(as_type<uint>(_2019.feature.x) ^ 2147483648u), as_type<float>(as_type<uint>(_2019.feature.x) ^ 2147483648u) };
                                Interval _2132 = iadd(_1250, _1251, intervalFailed);
                                Interval _1252 = Interval{ _2103.feature.y, _2103.feature.y };
                                Interval _1253 = Interval{ as_type<float>(as_type<uint>(_2019.feature.y) ^ 2147483648u), as_type<float>(as_type<uint>(_2019.feature.y) ^ 2147483648u) };
                                Interval _2135 = iadd(_1252, _1253, intervalFailed);
                                Interval _1254 = Interval{ _2103.feature.z, _2103.feature.z };
                                Interval _1255 = Interval{ as_type<float>(as_type<uint>(_2019.feature.z) ^ 2147483648u), as_type<float>(as_type<uint>(_2019.feature.z) ^ 2147483648u) };
                                Interval _2138 = iadd(_1254, _1255, intervalFailed);
                                bool _2145;
                                if (_2132.lo <= 0.0)
                                {
                                    _2145 = _2132.hi >= 0.0;
                                }
                                else
                                {
                                    _2145 = false;
                                }
                                float _2152;
                                if (_2145)
                                {
                                    _2152 = 0.0;
                                }
                                else
                                {
                                    _2152 = precise::min(abs(_2132.lo), abs(_2132.hi));
                                }
                                bool _2162;
                                if (_2135.lo <= 0.0)
                                {
                                    _2162 = _2135.hi >= 0.0;
                                }
                                else
                                {
                                    _2162 = false;
                                }
                                float _2169;
                                if (_2162)
                                {
                                    _2169 = 0.0;
                                }
                                else
                                {
                                    _2169 = precise::min(abs(_2135.lo), abs(_2135.hi));
                                }
                                bool _2179;
                                if (_2138.lo <= 0.0)
                                {
                                    _2179 = _2138.hi >= 0.0;
                                }
                                else
                                {
                                    _2179 = false;
                                }
                                float _2186;
                                if (_2179)
                                {
                                    _2186 = 0.0;
                                }
                                else
                                {
                                    _2186 = precise::min(abs(_2138.lo), abs(_2138.hi));
                                }
                                Interval _1244 = Interval{ _2152, precise::max(abs(_2132.lo), abs(_2132.hi)) };
                                Interval _1245 = Interval{ 1.5, 1.5 };
                                Interval _2191 = imul(_1244, _1245, intervalFailed, optical_product_upper);
                                Interval _1246 = Interval{ _2169, precise::max(abs(_2135.lo), abs(_2135.hi)) };
                                Interval _1247 = Interval{ 1.5, 1.5 };
                                Interval _2193 = imul(_1246, _1247, intervalFailed, optical_product_upper);
                                Interval _1248 = Interval{ _2186, precise::max(abs(_2138.lo), abs(_2138.hi)) };
                                Interval _1249 = Interval{ 1.5, 1.5 };
                                Interval _2195 = imul(_1248, _1249, intervalFailed, optical_product_upper);
                                _2084 = precise::max(_2083, _2191.lo);
                                _2086 = precise::max(_2085, _2191.hi);
                                _2088 = precise::max(_2087, _2193.lo);
                                _2090 = precise::max(_2089, _2193.hi);
                                _2092 = precise::max(_2091, _2195.lo);
                                _2094 = precise::max(_2093, _2195.hi);
                            }
                            else
                            {
                                _2084 = _2083;
                                _2086 = _2085;
                                _2088 = _2087;
                                _2090 = _2089;
                                _2092 = _2091;
                                _2094 = _2093;
                            }
                        }
                        bool param_var_pick_2 = _2081 == 0u;
                        Interval param_var_a_6 = Interval{ _1953, _1955 };
                        Interval param_var_b_6 = Interval{ _1957, _1959 };
                        float _2214 = float(pixel[_2081]);
                        Interval _1242 = witness_select(param_var_pick_2, param_var_a_6, param_var_b_6);
                        Interval _1243 = Interval{ as_type<float>(as_type<uint>(_2214) ^ 2147483648u), as_type<float>(as_type<uint>(_2214) ^ 2147483648u) };
                        Interval _2222 = iadd(_1242, _1243, intervalFailed);
                        bool _2229;
                        if (_2222.lo <= 0.0)
                        {
                            _2229 = _2222.hi >= 0.0;
                        }
                        else
                        {
                            _2229 = false;
                        }
                        float _2236;
                        if (_2229)
                        {
                            _2236 = 0.0;
                        }
                        else
                        {
                            _2236 = precise::min(abs(_2222.lo), abs(_2222.hi));
                        }
                        float _2239 = precise::max(abs(_2222.lo), abs(_2222.hi));
                        Interval _1236 = Interval{ _2083, _2085 };
                        Interval _1237 = Interval{ _2236, _2239 };
                        Interval _2242 = imul(_1236, _1237, intervalFailed, optical_product_upper);
                        Interval _1238 = Interval{ _2087, _2089 };
                        Interval _1239 = Interval{ _2236, _2239 };
                        Interval _2245 = imul(_1238, _1239, intervalFailed, optical_product_upper);
                        Interval _1240 = Interval{ _2091, _2093 };
                        Interval _1241 = Interval{ _2236, _2239 };
                        Interval _2248 = imul(_1240, _1241, intervalFailed, optical_product_upper);
                        Interval _1230 = Interval{ _2079, _2069 };
                        Interval _1231 = _2242;
                        Interval _2250 = iadd(_1230, _1231, intervalFailed);
                        Interval _1232 = Interval{ _2077, _2071 };
                        Interval _1233 = _2245;
                        Interval _2252 = iadd(_1232, _1233, intervalFailed);
                        Interval _1234 = Interval{ _2075, _2073 };
                        Interval _1235 = _2248;
                        Interval _2254 = iadd(_1234, _1235, intervalFailed);
                        _2080 = _2250.lo;
                        _2070 = _2250.hi;
                        _2078 = _2252.lo;
                        _2072 = _2252.hi;
                        _2076 = _2254.lo;
                        _2074 = _2254.hi;
                    }
                    Interval _1224 = Interval{ _2019.feature.x, _2019.feature.x };
                    Interval _1225 = Interval{ as_type<float>(as_type<uint>(_1572) ^ 2147483648u), as_type<float>(as_type<uint>(_1571) ^ 2147483648u) };
                    Interval _2278 = iadd(_1224, _1225, intervalFailed);
                    Interval _1226 = Interval{ _2019.feature.y, _2019.feature.y };
                    Interval _1227 = Interval{ as_type<float>(as_type<uint>(_1574) ^ 2147483648u), as_type<float>(as_type<uint>(_1573) ^ 2147483648u) };
                    Interval _2281 = iadd(_1226, _1227, intervalFailed);
                    Interval _1228 = Interval{ _2019.feature.z, _2019.feature.z };
                    Interval _1229 = Interval{ as_type<float>(as_type<uint>(_1576) ^ 2147483648u), as_type<float>(as_type<uint>(_1575) ^ 2147483648u) };
                    Interval _2284 = iadd(_1228, _1229, intervalFailed);
                    float _2308;
                    if ((_1520 >> 8u) == 1u)
                    {
                        _2308 = 0.04999999701976776123046875;
                    }
                    else
                    {
                        _2308 = 0.0199999995529651641845703125;
                    }
                    bool _2313;
                    if (!intervalFailed)
                    {
                        _2313 = precise::max(abs(_2278.lo), abs(_2278.hi)) > precise::min(_2079, _2308);
                    }
                    else
                    {
                        _2313 = true;
                    }
                    bool _2317;
                    if (!_2313)
                    {
                        _2317 = precise::max(abs(_2281.lo), abs(_2281.hi)) > precise::min(_2077, _2308);
                    }
                    else
                    {
                        _2317 = true;
                    }
                    bool _2321;
                    if (!_2317)
                    {
                        _2321 = precise::max(abs(_2284.lo), abs(_2284.hi)) > precise::min(_2075, _2308);
                    }
                    else
                    {
                        _2321 = true;
                    }
                    if (_2321)
                    {
                        uint _2322 = _712 >> 2u;
                        uint _2327;
                        if (intervalFailed)
                        {
                            _2327 = 6u;
                        }
                        else
                        {
                            _2327 = 5u;
                        }
                        results._m0[_2322] = 0u;
                        results._m0[_2322 + 1u] = 7u;
                        results._m0[_2322 + 2u] = _2327;
                        results._m0[_2322 + 3u] = _1944;
                        return;
                    }
                    _2000 = _2066.lo;
                    _2001 = _2066.hi;
                }
            }
            float _2385;
            float _2386;
            if (_1996)
            {
                Interval param_var_a_7 = depths[0];
                Interval param_var_a_8 = Interval{ _1974, _1975 };
                Interval _1222 = depths[1];
                Interval _1223 = Interval{ as_type<float>(as_type<uint>(depths[0].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[0].lo) ^ 2147483648u) };
                Interval _2348 = iadd(_1222, _1223, intervalFailed);
                Interval param_var_b_7 = _2348;
                Interval _2349 = imul(param_var_a_8, param_var_b_7, intervalFailed, optical_product_upper);
                Interval param_var_b_8 = _2349;
                Interval _2350 = iadd(param_var_a_7, param_var_b_8, intervalFailed);
                Interval param_var_a_9 = depths[2];
                Interval param_var_a_10 = Interval{ _1974, _1975 };
                Interval _1220 = depths[3];
                Interval _1221 = Interval{ as_type<float>(as_type<uint>(depths[2].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[2].lo) ^ 2147483648u) };
                Interval _2367 = iadd(_1220, _1221, intervalFailed);
                Interval param_var_b_9 = _2367;
                Interval _2368 = imul(param_var_a_10, param_var_b_9, intervalFailed, optical_product_upper);
                Interval param_var_b_10 = _2368;
                Interval _2369 = iadd(param_var_a_9, param_var_b_10, intervalFailed);
                Interval param_var_a_11 = _2350;
                Interval param_var_a_12 = Interval{ _1990, _1991 };
                Interval _1218 = _2369;
                Interval _1219 = Interval{ as_type<float>(as_type<uint>(_2350.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2350.lo) ^ 2147483648u) };
                Interval _2380 = iadd(_1218, _1219, intervalFailed);
                Interval param_var_b_11 = _2380;
                Interval _2381 = imul(param_var_a_12, param_var_b_11, intervalFailed, optical_product_upper);
                Interval param_var_b_12 = _2381;
                Interval _2382 = iadd(param_var_a_11, param_var_b_12, intervalFailed);
                _2385 = _2382.lo;
                _2386 = _2382.hi;
            }
            else
            {
                _2385 = _1992;
                _2386 = _1994;
            }
            Interval param_var_a_13 = Interval{ 1.0, 1.0 };
            Interval param_var_b_13 = Interval{ _2385, _2386 };
            Interval _2388 = idiv(param_var_a_13, param_var_b_13, intervalFailed, interval_divide_upper);
            Interval _1216 = _2388;
            Interval _1217 = Interval{ as_type<float>(as_type<uint>(_1840.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_1840.lo) ^ 2147483648u) };
            Interval _2398 = iadd(_1216, _1217, intervalFailed);
            float _2799;
            if (_1996)
            {
                float _2597;
                float _2598;
                float _2599;
                float _2600;
                float _2601;
                float _2602;
                float _2603;
                float _2604;
                float _2605;
                float _2606;
                float _2607;
                float _2608;
                if (frames._m0[(_713 + 488u) >> 2u] != 0u)
                {
                    uint _2413 = (_713 + 288u) >> 2u;
                    float4 _2423 = as_type<float4>(uint4(frames._m0[_2413], frames._m0[_2413 + 1u], frames._m0[_2413 + 2u], frames._m0[_2413 + 3u]));
                    float _2424 = _2423.x;
                    float _2425 = _2423.y;
                    float _2426 = _2423.z;
                    uint _2427 = (_713 + 304u) >> 2u;
                    float4 _2437 = as_type<float4>(uint4(frames._m0[_2427], frames._m0[_2427 + 1u], frames._m0[_2427 + 2u], frames._m0[_2427 + 3u]));
                    float _2438 = _2437.x;
                    float _2439 = _2437.y;
                    float _2440 = _2437.z;
                    _2597 = _2438;
                    _2598 = _2438;
                    _2599 = _2439;
                    _2600 = _2439;
                    _2601 = _2440;
                    _2602 = _2440;
                    _2603 = _2424;
                    _2604 = _2424;
                    _2605 = _2425;
                    _2606 = _2425;
                    _2607 = _2426;
                    _2608 = _2426;
                }
                else
                {
                    uint _2441 = _713 >> 2u;
                    uint _2443 = frames._m0[_2441];
                    uint _2445 = frames._m0[_2441 + 1u];
                    uint _2447 = frames._m0[_2441 + 2u];
                    uint _2449 = frames._m0[_2441 + 3u];
                    float4 _2451 = as_type<float4>(uint4(_2443, _2445, _2447, _2449));
                    uint _2452 = (_713 + 16u) >> 2u;
                    uint _2454 = frames._m0[_2452];
                    uint _2456 = frames._m0[_2452 + 1u];
                    uint _2458 = frames._m0[_2452 + 2u];
                    uint _2460 = frames._m0[_2452 + 3u];
                    float4 _2462 = as_type<float4>(uint4(_2454, _2456, _2458, _2460));
                    uint _2463 = (_713 + 32u) >> 2u;
                    uint _2465 = frames._m0[_2463];
                    uint _2467 = frames._m0[_2463 + 1u];
                    uint _2469 = frames._m0[_2463 + 2u];
                    uint _2471 = frames._m0[_2463 + 3u];
                    float4 _2473 = as_type<float4>(uint4(_2465, _2467, _2469, _2471));
                    float _2474 = _2451.x;
                    float _2475 = _2451.y;
                    float _2476 = _2451.z;
                    float _2591;
                    float _2592;
                    float _2593;
                    float _2594;
                    float _2595;
                    float _2596;
                    if (all(float3(_2451.w, _2462.w, _2473.w) == float3(2.0)))
                    {
                        float _2486 = _2462.x;
                        float _2487 = _2462.y;
                        float _2488 = _2462.z;
                        _2591 = _2486;
                        _2592 = _2486;
                        _2593 = _2487;
                        _2594 = _2487;
                        _2595 = _2488;
                        _2596 = _2488;
                    }
                    else
                    {
                        float _2489 = _2462.x;
                        float _2490 = _2462.y;
                        float _2491 = _2462.z;
                        Interval _1210 = Interval{ _2489, _2489 };
                        Interval _1211 = Interval{ as_type<float>(as_type<uint>(_2474) ^ 2147483648u), as_type<float>(as_type<uint>(_2474) ^ 2147483648u) };
                        Interval _2512 = iadd(_1210, _1211, intervalFailed);
                        Interval _1212 = Interval{ _2490, _2490 };
                        Interval _1213 = Interval{ as_type<float>(as_type<uint>(_2475) ^ 2147483648u), as_type<float>(as_type<uint>(_2475) ^ 2147483648u) };
                        Interval _2515 = iadd(_1212, _1213, intervalFailed);
                        Interval _1214 = Interval{ _2491, _2491 };
                        Interval _1215 = Interval{ as_type<float>(as_type<uint>(_2476) ^ 2147483648u), as_type<float>(as_type<uint>(_2476) ^ 2147483648u) };
                        Interval _2518 = iadd(_1214, _1215, intervalFailed);
                        float _2519 = _2473.x;
                        float _2520 = _2473.y;
                        float _2521 = _2473.z;
                        Interval _1204 = Interval{ _2519, _2519 };
                        Interval _1205 = Interval{ as_type<float>(as_type<uint>(_2474) ^ 2147483648u), as_type<float>(as_type<uint>(_2474) ^ 2147483648u) };
                        Interval _2542 = iadd(_1204, _1205, intervalFailed);
                        Interval _1206 = Interval{ _2520, _2520 };
                        Interval _1207 = Interval{ as_type<float>(as_type<uint>(_2475) ^ 2147483648u), as_type<float>(as_type<uint>(_2475) ^ 2147483648u) };
                        Interval _2545 = iadd(_1206, _1207, intervalFailed);
                        Interval _1208 = Interval{ _2521, _2521 };
                        Interval _1209 = Interval{ as_type<float>(as_type<uint>(_2476) ^ 2147483648u), as_type<float>(as_type<uint>(_2476) ^ 2147483648u) };
                        Interval _2548 = iadd(_1208, _1209, intervalFailed);
                        Interval _1192 = _2515;
                        Interval _1193 = _2548;
                        Interval _2549 = imul(_1192, _1193, intervalFailed, optical_product_upper);
                        Interval _1194 = _2518;
                        Interval _1195 = _2545;
                        Interval _2550 = imul(_1194, _1195, intervalFailed, optical_product_upper);
                        Interval _1190 = _2549;
                        Interval _1191 = Interval{ as_type<float>(as_type<uint>(_2550.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2550.lo) ^ 2147483648u) };
                        Interval _2560 = iadd(_1190, _1191, intervalFailed);
                        Interval _1196 = _2518;
                        Interval _1197 = _2542;
                        Interval _2561 = imul(_1196, _1197, intervalFailed, optical_product_upper);
                        Interval _1198 = _2512;
                        Interval _1199 = _2548;
                        Interval _2562 = imul(_1198, _1199, intervalFailed, optical_product_upper);
                        Interval _1188 = _2561;
                        Interval _1189 = Interval{ as_type<float>(as_type<uint>(_2562.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2562.lo) ^ 2147483648u) };
                        Interval _2572 = iadd(_1188, _1189, intervalFailed);
                        Interval _1200 = _2512;
                        Interval _1201 = _2545;
                        Interval _2573 = imul(_1200, _1201, intervalFailed, optical_product_upper);
                        Interval _1202 = _2515;
                        Interval _1203 = _2542;
                        Interval _2574 = imul(_1202, _1203, intervalFailed, optical_product_upper);
                        Interval _1186 = _2573;
                        Interval _1187 = Interval{ as_type<float>(as_type<uint>(_2574.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2574.lo) ^ 2147483648u) };
                        Interval _2584 = iadd(_1186, _1187, intervalFailed);
                        _2591 = _2560.lo;
                        _2592 = _2560.hi;
                        _2593 = _2572.lo;
                        _2594 = _2572.hi;
                        _2595 = _2584.lo;
                        _2596 = _2584.hi;
                    }
                    _2597 = _2591;
                    _2598 = _2592;
                    _2599 = _2593;
                    _2600 = _2594;
                    _2601 = _2595;
                    _2602 = _2596;
                    _2603 = _2474;
                    _2604 = _2474;
                    _2605 = _2475;
                    _2606 = _2475;
                    _2607 = _2476;
                    _2608 = _2476;
                }
                Interval _1176 = Interval{ _2603, _2604 };
                Interval _1177 = Interval{ _2597, _2598 };
                Interval _2611 = imul(_1176, _1177, intervalFailed, optical_product_upper);
                Interval _1178 = _2611;
                Interval _1179 = Interval{ _2605, _2606 };
                Interval _1180 = Interval{ _2599, _2600 };
                Interval _2614 = imul(_1179, _1180, intervalFailed, optical_product_upper);
                Interval _1181 = _2614;
                Interval _2615 = iadd(_1178, _1181, intervalFailed);
                Interval _1182 = _2615;
                Interval _1183 = Interval{ _2607, _2608 };
                Interval _1184 = Interval{ _2601, _2602 };
                Interval _2618 = imul(_1183, _1184, intervalFailed, optical_product_upper);
                Interval _1185 = _2618;
                Interval _2619 = iadd(_1182, _1185, intervalFailed);
                uint _2620 = (_713 + 48u) >> 2u;
                uint _2622 = frames._m0[_2620];
                uint _2624 = frames._m0[_2620 + 1u];
                uint _2626 = frames._m0[_2620 + 2u];
                uint _2628 = frames._m0[_2620 + 3u];
                float4 _2630 = as_type<float4>(uint4(_2622, _2624, _2626, _2628));
                float _776 = spvFAdd(float(_1949), 0.5);
                float _2632 = _2630.z;
                Interval _1174 = Interval{ _776, _776 };
                Interval _1175 = Interval{ as_type<float>(as_type<uint>(_2632) ^ 2147483648u), as_type<float>(as_type<uint>(_2632) ^ 2147483648u) };
                Interval _2641 = iadd(_1174, _1175, intervalFailed);
                Interval param_var_a_14 = _2641;
                float _2642 = _2630.x;
                Interval param_var_b_14 = Interval{ _2642, _2642 };
                Interval _2644 = idiv(param_var_a_14, param_var_b_14, intervalFailed, interval_divide_upper);
                float _777 = spvFAdd(float(_1945), 0.5);
                float _2646 = _2630.w;
                Interval _1172 = Interval{ _777, _777 };
                Interval _1173 = Interval{ as_type<float>(as_type<uint>(_2646) ^ 2147483648u), as_type<float>(as_type<uint>(_2646) ^ 2147483648u) };
                Interval _2655 = iadd(_1172, _1173, intervalFailed);
                Interval param_var_a_15 = _2655;
                float _2656 = _2630.y;
                Interval param_var_b_15 = Interval{ _2656, _2656 };
                Interval _2658 = idiv(param_var_a_15, param_var_b_15, intervalFailed, interval_divide_upper);
                Interval _1162 = _2644;
                Interval _1163 = Interval{ _2597, _2598 };
                Interval _2660 = imul(_1162, _1163, intervalFailed, optical_product_upper);
                Interval _1164 = _2660;
                Interval _1165 = _2658;
                Interval _1166 = Interval{ _2599, _2600 };
                Interval _2662 = imul(_1165, _1166, intervalFailed, optical_product_upper);
                Interval _1167 = _2662;
                Interval _2663 = iadd(_1164, _1167, intervalFailed);
                Interval _1168 = _2663;
                Interval _1169 = Interval{ 1.0, 1.0 };
                Interval _1170 = Interval{ _2601, _2602 };
                Interval _2665 = imul(_1169, _1170, intervalFailed, optical_product_upper);
                Interval _1171 = _2665;
                Interval _2666 = iadd(_1168, _1171, intervalFailed);
                Interval param_var_a_16 = _2666;
                Interval param_var_b_16 = _2619;
                Interval _2667 = idiv(param_var_a_16, param_var_b_16, intervalFailed, interval_divide_upper);
                Interval param_var_a_17 = Interval{ _2597, _2598 };
                Interval param_var_a_18 = _2619;
                float _2669 = _2630.x;
                Interval param_var_b_17 = Interval{ _2669, _2669 };
                Interval _2671 = imul(param_var_a_18, param_var_b_17, intervalFailed, optical_product_upper);
                Interval param_var_b_18 = _2671;
                Interval _2672 = idiv(param_var_a_17, param_var_b_18, intervalFailed, interval_divide_upper);
                Interval param_var_a_19 = Interval{ _2599, _2600 };
                Interval param_var_a_20 = _2619;
                float _2674 = _2630.y;
                Interval param_var_b_19 = Interval{ _2674, _2674 };
                Interval _2676 = imul(param_var_a_20, param_var_b_19, intervalFailed, optical_product_upper);
                Interval param_var_b_20 = _2676;
                Interval _2677 = idiv(param_var_a_19, param_var_b_20, intervalFailed, interval_divide_upper);
                Interval _1160 = depths[0];
                Interval _1161 = Interval{ as_type<float>(as_type<uint>(_2667.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2667.lo) ^ 2147483648u) };
                Interval _2689 = iadd(_1160, _1161, intervalFailed);
                Interval _1158 = depths[1];
                Interval _1159 = Interval{ as_type<float>(as_type<uint>(depths[0].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[0].lo) ^ 2147483648u) };
                Interval _2703 = iadd(_1158, _1159, intervalFailed);
                Interval _1156 = _2703;
                Interval _1157 = Interval{ as_type<float>(as_type<uint>(_2672.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2672.lo) ^ 2147483648u) };
                Interval _2713 = iadd(_1156, _1157, intervalFailed);
                Interval _1154 = depths[2];
                Interval _1155 = Interval{ as_type<float>(as_type<uint>(depths[0].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[0].lo) ^ 2147483648u) };
                Interval _2727 = iadd(_1154, _1155, intervalFailed);
                Interval _1152 = _2727;
                Interval _1153 = Interval{ as_type<float>(as_type<uint>(_2677.hi) ^ 2147483648u), as_type<float>(as_type<uint>(_2677.lo) ^ 2147483648u) };
                Interval _2737 = iadd(_1152, _1153, intervalFailed);
                Interval _1150 = depths[3];
                Interval _1151 = Interval{ as_type<float>(as_type<uint>(depths[1].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[1].lo) ^ 2147483648u) };
                Interval _2751 = iadd(_1150, _1151, intervalFailed);
                Interval _1148 = _2751;
                Interval _1149 = Interval{ as_type<float>(as_type<uint>(depths[2].hi) ^ 2147483648u), as_type<float>(as_type<uint>(depths[2].lo) ^ 2147483648u) };
                Interval _2763 = iadd(_1148, _1149, intervalFailed);
                Interval param_var_a_21 = _2763;
                Interval param_var_b_21 = depths[0];
                Interval _2766 = iadd(param_var_a_21, param_var_b_21, intervalFailed);
                Interval param_var_a_22 = _2689;
                Interval param_var_a_23 = Interval{ _1974, _1975 };
                Interval param_var_b_22 = _2713;
                Interval _2768 = imul(param_var_a_23, param_var_b_22, intervalFailed, optical_product_upper);
                Interval param_var_b_23 = _2768;
                Interval _2769 = iadd(param_var_a_22, param_var_b_23, intervalFailed);
                Interval param_var_a_24 = _2769;
                Interval param_var_a_25 = Interval{ _1990, _1991 };
                Interval param_var_a_26 = _2737;
                Interval param_var_a_27 = Interval{ _1974, _1975 };
                Interval param_var_b_24 = _2766;
                Interval _2772 = imul(param_var_a_27, param_var_b_24, intervalFailed, optical_product_upper);
                Interval param_var_b_25 = _2772;
                Interval _2773 = iadd(param_var_a_26, param_var_b_25, intervalFailed);
                Interval param_var_b_26 = _2773;
                Interval _2774 = imul(param_var_a_25, param_var_b_26, intervalFailed, optical_product_upper);
                Interval param_var_b_27 = _2774;
                Interval _2775 = iadd(param_var_a_24, param_var_b_27, intervalFailed);
                bool _2782;
                if (_2775.lo <= 0.0)
                {
                    _2782 = _2775.hi >= 0.0;
                }
                else
                {
                    _2782 = false;
                }
                float _2789;
                if (_2782)
                {
                    _2789 = 0.0;
                }
                else
                {
                    _2789 = precise::min(abs(_2775.lo), abs(_2775.hi));
                }
                Interval param_var_a_28 = Interval{ _2789, precise::max(abs(_2775.lo), abs(_2775.hi)) };
                Interval param_var_a_29 = Interval{ _2385, _2386 };
                Interval param_var_a_30 = Interval{ 1.0, 1.0 };
                Interval param_var_b_28 = _1840;
                Interval _2795 = idiv(param_var_a_30, param_var_b_28, intervalFailed, interval_divide_upper);
                Interval param_var_b_29 = _2795;
                Interval _2796 = imul(param_var_a_29, param_var_b_29, intervalFailed, optical_product_upper);
                Interval param_var_b_30 = _2796;
                Interval _2797 = idiv(param_var_a_28, param_var_b_30, intervalFailed, interval_divide_upper);
                _2799 = _2797.hi;
            }
            else
            {
                _2799 = precise::max(abs(_2398.lo), abs(_2398.hi));
            }
            Interval param_var_a_31 = _1840;
            Interval param_var_b_31 = Interval{ 0.004999999888241291046142578125, 0.004999999888241291046142578125 };
            Interval _2800 = imul(param_var_a_31, param_var_b_31, intervalFailed, optical_product_upper);
            bool _2806;
            if (!intervalFailed)
            {
                _2806 = _2385 <= 0.0;
            }
            else
            {
                _2806 = true;
            }
            bool _2809;
            if (!_2806)
            {
                _2809 = _2799 > precise::max(0.00999999977648258209228515625, _2800.lo);
            }
            else
            {
                _2809 = true;
            }
            if (_2809)
            {
                uint _2810 = _712 >> 2u;
                uint _2815;
                if (intervalFailed)
                {
                    _2815 = 6u;
                }
                else
                {
                    _2815 = 4u;
                }
                results._m0[_2810] = 0u;
                results._m0[_2810 + 1u] = 23u;
                results._m0[_2810 + 2u] = _2815;
                results._m0[_2810 + 3u] = _1944;
                return;
            }
            _778 = _1944 + 1u;
        }
    }
    bool _2823;
    if (!intervalFailed)
    {
        _2823 = _1943 == 0u;
    }
    else
    {
        _2823 = true;
    }
    if (_2823)
    {
        uint _2824 = _712 >> 2u;
        results._m0[_2824] = 0u;
        results._m0[_2824 + 1u] = 0u;
        results._m0[_2824 + 2u] = 6u;
        results._m0[_2824 + 3u] = _1943;
        return;
    }
    uint _2830 = _712 >> 2u;
    results._m0[_2830] = 1u;
    results._m0[_2830 + 1u] = 31u;
    results._m0[_2830 + 2u] = 0u;
    results._m0[_2830 + 3u] = _1943;
}

kernel void feature_witness_main(constant type_Settings& Settings [[buffer(0)]], device type_ByteAddressBuffer& frames [[buffer(3)]], device type_ByteAddressBuffer& queries [[buffer(4)]], device type_ByteAddressBuffer& source [[buffer(5)]], device type_RWByteAddressBuffer& results [[buffer(6)]], constant type_WitnessSettings& WitnessSettings [[buffer(1)]], constant type_TargetSettings& TargetSettings [[buffer(2)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    bool intervalFailed = false;
    float optical_product_upper = 0.0;
    float interval_divide_upper = 0.0;
    uint3 param_var_id = gl_GlobalInvocationID;
    src_feature_witness_main(param_var_id, Settings, frames, queries, source, results, WitnessSettings, intervalFailed, optical_product_upper, interval_divide_upper, TargetSettings);
}

