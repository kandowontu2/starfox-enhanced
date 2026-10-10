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
    uint lobes;
    uint flags;
    uint previousVertices;
    uint previousMapping;
    uint triangles;
    uint geometryBytes;
    float weight;
    float roughness;
    float4 projection;
    float4 clip;
    uint oldTriangles;
    uint scenePaths;
    uint primaryPrefix;
    uint curvedReceivers;
    float4 currentCube[3];
    float4 previousCube[3];
    float4 currentPoint;
    float4 currentNormal;
    float4 previousPoint;
    float4 previousNormal;
};

struct ReflectionSpecularPlane
{
    float4 a;
    float4 b;
    float4 c;
};

constant bool _171 = {};
constant float2 _172 = {};
constant float4 _173 = {};

constant spvUnsafeArray<float2, 8> _175 = spvUnsafeArray<float2, 8>({ float2(0.5, 0.0), float2(-0.5, 0.0), float2(0.0, 0.5), float2(0.0, -0.5), float2(0.611999988555908203125), float2(-0.611999988555908203125, 0.611999988555908203125), float2(0.611999988555908203125, -0.611999988555908203125), float2(-0.611999988555908203125) });

kernel void reflection_paths_main(device type_ByteAddressBuffer& current [[buffer(0)]], device type_ByteAddressBuffer& history [[buffer(1)]], device type_ByteAddressBuffer& geometry [[buffer(2)]], device type_RWByteAddressBuffer& resolved [[buffer(3)]], constant type_Settings& Settings [[buffer(4)]], texture2d<float> ownership [[texture(0)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        bool _223;
        if (gl_GlobalInvocationID.x < Settings.width)
        {
            _223 = gl_GlobalInvocationID.y >= Settings.height;
        }
        else
        {
            _223 = true;
        }
        if (_223)
        {
            break;
        }
        uint _228 = Settings.width * Settings.height;
        uint _232 = (gl_GlobalInvocationID.y * Settings.width) + gl_GlobalInvocationID.x;
        uint _233 = _232 * 4u;
        uint _234 = _233 >> 2u;
        uint _236 = current._m0[_234];
        uint _239 = (4u * (_228 + _232)) >> 2u;
        uint _241 = current._m0[_239];
        uint _242 = _228 * 8u;
        uint _244 = (_242 + _233) >> 2u;
        uint _246 = current._m0[_244];
        float _247 = as_type<float>(_246);
        uint _248 = _228 * 12u;
        uint _249 = _232 * 16u;
        uint _251 = (_248 + _249) >> 2u;
        uint _253 = current._m0[_251];
        uint _254 = _251 + 1u;
        uint _256 = current._m0[_254];
        uint _257 = _251 + 2u;
        uint _259 = current._m0[_257];
        uint _260 = _251 + 3u;
        uint _262 = current._m0[_260];
        float4 _264 = as_type<float4>(uint4(_253, _256, _259, _262));
        bool _267 = Settings.scenePaths != 0u;
        float4 _287;
        if (_267)
        {
            uint _273 = ((_228 * 28u) + _249) >> 2u;
            _287 = as_type<float4>(uint4(current._m0[_273], current._m0[_273 + 1u], current._m0[_273 + 2u], current._m0[_273 + 3u]));
        }
        else
        {
            _287 = float4(0.0, 0.0, 0.0, 1.0);
        }
        float4 _295 = ownership.read(uint2(int3(int(gl_GlobalInvocationID.x), int(gl_GlobalInvocationID.y), 0).xy), 0);
        bool _296 = _241 == 4294967294u;
        bool _356;
        if (_296)
        {
            float _304 = dot(Settings.currentNormal.xyz, Settings.currentNormal.xyz);
            bool _310;
            if (Settings.scenePaths == 1u)
            {
                _310 = Settings.currentPoint.w == 1.0;
            }
            else
            {
                _310 = false;
            }
            bool _315;
            if (_310)
            {
                _315 = Settings.currentNormal.w == 0.0;
            }
            else
            {
                _315 = false;
            }
            bool _323;
            if (_315)
            {
                bool4 _318 = isnan(Settings.currentPoint);
                bool4 _319 = isinf(Settings.currentPoint);
                _323 = all(not(bool4(_318.x || _319.x, _318.y || _319.y, _318.z || _319.z, _318.w || _319.w)));
            }
            else
            {
                _323 = false;
            }
            bool _331;
            if (_323)
            {
                bool4 _326 = isnan(Settings.currentNormal);
                bool4 _327 = isinf(Settings.currentNormal);
                _331 = all(not(bool4(_326.x || _327.x, _326.y || _327.y, _326.z || _327.z, _326.w || _327.w)));
            }
            else
            {
                _331 = false;
            }
            bool _338;
            if (_331)
            {
                _338 = all(abs(Settings.currentPoint.xyz) <= float3(999999995904.0));
            }
            else
            {
                _338 = false;
            }
            bool _344;
            if (_338)
            {
                _344 = all(abs(Settings.currentNormal.xyz) <= float3(999999995904.0));
            }
            else
            {
                _344 = false;
            }
            bool _351;
            if (_344)
            {
                _351 = !(isnan(_304) || isinf(_304));
            }
            else
            {
                _351 = false;
            }
            bool _355;
            if (_351)
            {
                _355 = _304 > 9.9999996826552253889678874634872e-21;
            }
            else
            {
                _355 = false;
            }
            _356 = _355;
        }
        else
        {
            _356 = false;
        }
        bool _381;
        if (_267)
        {
            bool _368;
            if (_287.w == 1.0)
            {
                bool4 _363 = isnan(_287);
                bool4 _364 = isinf(_287);
                _368 = all(not(bool4(_363.x || _364.x, _363.y || _364.y, _363.z || _364.z, _363.w || _364.w)));
            }
            else
            {
                _368 = false;
            }
            bool _374;
            if (_368)
            {
                _374 = all(_287.xyz >= float3(0.0));
            }
            else
            {
                _374 = false;
            }
            bool _380;
            if (_374)
            {
                _380 = all(_287.xyz <= float3(999999995904.0));
            }
            else
            {
                _380 = false;
            }
            _381 = _380;
        }
        else
        {
            _381 = true;
        }
        bool _395;
        if (_356)
        {
            _395 = (_236 >> 24u) == 254u;
        }
        else
        {
            bool _392;
            if (_241 < Settings.triangles)
            {
                _392 = (_236 >> 24u) == 255u;
            }
            else
            {
                _392 = false;
            }
            _395 = _392;
        }
        bool _433;
        if (_295.w > 0.5)
        {
            bool _432;
            do
            {
                uint _405 = uint(rint(_295.z * 255.0));
                float _406 = _295.x;
                bool _415;
                if ((isunordered(_406, 0.0) || _406 > 0.0))
                {
                    bool _414;
                    if (_405 != 1u)
                    {
                        _414 = _405 != 2u;
                    }
                    else
                    {
                        _414 = false;
                    }
                    _415 = _414;
                }
                else
                {
                    _415 = true;
                }
                if (_415)
                {
                    _432 = false;
                    break;
                }
                uint _418 = _236 >> 24u;
                if (_418 == 253u)
                {
                    _432 = true;
                    break;
                }
                if (_406 == 0.0039215688593685626983642578125)
                {
                    _432 = _418 == 254u;
                    break;
                }
                bool _431;
                if (_418 == 255u)
                {
                    _431 = _295.y > 0.0;
                }
                else
                {
                    _431 = false;
                }
                _432 = _431;
                break;
            } while(false);
            _433 = _432;
        }
        else
        {
            _433 = false;
        }
        bool _441;
        if (_433 ? _395 : false)
        {
            _441 = !(isnan(_247) || isinf(_247));
        }
        else
        {
            _441 = false;
        }
        bool _445;
        if (_441)
        {
            _445 = _247 > 0.0;
        }
        else
        {
            _445 = false;
        }
        bool _450;
        if (_445)
        {
            _450 = _264.w == 1.0;
        }
        else
        {
            _450 = false;
        }
        bool _459;
        if (_450 ? _381 : false)
        {
            bool4 _454 = isnan(_264);
            bool4 _455 = isinf(_264);
            _459 = all(not(bool4(_454.x || _455.x, _454.y || _455.y, _454.z || _455.z, _454.w || _455.w)));
        }
        else
        {
            _459 = false;
        }
        bool _465;
        if (_459)
        {
            _465 = all(_264.xyz >= float3(0.0));
        }
        else
        {
            _465 = false;
        }
        bool _471;
        if (_465)
        {
            _471 = all(_264.xyz <= float3(1.0));
        }
        else
        {
            _471 = false;
        }
        bool _477;
        if (_471)
        {
            _477 = any(_264.xyz > float3(0.0));
        }
        else
        {
            _477 = false;
        }
        bool _482;
        _482 = _477;
        bool _480;
        bool _483;
        bool _479;
        for (uint _484 = 0u; _484 < Settings.lobes; _479 = _480, _482 = _483, _484++)
        {
            if (_482)
            {
                uint _499 = (_228 * uint(_267 ? 44 : 28)) + (((_232 * Settings.lobes) + _484) * 52u);
                uint _500 = _499 >> 2u;
                uint _514 = (_499 + 16u) >> 2u;
                uint _517 = _514 + 1u;
                uint _521 = (_499 + 24u) >> 2u;
                float3 _531 = as_type<float3>(uint3(current._m0[_521], current._m0[_521 + 1u], current._m0[_521 + 2u]));
                uint _537 = (_499 + 40u) >> 2u;
                float3 _547 = as_type<float3>(uint3(current._m0[_537], current._m0[_537 + 1u], current._m0[_537 + 2u]));
                uint4 _186 = uint4(current._m0[_500], current._m0[_500 + 1u], current._m0[_500 + 2u], current._m0[_500 + 3u]);
                bool _745;
                do
                {
                    uint _552 = current._m0[_514] & 7u;
                    uint _553 = current._m0[_514] >> 8u;
                    bool _558;
                    if (_552 <= 4u)
                    {
                        _558 = _553 < 1u;
                    }
                    else
                    {
                        _558 = true;
                    }
                    bool _563;
                    if (!_558)
                    {
                        _563 = _553 > 3u;
                    }
                    else
                    {
                        _563 = true;
                    }
                    bool _570;
                    if (!_563)
                    {
                        _570 = current._m0[_514] != ((_553 << 8u) | _552);
                    }
                    else
                    {
                        _570 = true;
                    }
                    bool _579;
                    if (!_570)
                    {
                        bool _578;
                        if (_553 != 3u)
                        {
                            _578 = _552 == 4u;
                        }
                        else
                        {
                            _578 = false;
                        }
                        _579 = _578;
                    }
                    else
                    {
                        _579 = true;
                    }
                    bool _588;
                    if (!_579)
                    {
                        bool _587;
                        if (_553 == 3u)
                        {
                            _587 = _552 != 4u;
                        }
                        else
                        {
                            _587 = false;
                        }
                        _588 = _587;
                    }
                    else
                    {
                        _588 = true;
                    }
                    bool _594;
                    if (!_588)
                    {
                        _594 = (current._m0[(_499 + 36u) >> 2u] >> 24u) != 255u;
                    }
                    else
                    {
                        _594 = true;
                    }
                    bool _604;
                    if (!_594)
                    {
                        bool3 _598 = isnan(_531);
                        bool3 _599 = isinf(_531);
                        _604 = !all(not(bool3(_598.x || _599.x, _598.y || _599.y, _598.z || _599.z)));
                    }
                    else
                    {
                        _604 = true;
                    }
                    bool _614;
                    if (!_604)
                    {
                        bool3 _608 = isnan(_547);
                        bool3 _609 = isinf(_547);
                        _614 = !all(not(bool3(_608.x || _609.x, _608.y || _609.y, _608.z || _609.z)));
                    }
                    else
                    {
                        _614 = true;
                    }
                    bool _620;
                    if (!_614)
                    {
                        _620 = any(_547 < float3(0.0));
                    }
                    else
                    {
                        _620 = true;
                    }
                    bool _626;
                    if (!_620)
                    {
                        _626 = any(_547 > float3(1.0));
                    }
                    else
                    {
                        _626 = true;
                    }
                    if (_626)
                    {
                        _745 = false;
                        break;
                    }
                    bool _713;
                    bool _714;
                    uint _630 = 0u;
                    for (;;)
                    {
                        if (_630 < 4u)
                        {
                            bool _710;
                            if (_630 < _552)
                            {
                                bool _709;
                                if (_186[_630] == 4294967294u)
                                {
                                    float _656 = dot(Settings.currentNormal.xyz, Settings.currentNormal.xyz);
                                    bool _662;
                                    if (Settings.scenePaths == 1u)
                                    {
                                        _662 = Settings.currentPoint.w == 1.0;
                                    }
                                    else
                                    {
                                        _662 = false;
                                    }
                                    bool _667;
                                    if (_662)
                                    {
                                        _667 = Settings.currentNormal.w == 0.0;
                                    }
                                    else
                                    {
                                        _667 = false;
                                    }
                                    bool _675;
                                    if (_667)
                                    {
                                        bool4 _670 = isnan(Settings.currentPoint);
                                        bool4 _671 = isinf(Settings.currentPoint);
                                        _675 = all(not(bool4(_670.x || _671.x, _670.y || _671.y, _670.z || _671.z, _670.w || _671.w)));
                                    }
                                    else
                                    {
                                        _675 = false;
                                    }
                                    bool _683;
                                    if (_675)
                                    {
                                        bool4 _678 = isnan(Settings.currentNormal);
                                        bool4 _679 = isinf(Settings.currentNormal);
                                        _683 = all(not(bool4(_678.x || _679.x, _678.y || _679.y, _678.z || _679.z, _678.w || _679.w)));
                                    }
                                    else
                                    {
                                        _683 = false;
                                    }
                                    bool _690;
                                    if (_683)
                                    {
                                        _690 = all(abs(Settings.currentPoint.xyz) <= float3(999999995904.0));
                                    }
                                    else
                                    {
                                        _690 = false;
                                    }
                                    bool _696;
                                    if (_690)
                                    {
                                        _696 = all(abs(Settings.currentNormal.xyz) <= float3(999999995904.0));
                                    }
                                    else
                                    {
                                        _696 = false;
                                    }
                                    bool _703;
                                    if (_696)
                                    {
                                        _703 = !(isnan(_656) || isinf(_656));
                                    }
                                    else
                                    {
                                        _703 = false;
                                    }
                                    bool _707;
                                    if (_703)
                                    {
                                        _707 = _656 > 9.9999996826552253889678874634872e-21;
                                    }
                                    else
                                    {
                                        _707 = false;
                                    }
                                    _709 = !_707;
                                }
                                else
                                {
                                    _709 = _186[_630] >= Settings.triangles;
                                }
                                _710 = _709;
                            }
                            else
                            {
                                _710 = _186[_630] != 4294967295u;
                            }
                            if (_710)
                            {
                                _713 = false;
                                _714 = true;
                                break;
                            }
                            _630++;
                            continue;
                        }
                        else
                        {
                            _713 = _479;
                            _714 = false;
                            break;
                        }
                    }
                    if (_714)
                    {
                        _745 = _713;
                        break;
                    }
                    if (_553 == 1u)
                    {
                        bool _724;
                        if (current._m0[_517] < Settings.triangles)
                        {
                            _724 = _531.z == 0.0;
                        }
                        else
                        {
                            _724 = false;
                        }
                        bool _730;
                        if (_724)
                        {
                            _730 = all(_531.xy >= float2(0.0));
                        }
                        else
                        {
                            _730 = false;
                        }
                        bool _736;
                        if (_730)
                        {
                            _736 = dot(_531.xy, float2(1.0)) <= 1.0;
                        }
                        else
                        {
                            _736 = false;
                        }
                        _745 = _736;
                        break;
                    }
                    bool _744;
                    if (current._m0[_517] == 4294967295u)
                    {
                        _744 = abs(dot(_531, _531) - 1.0) < 9.9999997473787516355514526367188e-05;
                    }
                    else
                    {
                        _744 = false;
                    }
                    _745 = _744;
                    break;
                } while(false);
                _480 = _745;
                _483 = _745;
            }
            else
            {
                _480 = _479;
                _483 = false;
            }
        }
        resolved._m0[_234] = _236;
        resolved._m0[_239] = _482 ? _241 : 4294967295u;
        resolved._m0[_244] = _246;
        resolved._m0[_251] = _253;
        resolved._m0[_254] = _256;
        resolved._m0[_257] = _259;
        resolved._m0[_260] = _262;
        if (_267)
        {
            uint _758 = ((_228 * 28u) + _249) >> 2u;
            uint4 _759 = as_type<uint4>(_287);
            resolved._m0[_758] = _759.x;
            resolved._m0[_758 + 1u] = _759.y;
            resolved._m0[_758 + 2u] = _759.z;
            resolved._m0[_758 + 3u] = _759.w;
        }
        float4 _779;
        float2 _789;
        float3 _799;
        bool _801;
        _779 = _173;
        _789 = _172;
        _799 = float3(0.0);
        _801 = false;
        float3 _800;
        uint4 _184;
        spvUnsafeArray<ReflectionSpecularPlane, 4> _206;
        bool _773;
        bool _776;
        bool _778;
        float4 _780;
        bool _782;
        bool _784;
        bool _786;
        bool _788;
        float2 _790;
        bool _792;
        bool _794;
        bool _796;
        bool _798;
        bool _802;
        bool _806;
        bool _772;
        bool _775;
        bool _777;
        bool _781;
        bool _783;
        bool _785;
        bool _787;
        bool _791;
        bool _793;
        bool _795;
        bool _797;
        bool _805;
        for (uint _803 = 0u; _803 < Settings.lobes; _772 = _773, _775 = _776, _777 = _778, _779 = _780, _781 = _782, _783 = _784, _785 = _786, _787 = _788, _789 = _790, _791 = _792, _793 = _794, _795 = _796, _797 = _798, _799 = _800, _801 = _802, _803++, _805 = _806)
        {
            uint _811 = uint(_267 ? 44 : 28);
            uint _812 = _228 * _811;
            uint _816 = _812 + (((_232 * Settings.lobes) + _803) * 52u);
            uint _817 = _816 >> 2u;
            uint _819 = current._m0[_817];
            uint _820 = _817 + 1u;
            uint _822 = current._m0[_820];
            uint _823 = _817 + 2u;
            uint _825 = current._m0[_823];
            uint _826 = _817 + 3u;
            uint _828 = current._m0[_826];
            uint4 _829 = uint4(_819, _822, _825, _828);
            uint _831 = (_816 + 16u) >> 2u;
            uint _833 = current._m0[_831];
            uint _834 = _831 + 1u;
            uint _836 = current._m0[_834];
            uint _838 = (_816 + 24u) >> 2u;
            uint _840 = current._m0[_838];
            uint _841 = _838 + 1u;
            uint _843 = current._m0[_841];
            uint _844 = _838 + 2u;
            uint _846 = current._m0[_844];
            float3 _848 = as_type<float3>(uint3(_840, _843, _846));
            uint _850 = (_816 + 36u) >> 2u;
            uint _854 = (_816 + 40u) >> 2u;
            uint _856 = current._m0[_854];
            uint _857 = _854 + 1u;
            uint _859 = current._m0[_857];
            uint _860 = _854 + 2u;
            uint _862 = current._m0[_860];
            float3 _864 = as_type<float3>(uint3(_856, _859, _862));
            uint4 _185 = _829;
            float3 _874 = float3(float(current._m0[_850] & 255u), float((current._m0[_850] >> 8u) & 255u), float((current._m0[_850] >> 16u) & 255u)) * float3(0.0039215688593685626983642578125);
            bool _878 = (Settings.flags & 2u) != 0u;
            float3 _913;
            if (_878)
            {
                float _882 = _874.x;
                float _891;
                if (_882 <= 0.040449999272823333740234375)
                {
                    _891 = _882 * 0.077399380505084991455078125;
                }
                else
                {
                    _891 = powr((_882 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                float _892 = _874.y;
                float _901;
                if (_892 <= 0.040449999272823333740234375)
                {
                    _901 = _892 * 0.077399380505084991455078125;
                }
                else
                {
                    _901 = powr((_892 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                float _902 = _874.z;
                float _911;
                if (_902 <= 0.040449999272823333740234375)
                {
                    _911 = _902 * 0.077399380505084991455078125;
                }
                else
                {
                    _911 = powr((_902 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                _913 = float3(_891, _901, _911);
            }
            else
            {
                _913 = _874;
            }
            bool _918;
            if (_482)
            {
                _918 = (Settings.flags & 1u) != 0u;
            }
            else
            {
                _918 = false;
            }
            bool _924;
            if (_918)
            {
                _924 = Settings.weight > 0.0;
            }
            else
            {
                _924 = false;
            }
            uint _8883;
            float3 _8884;
            if (_924)
            {
                uint _929;
                uint4 _183 = _829;
                uint _1066;
                float3 _1067;
                bool _1068;
                do
                {
                    _184 = _829;
                    _929 = _833 & 7u;
                    bool _1017;
                    bool _1018;
                    uint _931 = 0u;
                    for (;;)
                    {
                        if (_931 < _929)
                        {
                            if (_183[_931] == 4294967294u)
                            {
                                float _947 = dot(Settings.previousNormal.xyz, Settings.previousNormal.xyz);
                                bool _953;
                                if (Settings.scenePaths == 1u)
                                {
                                    _953 = Settings.previousPoint.w == 1.0;
                                }
                                else
                                {
                                    _953 = false;
                                }
                                bool _958;
                                if (_953)
                                {
                                    _958 = Settings.previousNormal.w == 0.0;
                                }
                                else
                                {
                                    _958 = false;
                                }
                                bool _966;
                                if (_958)
                                {
                                    bool4 _961 = isnan(Settings.previousPoint);
                                    bool4 _962 = isinf(Settings.previousPoint);
                                    _966 = all(not(bool4(_961.x || _962.x, _961.y || _962.y, _961.z || _962.z, _961.w || _962.w)));
                                }
                                else
                                {
                                    _966 = false;
                                }
                                bool _974;
                                if (_966)
                                {
                                    bool4 _969 = isnan(Settings.previousNormal);
                                    bool4 _970 = isinf(Settings.previousNormal);
                                    _974 = all(not(bool4(_969.x || _970.x, _969.y || _970.y, _969.z || _970.z, _969.w || _970.w)));
                                }
                                else
                                {
                                    _974 = false;
                                }
                                bool _981;
                                if (_974)
                                {
                                    _981 = all(abs(Settings.previousPoint.xyz) <= float3(999999995904.0));
                                }
                                else
                                {
                                    _981 = false;
                                }
                                bool _987;
                                if (_981)
                                {
                                    _987 = all(abs(Settings.previousNormal.xyz) <= float3(999999995904.0));
                                }
                                else
                                {
                                    _987 = false;
                                }
                                bool _994;
                                if (_987)
                                {
                                    _994 = !(isnan(_947) || isinf(_947));
                                }
                                else
                                {
                                    _994 = false;
                                }
                                bool _998;
                                if (_994)
                                {
                                    _998 = _947 > 9.9999996826552253889678874634872e-21;
                                }
                                else
                                {
                                    _998 = false;
                                }
                                if (!_998)
                                {
                                    _1017 = false;
                                    _1018 = true;
                                    break;
                                }
                                uint _932 = _931 + 1u;
                                _931 = _932;
                                continue;
                            }
                            _184[_931] = geometry._m0[(Settings.previousMapping + (_183[_931] * 4u)) >> 2u];
                            if (_184[_931] >= Settings.oldTriangles)
                            {
                                _1017 = false;
                                _1018 = true;
                                break;
                            }
                            uint _932 = _931 + 1u;
                            _931 = _932;
                            continue;
                        }
                        else
                        {
                            _1017 = _797;
                            _1018 = false;
                            break;
                        }
                    }
                    if (_1018)
                    {
                        _1066 = _836;
                        _1067 = _848;
                        _1068 = _1017;
                        break;
                    }
                    uint _1064;
                    float3 _1065;
                    if ((_833 >> 8u) == 1u)
                    {
                        uint _1056 = (Settings.previousMapping + (_836 * 4u)) >> 2u;
                        if (geometry._m0[_1056] >= Settings.oldTriangles)
                        {
                            _1066 = geometry._m0[_1056];
                            _1067 = _848;
                            _1068 = false;
                            break;
                        }
                        _1064 = geometry._m0[_1056];
                        _1065 = _848;
                    }
                    else
                    {
                        _1064 = _836;
                        _1065 = fast::normalize(((Settings.previousCube[0].xyz * dot(Settings.currentCube[0].xyz, _848)) + (Settings.previousCube[1].xyz * dot(Settings.currentCube[1].xyz, _848))) + (Settings.previousCube[2].xyz * dot(Settings.currentCube[2].xyz, _848)));
                    }
                    _1066 = _1064;
                    _1067 = _1065;
                    _1068 = true;
                    break;
                } while(false);
                bool _8867;
                bool _8868;
                bool _8869;
                float4 _8870;
                bool _8871;
                bool _8872;
                bool _8873;
                bool _8874;
                float2 _8875;
                bool _8876;
                bool _8877;
                bool _8878;
                bool _8879;
                bool _8880;
                uint _8881;
                float3 _8882;
                if (_1068)
                {
                    float4 _1152;
                    float4 _1153;
                    float4 _1154;
                    do
                    {
                        if (_296)
                        {
                            _1152 = float4(Settings.previousPoint.xyz, 2.0);
                            _1153 = float4(Settings.previousNormal.xyz, 2.0);
                            _1154 = float4(0.0, 0.0, 0.0, 2.0);
                            break;
                        }
                        uint _1090 = Settings.previousVertices + (_241 * 48u);
                        uint _1091 = _1090 >> 2u;
                        float4 _1104 = as_type<float4>(uint4(geometry._m0[_1091], geometry._m0[_1091 + 1u], geometry._m0[_1091 + 2u], geometry._m0[_1091 + 3u]));
                        uint _1106 = (_1090 + 16u) >> 2u;
                        float4 _1119 = as_type<float4>(uint4(geometry._m0[_1106], geometry._m0[_1106 + 1u], geometry._m0[_1106 + 2u], geometry._m0[_1106 + 3u]));
                        uint _1121 = (_1090 + 32u) >> 2u;
                        float4 _1134 = as_type<float4>(uint4(geometry._m0[_1121], geometry._m0[_1121 + 1u], geometry._m0[_1121 + 2u], geometry._m0[_1121 + 3u]));
                        float _1135 = _1104.w;
                        bool _1141;
                        if ((isunordered(_1135, 1.0) || _1135 == 1.0))
                        {
                            _1141 = _1119.w != 1.0;
                        }
                        else
                        {
                            _1141 = true;
                        }
                        bool _1147;
                        if (!_1141)
                        {
                            _1147 = _1134.w != 1.0;
                        }
                        else
                        {
                            _1147 = true;
                        }
                        bool4 _1148 = bool4(_1147);
                        _1152 = select(_1104, float4(0.0), _1148);
                        _1153 = select(_1119, float4(0.0), _1148);
                        _1154 = select(_1134, float4(0.0), _1148);
                        break;
                    } while(false);
                    _206[0u].c = float4(0.0);
                    _206[0u].b = float4(0.0);
                    _206[0u].a = float4(0.0);
                    _206[1u].c = float4(0.0);
                    _206[1u].b = float4(0.0);
                    _206[1u].a = float4(0.0);
                    _206[2u].c = float4(0.0);
                    _206[2u].b = float4(0.0);
                    _206[2u].a = float4(0.0);
                    _206[3u].c = float4(0.0);
                    _206[3u].b = float4(0.0);
                    _206[3u].a = float4(0.0);
                    for (uint _1168 = 0u; _1168 < _929; _1168++)
                    {
                        float4 _1258;
                        float4 _1259;
                        float4 _1260;
                        do
                        {
                            if (_185[_1168] == 4294967294u)
                            {
                                _1258 = float4(Settings.previousPoint.xyz, 2.0);
                                _1259 = float4(Settings.previousNormal.xyz, 2.0);
                                _1260 = float4(0.0, 0.0, 0.0, 2.0);
                                break;
                            }
                            uint _1196 = Settings.previousVertices + (_185[_1168] * 48u);
                            uint _1197 = _1196 >> 2u;
                            float4 _1210 = as_type<float4>(uint4(geometry._m0[_1197], geometry._m0[_1197 + 1u], geometry._m0[_1197 + 2u], geometry._m0[_1197 + 3u]));
                            uint _1212 = (_1196 + 16u) >> 2u;
                            float4 _1225 = as_type<float4>(uint4(geometry._m0[_1212], geometry._m0[_1212 + 1u], geometry._m0[_1212 + 2u], geometry._m0[_1212 + 3u]));
                            uint _1227 = (_1196 + 32u) >> 2u;
                            float4 _1240 = as_type<float4>(uint4(geometry._m0[_1227], geometry._m0[_1227 + 1u], geometry._m0[_1227 + 2u], geometry._m0[_1227 + 3u]));
                            float _1241 = _1210.w;
                            bool _1247;
                            if ((isunordered(_1241, 1.0) || _1241 == 1.0))
                            {
                                _1247 = _1225.w != 1.0;
                            }
                            else
                            {
                                _1247 = true;
                            }
                            bool _1253;
                            if (!_1247)
                            {
                                _1253 = _1240.w != 1.0;
                            }
                            else
                            {
                                _1253 = true;
                            }
                            bool4 _1254 = bool4(_1253);
                            _1258 = select(_1210, float4(0.0), _1254);
                            _1259 = select(_1225, float4(0.0), _1254);
                            _1260 = select(_1240, float4(0.0), _1254);
                            break;
                        } while(false);
                        _206[_1168] = ReflectionSpecularPlane{ _1258, _1259, _1260 };
                    }
                    float _1267 = float(Settings.previousWidth);
                    float _1270 = float(Settings.previousHeight);
                    float4 _1275 = float4(_1267, _1270, Settings.clip.xy);
                    float4 _1283;
                    if (_296)
                    {
                        _1283 = float4(0.0);
                    }
                    else
                    {
                        _1283 = float4(Settings.roughness, float(_803), 0.0, 0.0);
                    }
                    bool _1289 = (_833 >> 8u) == 1u;
                    float4 _1521;
                    if (_1289)
                    {
                        float4 _1374;
                        float4 _1375;
                        float4 _1376;
                        do
                        {
                            if (_836 == 4294967294u)
                            {
                                _1374 = float4(Settings.previousPoint.xyz, 2.0);
                                _1375 = float4(Settings.previousNormal.xyz, 2.0);
                                _1376 = float4(0.0, 0.0, 0.0, 2.0);
                                break;
                            }
                            uint _1312 = Settings.previousVertices + (_836 * 48u);
                            uint _1313 = _1312 >> 2u;
                            float4 _1326 = as_type<float4>(uint4(geometry._m0[_1313], geometry._m0[_1313 + 1u], geometry._m0[_1313 + 2u], geometry._m0[_1313 + 3u]));
                            uint _1328 = (_1312 + 16u) >> 2u;
                            float4 _1341 = as_type<float4>(uint4(geometry._m0[_1328], geometry._m0[_1328 + 1u], geometry._m0[_1328 + 2u], geometry._m0[_1328 + 3u]));
                            uint _1343 = (_1312 + 32u) >> 2u;
                            float4 _1356 = as_type<float4>(uint4(geometry._m0[_1343], geometry._m0[_1343 + 1u], geometry._m0[_1343 + 2u], geometry._m0[_1343 + 3u]));
                            float _1357 = _1326.w;
                            bool _1363;
                            if ((isunordered(_1357, 1.0) || _1357 == 1.0))
                            {
                                _1363 = _1341.w != 1.0;
                            }
                            else
                            {
                                _1363 = true;
                            }
                            bool _1369;
                            if (!_1363)
                            {
                                _1369 = _1356.w != 1.0;
                            }
                            else
                            {
                                _1369 = true;
                            }
                            bool4 _1370 = bool4(_1369);
                            _1374 = select(_1326, float4(0.0), _1370);
                            _1375 = select(_1341, float4(0.0), _1370);
                            _1376 = select(_1356, float4(0.0), _1370);
                            break;
                        } while(false);
                        bool _1499;
                        do
                        {
                            bool _1385;
                            if (_1374.w == 2.0)
                            {
                                _1385 = _1375.w == 2.0;
                            }
                            else
                            {
                                _1385 = false;
                            }
                            bool _1390;
                            if (_1385)
                            {
                                _1390 = _1376.w == 2.0;
                            }
                            else
                            {
                                _1390 = false;
                            }
                            bool _1406;
                            if (!_1390)
                            {
                                bool _1399;
                                if ((isunordered(_1374.w, 1.0) || _1374.w == 1.0))
                                {
                                    _1399 = _1375.w != 1.0;
                                }
                                else
                                {
                                    _1399 = true;
                                }
                                bool _1405;
                                if (!_1399)
                                {
                                    _1405 = _1376.w != 1.0;
                                }
                                else
                                {
                                    _1405 = true;
                                }
                                _1406 = _1405;
                            }
                            else
                            {
                                _1406 = false;
                            }
                            bool _1416;
                            if (!_1406)
                            {
                                bool4 _1410 = isnan(_1374);
                                bool4 _1411 = isinf(_1374);
                                _1416 = !all(not(bool4(_1410.x || _1411.x, _1410.y || _1411.y, _1410.z || _1411.z, _1410.w || _1411.w)));
                            }
                            else
                            {
                                _1416 = true;
                            }
                            bool _1426;
                            if (!_1416)
                            {
                                bool4 _1420 = isnan(_1375);
                                bool4 _1421 = isinf(_1375);
                                _1426 = !all(not(bool4(_1420.x || _1421.x, _1420.y || _1421.y, _1420.z || _1421.z, _1420.w || _1421.w)));
                            }
                            else
                            {
                                _1426 = true;
                            }
                            bool _1436;
                            if (!_1426)
                            {
                                bool4 _1430 = isnan(_1376);
                                bool4 _1431 = isinf(_1376);
                                _1436 = !all(not(bool4(_1430.x || _1431.x, _1430.y || _1431.y, _1430.z || _1431.z, _1430.w || _1431.w)));
                            }
                            else
                            {
                                _1436 = true;
                            }
                            bool _1444;
                            if (!_1436)
                            {
                                _1444 = any(abs(_1374.xyz) > float3(999999995904.0));
                            }
                            else
                            {
                                _1444 = true;
                            }
                            bool _1452;
                            if (!_1444)
                            {
                                _1452 = any(abs(_1375.xyz) > float3(999999995904.0));
                            }
                            else
                            {
                                _1452 = true;
                            }
                            bool _1460;
                            if (!_1452)
                            {
                                _1460 = any(abs(_1376.xyz) > float3(999999995904.0));
                            }
                            else
                            {
                                _1460 = true;
                            }
                            if (_1460)
                            {
                                _1499 = false;
                                break;
                            }
                            bool _1468;
                            if (_1390)
                            {
                                _1468 = any(_1376.xyz != float3(0.0));
                            }
                            else
                            {
                                _1468 = false;
                            }
                            if (_1468)
                            {
                                _1499 = false;
                                break;
                            }
                            float3 _1481;
                            if (_1390)
                            {
                                _1481 = _1375.xyz;
                            }
                            else
                            {
                                _1481 = cross(_1375.xyz - _1374.xyz, _1376.xyz - _1374.xyz);
                            }
                            float _1482 = dot(_1481, _1481);
                            bool3 _1483 = isnan(_1481);
                            bool3 _1484 = isinf(_1481);
                            bool _1494;
                            if (all(not(bool3(_1483.x || _1484.x, _1483.y || _1484.y, _1483.z || _1484.z))))
                            {
                                _1494 = !(isnan(_1482) || isinf(_1482));
                            }
                            else
                            {
                                _1494 = false;
                            }
                            bool _1498;
                            if (_1494)
                            {
                                _1498 = _1482 > 9.9999996826552253889678874634872e-21;
                            }
                            else
                            {
                                _1498 = false;
                            }
                            _1499 = _1498;
                            break;
                        } while(false);
                        float4 _1520;
                        if (!_1499)
                        {
                            _1520 = float4(0.0);
                        }
                        else
                        {
                            float _1505 = _848.x;
                            float _1507 = _848.y;
                            _1520 = float4(((_1374.xyz * ((1.0 - _1505) - _1507)) + (_1375.xyz * _1505)) + (_1376.xyz * _1507), 1.0);
                        }
                        _1521 = _1520;
                    }
                    else
                    {
                        _1521 = float4(_1067, 0.0);
                    }
                    spvUnsafeArray<ReflectionSpecularPlane, 4> _207 = _206;
                    bool _7318;
                    bool _7319;
                    bool _7320;
                    bool _7321;
                    float2 _7322;
                    bool _7323;
                    bool _7324;
                    bool _7325;
                    float4 _7326;
                    do
                    {
                        bool _1525 = _929 <= 4u;
                        bool _1763;
                        if (_1525)
                        {
                            bool _1761;
                            do
                            {
                                bool _1536;
                                if (_1152.w == 2.0)
                                {
                                    _1536 = _1153.w == 2.0;
                                }
                                else
                                {
                                    _1536 = false;
                                }
                                bool _1541;
                                if (_1536)
                                {
                                    _1541 = _1154.w == 2.0;
                                }
                                else
                                {
                                    _1541 = false;
                                }
                                bool _1557;
                                if (!_1541)
                                {
                                    bool _1550;
                                    if ((isunordered(_1152.w, 1.0) || _1152.w == 1.0))
                                    {
                                        _1550 = _1153.w != 1.0;
                                    }
                                    else
                                    {
                                        _1550 = true;
                                    }
                                    bool _1556;
                                    if (!_1550)
                                    {
                                        _1556 = _1154.w != 1.0;
                                    }
                                    else
                                    {
                                        _1556 = true;
                                    }
                                    _1557 = _1556;
                                }
                                else
                                {
                                    _1557 = false;
                                }
                                bool _1567;
                                if (!_1557)
                                {
                                    bool4 _1561 = isnan(_1152);
                                    bool4 _1562 = isinf(_1152);
                                    _1567 = !all(not(bool4(_1561.x || _1562.x, _1561.y || _1562.y, _1561.z || _1562.z, _1561.w || _1562.w)));
                                }
                                else
                                {
                                    _1567 = true;
                                }
                                bool _1577;
                                if (!_1567)
                                {
                                    bool4 _1571 = isnan(_1153);
                                    bool4 _1572 = isinf(_1153);
                                    _1577 = !all(not(bool4(_1571.x || _1572.x, _1571.y || _1572.y, _1571.z || _1572.z, _1571.w || _1572.w)));
                                }
                                else
                                {
                                    _1577 = true;
                                }
                                bool _1587;
                                if (!_1577)
                                {
                                    bool4 _1581 = isnan(_1154);
                                    bool4 _1582 = isinf(_1154);
                                    _1587 = !all(not(bool4(_1581.x || _1582.x, _1581.y || _1582.y, _1581.z || _1582.z, _1581.w || _1582.w)));
                                }
                                else
                                {
                                    _1587 = true;
                                }
                                bool _1597;
                                if (!_1587)
                                {
                                    bool4 _1591 = isnan(Settings.projection);
                                    bool4 _1592 = isinf(Settings.projection);
                                    _1597 = !all(not(bool4(_1591.x || _1592.x, _1591.y || _1592.y, _1591.z || _1592.z, _1591.w || _1592.w)));
                                }
                                else
                                {
                                    _1597 = true;
                                }
                                bool _1607;
                                if (!_1597)
                                {
                                    bool4 _1601 = isnan(_1275);
                                    bool4 _1602 = isinf(_1275);
                                    _1607 = !all(not(bool4(_1601.x || _1602.x, _1601.y || _1602.y, _1601.z || _1602.z, _1601.w || _1602.w)));
                                }
                                else
                                {
                                    _1607 = true;
                                }
                                bool _1617;
                                if (!_1607)
                                {
                                    bool4 _1611 = isnan(_1283);
                                    bool4 _1612 = isinf(_1283);
                                    _1617 = !all(not(bool4(_1611.x || _1612.x, _1611.y || _1612.y, _1611.z || _1612.z, _1611.w || _1612.w)));
                                }
                                else
                                {
                                    _1617 = true;
                                }
                                bool _1625;
                                if (!_1617)
                                {
                                    _1625 = any(abs(_1152.xyz) > float3(999999995904.0));
                                }
                                else
                                {
                                    _1625 = true;
                                }
                                bool _1633;
                                if (!_1625)
                                {
                                    _1633 = any(abs(_1153.xyz) > float3(999999995904.0));
                                }
                                else
                                {
                                    _1633 = true;
                                }
                                bool _1641;
                                if (!_1633)
                                {
                                    _1641 = any(abs(_1154.xyz) > float3(999999995904.0));
                                }
                                else
                                {
                                    _1641 = true;
                                }
                                bool _1648;
                                if (!_1641)
                                {
                                    _1648 = any(Settings.projection.xy <= float2(0.0));
                                }
                                else
                                {
                                    _1648 = true;
                                }
                                bool _1655;
                                if (!_1648)
                                {
                                    _1655 = any(abs(Settings.projection) > float4(999999995904.0));
                                }
                                else
                                {
                                    _1655 = true;
                                }
                                bool _1662;
                                if (!_1655)
                                {
                                    _1662 = any(_1275.xy < float2(1.0));
                                }
                                else
                                {
                                    _1662 = true;
                                }
                                bool _1669;
                                if (!_1662)
                                {
                                    _1669 = any(_1275.xy > float2(16384.0));
                                }
                                else
                                {
                                    _1669 = true;
                                }
                                bool _1674;
                                if (!_1669)
                                {
                                    _1674 = Settings.clip.x <= 0.0;
                                }
                                else
                                {
                                    _1674 = true;
                                }
                                bool _1679;
                                if (!_1674)
                                {
                                    _1679 = Settings.clip.y <= Settings.clip.x;
                                }
                                else
                                {
                                    _1679 = true;
                                }
                                bool _1684;
                                if (!_1679)
                                {
                                    _1684 = Settings.clip.y > 999999995904.0;
                                }
                                else
                                {
                                    _1684 = true;
                                }
                                bool _1690;
                                if (!_1684)
                                {
                                    _1690 = _1283.x < 0.0;
                                }
                                else
                                {
                                    _1690 = true;
                                }
                                bool _1696;
                                if (!_1690)
                                {
                                    _1696 = _1283.x > 1.0;
                                }
                                else
                                {
                                    _1696 = true;
                                }
                                bool _1702;
                                if (!_1696)
                                {
                                    _1702 = _1283.y < 0.0;
                                }
                                else
                                {
                                    _1702 = true;
                                }
                                bool _1708;
                                if (!_1702)
                                {
                                    _1708 = _1283.y > 7.0;
                                }
                                else
                                {
                                    _1708 = true;
                                }
                                bool _1715;
                                if (!_1708)
                                {
                                    _1715 = floor(_1283.y) != _1283.y;
                                }
                                else
                                {
                                    _1715 = true;
                                }
                                bool _1722;
                                if (!_1715)
                                {
                                    _1722 = any(_1283.zw != float2(0.0));
                                }
                                else
                                {
                                    _1722 = true;
                                }
                                if (_1722)
                                {
                                    _1761 = false;
                                    break;
                                }
                                bool _1730;
                                if (_1541)
                                {
                                    _1730 = any(_1154.xyz != float3(0.0));
                                }
                                else
                                {
                                    _1730 = false;
                                }
                                if (_1730)
                                {
                                    _1761 = false;
                                    break;
                                }
                                float3 _1743;
                                if (_1541)
                                {
                                    _1743 = _1153.xyz;
                                }
                                else
                                {
                                    _1743 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                }
                                float _1744 = dot(_1743, _1743);
                                bool3 _1745 = isnan(_1743);
                                bool3 _1746 = isinf(_1743);
                                bool _1756;
                                if (all(not(bool3(_1745.x || _1746.x, _1745.y || _1746.y, _1745.z || _1746.z))))
                                {
                                    _1756 = !(isnan(_1744) || isinf(_1744));
                                }
                                else
                                {
                                    _1756 = false;
                                }
                                bool _1760;
                                if (_1756)
                                {
                                    _1760 = _1744 > 9.9999996826552253889678874634872e-21;
                                }
                                else
                                {
                                    _1760 = false;
                                }
                                _1761 = _1760;
                                break;
                            } while(false);
                            _1763 = !_1761;
                        }
                        else
                        {
                            _1763 = true;
                        }
                        bool _1773;
                        if (!_1763)
                        {
                            bool4 _1767 = isnan(_1521);
                            bool4 _1768 = isinf(_1521);
                            _1773 = !all(not(bool4(_1767.x || _1768.x, _1767.y || _1768.y, _1767.z || _1768.z, _1767.w || _1768.w)));
                        }
                        else
                        {
                            _1773 = true;
                        }
                        bool _1783;
                        if (!_1773)
                        {
                            bool _1782;
                            if (_1521.w != 0.0)
                            {
                                _1782 = _1521.w != 1.0;
                            }
                            else
                            {
                                _1782 = false;
                            }
                            _1783 = _1782;
                        }
                        else
                        {
                            _1783 = true;
                        }
                        bool _1791;
                        if (!_1783)
                        {
                            _1791 = any(abs(_1521.xyz) > float3(999999995904.0));
                        }
                        else
                        {
                            _1791 = true;
                        }
                        bool _1803;
                        if (!_1791)
                        {
                            bool _1802;
                            if (_1521.w == 0.0)
                            {
                                _1802 = dot(_1521.xyz, _1521.xyz) <= 9.9999996826552253889678874634872e-21;
                            }
                            else
                            {
                                _1802 = false;
                            }
                            _1803 = _1802;
                        }
                        else
                        {
                            _1803 = true;
                        }
                        if (_1803)
                        {
                            _7318 = _781;
                            _7319 = _783;
                            _7320 = _785;
                            _7321 = _787;
                            _7322 = _789;
                            _7323 = _791;
                            _7324 = _793;
                            _7325 = _795;
                            _7326 = float4(0.0);
                            break;
                        }
                        float4 _1944;
                        bool _1945;
                        uint _1807 = 0u;
                        for (;;)
                        {
                            if (_1807 < _929)
                            {
                                bool _1940;
                                do
                                {
                                    bool _1826;
                                    if (_207[_1807].a.w == 2.0)
                                    {
                                        _1826 = _207[_1807].b.w == 2.0;
                                    }
                                    else
                                    {
                                        _1826 = false;
                                    }
                                    bool _1831;
                                    if (_1826)
                                    {
                                        _1831 = _207[_1807].c.w == 2.0;
                                    }
                                    else
                                    {
                                        _1831 = false;
                                    }
                                    bool _1847;
                                    if (!_1831)
                                    {
                                        bool _1840;
                                        if ((isunordered(_207[_1807].a.w, 1.0) || _207[_1807].a.w == 1.0))
                                        {
                                            _1840 = _207[_1807].b.w != 1.0;
                                        }
                                        else
                                        {
                                            _1840 = true;
                                        }
                                        bool _1846;
                                        if (!_1840)
                                        {
                                            _1846 = _207[_1807].c.w != 1.0;
                                        }
                                        else
                                        {
                                            _1846 = true;
                                        }
                                        _1847 = _1846;
                                    }
                                    else
                                    {
                                        _1847 = false;
                                    }
                                    bool _1857;
                                    if (!_1847)
                                    {
                                        bool4 _1851 = isnan(_207[_1807].a);
                                        bool4 _1852 = isinf(_207[_1807].a);
                                        _1857 = !all(not(bool4(_1851.x || _1852.x, _1851.y || _1852.y, _1851.z || _1852.z, _1851.w || _1852.w)));
                                    }
                                    else
                                    {
                                        _1857 = true;
                                    }
                                    bool _1867;
                                    if (!_1857)
                                    {
                                        bool4 _1861 = isnan(_207[_1807].b);
                                        bool4 _1862 = isinf(_207[_1807].b);
                                        _1867 = !all(not(bool4(_1861.x || _1862.x, _1861.y || _1862.y, _1861.z || _1862.z, _1861.w || _1862.w)));
                                    }
                                    else
                                    {
                                        _1867 = true;
                                    }
                                    bool _1877;
                                    if (!_1867)
                                    {
                                        bool4 _1871 = isnan(_207[_1807].c);
                                        bool4 _1872 = isinf(_207[_1807].c);
                                        _1877 = !all(not(bool4(_1871.x || _1872.x, _1871.y || _1872.y, _1871.z || _1872.z, _1871.w || _1872.w)));
                                    }
                                    else
                                    {
                                        _1877 = true;
                                    }
                                    bool _1885;
                                    if (!_1877)
                                    {
                                        _1885 = any(abs(_207[_1807].a.xyz) > float3(999999995904.0));
                                    }
                                    else
                                    {
                                        _1885 = true;
                                    }
                                    bool _1893;
                                    if (!_1885)
                                    {
                                        _1893 = any(abs(_207[_1807].b.xyz) > float3(999999995904.0));
                                    }
                                    else
                                    {
                                        _1893 = true;
                                    }
                                    bool _1901;
                                    if (!_1893)
                                    {
                                        _1901 = any(abs(_207[_1807].c.xyz) > float3(999999995904.0));
                                    }
                                    else
                                    {
                                        _1901 = true;
                                    }
                                    if (_1901)
                                    {
                                        _1940 = false;
                                        break;
                                    }
                                    bool _1909;
                                    if (_1831)
                                    {
                                        _1909 = any(_207[_1807].c.xyz != float3(0.0));
                                    }
                                    else
                                    {
                                        _1909 = false;
                                    }
                                    if (_1909)
                                    {
                                        _1940 = false;
                                        break;
                                    }
                                    float3 _1922;
                                    if (_1831)
                                    {
                                        _1922 = _207[_1807].b.xyz;
                                    }
                                    else
                                    {
                                        _1922 = cross(_207[_1807].b.xyz - _207[_1807].a.xyz, _207[_1807].c.xyz - _207[_1807].a.xyz);
                                    }
                                    float _1923 = dot(_1922, _1922);
                                    bool3 _1924 = isnan(_1922);
                                    bool3 _1925 = isinf(_1922);
                                    bool _1935;
                                    if (all(not(bool3(_1924.x || _1925.x, _1924.y || _1925.y, _1924.z || _1925.z))))
                                    {
                                        _1935 = !(isnan(_1923) || isinf(_1923));
                                    }
                                    else
                                    {
                                        _1935 = false;
                                    }
                                    bool _1939;
                                    if (_1935)
                                    {
                                        _1939 = _1923 > 9.9999996826552253889678874634872e-21;
                                    }
                                    else
                                    {
                                        _1939 = false;
                                    }
                                    _1940 = _1939;
                                    break;
                                } while(false);
                                if (!_1940)
                                {
                                    _1944 = float4(0.0);
                                    _1945 = true;
                                    break;
                                }
                                _1807++;
                                continue;
                            }
                            else
                            {
                                _1944 = _779;
                                _1945 = false;
                                break;
                            }
                        }
                        if (_1945)
                        {
                            _7318 = _781;
                            _7319 = _783;
                            _7320 = _785;
                            _7321 = _787;
                            _7322 = _789;
                            _7323 = _791;
                            _7324 = _793;
                            _7325 = _795;
                            _7326 = _1944;
                            break;
                        }
                        float3 _1949;
                        _1949 = _1521.xyz;
                        float3 _1950;
                        uint _1953;
                        for (uint _1952 = _929; _1952 > 0u; _1949 = _1950, _1952 = _1953)
                        {
                            _1953 = _1952 - 1u;
                            bool _1968;
                            if (_207[_1953].a.w == 2.0)
                            {
                                _1968 = _207[_1953].b.w == 2.0;
                            }
                            else
                            {
                                _1968 = false;
                            }
                            bool _1973;
                            if (_1968)
                            {
                                _1973 = _207[_1953].c.w == 2.0;
                            }
                            else
                            {
                                _1973 = false;
                            }
                            float3 _1984;
                            if (_1973)
                            {
                                _1984 = _207[_1953].b.xyz;
                            }
                            else
                            {
                                _1984 = cross(_207[_1953].b.xyz - _207[_1953].a.xyz, _207[_1953].c.xyz - _207[_1953].a.xyz);
                            }
                            float3 _1985 = fast::normalize(_1984);
                            float3 _1993;
                            if (_1521.w == 1.0)
                            {
                                _1993 = _207[_1953].a.xyz;
                            }
                            else
                            {
                                _1993 = float3(0.0);
                            }
                            _1950 = _1949 - ((_1985 * 2.0) * dot(_1949 - _1993, _1985));
                        }
                        bool _1998 = _1152.w == 2.0;
                        bool _2003;
                        if (_1998)
                        {
                            _2003 = _1153.w == 2.0;
                        }
                        else
                        {
                            _2003 = false;
                        }
                        bool _2008;
                        if (_2003)
                        {
                            _2008 = _1154.w == 2.0;
                        }
                        else
                        {
                            _2008 = false;
                        }
                        float3 _2019;
                        if (_2008)
                        {
                            _2019 = _1153.xyz;
                        }
                        else
                        {
                            _2019 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                        }
                        float3 _2020 = fast::normalize(_2019);
                        bool _2023 = _1521.w == 1.0;
                        float3 _2028;
                        if (_2023)
                        {
                            _2028 = _1152.xyz;
                        }
                        else
                        {
                            _2028 = float3(0.0);
                        }
                        float3 _2032 = _1949 - ((_2020 * 2.0) * dot(_1949 - _2028, _2020));
                        float2 _2033 = _1275.xy;
                        bool3 _2035 = isnan(_2032);
                        bool3 _2036 = isinf(_2032);
                        bool _2044;
                        if (all(not(bool3(_2035.x || _2036.x, _2035.y || _2036.y, _2035.z || _2036.z))))
                        {
                            _2044 = _2032.z > 0.0;
                        }
                        else
                        {
                            _2044 = false;
                        }
                        float2 _2057;
                        if (_2044)
                        {
                            _2057 = fast::clamp(((Settings.projection.xy * _2032.xy) / float2(_2032.z)) + Settings.projection.zw, float2(0.0), _2033 - float2(0.001000000047497451305389404296875));
                        }
                        else
                        {
                            _2057 = _2033 * 0.5;
                        }
                        float _2060 = precise::max(8.0, precise::min(_1267, _1270) * 0.125);
                        float2 _2062;
                        float2 _2067;
                        _2062 = _2057;
                        _2067 = _789;
                        float2 _2063;
                        bool _2066;
                        float2 _2068;
                        bool _2070;
                        bool _2072;
                        bool _2074;
                        float2 _5783;
                        bool _5784;
                        bool _5785;
                        float2 _5786;
                        bool _5787;
                        bool _5788;
                        bool _5789;
                        float4 _5790;
                        bool _5791;
                        bool _2065 = _787;
                        bool _2069 = _791;
                        bool _2071 = _793;
                        bool _2073 = _795;
                        uint _2075 = 0u;
                        for (;;)
                        {
                            if (_2075 < 16u)
                            {
                                float2 _2772;
                                bool _2773;
                                do
                                {
                                    spvUnsafeArray<ReflectionSpecularPlane, 4> _205 = _206;
                                    float3 _2718;
                                    float _2719;
                                    float3 _2720;
                                    do
                                    {
                                        bool _2317;
                                        if (_1525)
                                        {
                                            bool _2315;
                                            do
                                            {
                                                bool _2092;
                                                if (_1998)
                                                {
                                                    _2092 = _1153.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2092 = false;
                                                }
                                                bool _2097;
                                                if (_2092)
                                                {
                                                    _2097 = _1154.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2097 = false;
                                                }
                                                bool _2113;
                                                if (!_2097)
                                                {
                                                    bool _2106;
                                                    if ((isunordered(_1152.w, 1.0) || _1152.w == 1.0))
                                                    {
                                                        _2106 = _1153.w != 1.0;
                                                    }
                                                    else
                                                    {
                                                        _2106 = true;
                                                    }
                                                    bool _2112;
                                                    if (!_2106)
                                                    {
                                                        _2112 = _1154.w != 1.0;
                                                    }
                                                    else
                                                    {
                                                        _2112 = true;
                                                    }
                                                    _2113 = _2112;
                                                }
                                                else
                                                {
                                                    _2113 = false;
                                                }
                                                bool _2123;
                                                if (!_2113)
                                                {
                                                    bool4 _2117 = isnan(_1152);
                                                    bool4 _2118 = isinf(_1152);
                                                    _2123 = !all(not(bool4(_2117.x || _2118.x, _2117.y || _2118.y, _2117.z || _2118.z, _2117.w || _2118.w)));
                                                }
                                                else
                                                {
                                                    _2123 = true;
                                                }
                                                bool _2133;
                                                if (!_2123)
                                                {
                                                    bool4 _2127 = isnan(_1153);
                                                    bool4 _2128 = isinf(_1153);
                                                    _2133 = !all(not(bool4(_2127.x || _2128.x, _2127.y || _2128.y, _2127.z || _2128.z, _2127.w || _2128.w)));
                                                }
                                                else
                                                {
                                                    _2133 = true;
                                                }
                                                bool _2143;
                                                if (!_2133)
                                                {
                                                    bool4 _2137 = isnan(_1154);
                                                    bool4 _2138 = isinf(_1154);
                                                    _2143 = !all(not(bool4(_2137.x || _2138.x, _2137.y || _2138.y, _2137.z || _2138.z, _2137.w || _2138.w)));
                                                }
                                                else
                                                {
                                                    _2143 = true;
                                                }
                                                bool _2153;
                                                if (!_2143)
                                                {
                                                    bool4 _2147 = isnan(Settings.projection);
                                                    bool4 _2148 = isinf(Settings.projection);
                                                    _2153 = !all(not(bool4(_2147.x || _2148.x, _2147.y || _2148.y, _2147.z || _2148.z, _2147.w || _2148.w)));
                                                }
                                                else
                                                {
                                                    _2153 = true;
                                                }
                                                bool _2163;
                                                if (!_2153)
                                                {
                                                    bool4 _2157 = isnan(_1275);
                                                    bool4 _2158 = isinf(_1275);
                                                    _2163 = !all(not(bool4(_2157.x || _2158.x, _2157.y || _2158.y, _2157.z || _2158.z, _2157.w || _2158.w)));
                                                }
                                                else
                                                {
                                                    _2163 = true;
                                                }
                                                bool _2173;
                                                if (!_2163)
                                                {
                                                    bool4 _2167 = isnan(_1283);
                                                    bool4 _2168 = isinf(_1283);
                                                    _2173 = !all(not(bool4(_2167.x || _2168.x, _2167.y || _2168.y, _2167.z || _2168.z, _2167.w || _2168.w)));
                                                }
                                                else
                                                {
                                                    _2173 = true;
                                                }
                                                bool _2181;
                                                if (!_2173)
                                                {
                                                    _2181 = any(abs(_1152.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _2181 = true;
                                                }
                                                bool _2189;
                                                if (!_2181)
                                                {
                                                    _2189 = any(abs(_1153.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _2189 = true;
                                                }
                                                bool _2197;
                                                if (!_2189)
                                                {
                                                    _2197 = any(abs(_1154.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _2197 = true;
                                                }
                                                bool _2204;
                                                if (!_2197)
                                                {
                                                    _2204 = any(Settings.projection.xy <= float2(0.0));
                                                }
                                                else
                                                {
                                                    _2204 = true;
                                                }
                                                bool _2211;
                                                if (!_2204)
                                                {
                                                    _2211 = any(abs(Settings.projection) > float4(999999995904.0));
                                                }
                                                else
                                                {
                                                    _2211 = true;
                                                }
                                                bool _2217;
                                                if (!_2211)
                                                {
                                                    _2217 = any(_2033 < float2(1.0));
                                                }
                                                else
                                                {
                                                    _2217 = true;
                                                }
                                                bool _2223;
                                                if (!_2217)
                                                {
                                                    _2223 = any(_2033 > float2(16384.0));
                                                }
                                                else
                                                {
                                                    _2223 = true;
                                                }
                                                bool _2228;
                                                if (!_2223)
                                                {
                                                    _2228 = Settings.clip.x <= 0.0;
                                                }
                                                else
                                                {
                                                    _2228 = true;
                                                }
                                                bool _2233;
                                                if (!_2228)
                                                {
                                                    _2233 = Settings.clip.y <= Settings.clip.x;
                                                }
                                                else
                                                {
                                                    _2233 = true;
                                                }
                                                bool _2238;
                                                if (!_2233)
                                                {
                                                    _2238 = Settings.clip.y > 999999995904.0;
                                                }
                                                else
                                                {
                                                    _2238 = true;
                                                }
                                                bool _2244;
                                                if (!_2238)
                                                {
                                                    _2244 = _1283.x < 0.0;
                                                }
                                                else
                                                {
                                                    _2244 = true;
                                                }
                                                bool _2250;
                                                if (!_2244)
                                                {
                                                    _2250 = _1283.x > 1.0;
                                                }
                                                else
                                                {
                                                    _2250 = true;
                                                }
                                                bool _2256;
                                                if (!_2250)
                                                {
                                                    _2256 = _1283.y < 0.0;
                                                }
                                                else
                                                {
                                                    _2256 = true;
                                                }
                                                bool _2262;
                                                if (!_2256)
                                                {
                                                    _2262 = _1283.y > 7.0;
                                                }
                                                else
                                                {
                                                    _2262 = true;
                                                }
                                                bool _2269;
                                                if (!_2262)
                                                {
                                                    _2269 = floor(_1283.y) != _1283.y;
                                                }
                                                else
                                                {
                                                    _2269 = true;
                                                }
                                                bool _2276;
                                                if (!_2269)
                                                {
                                                    _2276 = any(_1283.zw != float2(0.0));
                                                }
                                                else
                                                {
                                                    _2276 = true;
                                                }
                                                if (_2276)
                                                {
                                                    _2315 = false;
                                                    break;
                                                }
                                                bool _2284;
                                                if (_2097)
                                                {
                                                    _2284 = any(_1154.xyz != float3(0.0));
                                                }
                                                else
                                                {
                                                    _2284 = false;
                                                }
                                                if (_2284)
                                                {
                                                    _2315 = false;
                                                    break;
                                                }
                                                float3 _2297;
                                                if (_2097)
                                                {
                                                    _2297 = _1153.xyz;
                                                }
                                                else
                                                {
                                                    _2297 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                }
                                                float _2298 = dot(_2297, _2297);
                                                bool3 _2299 = isnan(_2297);
                                                bool3 _2300 = isinf(_2297);
                                                bool _2310;
                                                if (all(not(bool3(_2299.x || _2300.x, _2299.y || _2300.y, _2299.z || _2300.z))))
                                                {
                                                    _2310 = !(isnan(_2298) || isinf(_2298));
                                                }
                                                else
                                                {
                                                    _2310 = false;
                                                }
                                                bool _2314;
                                                if (_2310)
                                                {
                                                    _2314 = _2298 > 9.9999996826552253889678874634872e-21;
                                                }
                                                else
                                                {
                                                    _2314 = false;
                                                }
                                                _2315 = _2314;
                                                break;
                                            } while(false);
                                            _2317 = !_2315;
                                        }
                                        else
                                        {
                                            _2317 = true;
                                        }
                                        float3 _2475;
                                        float _2476;
                                        float3 _2477;
                                        bool _2478;
                                        if (!_2317)
                                        {
                                            float3 _2470;
                                            float _2471;
                                            float3 _2472;
                                            bool _2473;
                                            do
                                            {
                                                bool2 _2323 = isnan(_2062);
                                                bool2 _2324 = isinf(_2062);
                                                bool _2332;
                                                if (all(not(bool2(_2323.x || _2324.x, _2323.y || _2324.y))))
                                                {
                                                    _2332 = any(_2062 < float2(0.0));
                                                }
                                                else
                                                {
                                                    _2332 = true;
                                                }
                                                bool _2338;
                                                if (!_2332)
                                                {
                                                    _2338 = any(_2062 >= _2033);
                                                }
                                                else
                                                {
                                                    _2338 = true;
                                                }
                                                if (_2338)
                                                {
                                                    _2470 = float3(0.0);
                                                    _2471 = 0.0;
                                                    _2472 = float3(0.0);
                                                    _2473 = false;
                                                    break;
                                                }
                                                bool _2345;
                                                if (_1998)
                                                {
                                                    _2345 = _1153.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2345 = false;
                                                }
                                                bool _2350;
                                                if (_2345)
                                                {
                                                    _2350 = _1154.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2350 = false;
                                                }
                                                float3 _2361;
                                                if (_2350)
                                                {
                                                    _2361 = _1153.xyz;
                                                }
                                                else
                                                {
                                                    _2361 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                }
                                                float3 _2362 = fast::normalize(_2361);
                                                float3 _2370 = fast::normalize(float3((_2062 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                                float3 _2376;
                                                if (dot(_2362, _2370) > 0.0)
                                                {
                                                    _2376 = -_2362;
                                                }
                                                else
                                                {
                                                    _2376 = _2362;
                                                }
                                                float _2377 = dot(_2370, _2376);
                                                bool _2386;
                                                if (!(isnan(_2377) || isinf(_2377)))
                                                {
                                                    _2386 = abs(_2377) <= 9.9999999600419720025001879548654e-13;
                                                }
                                                else
                                                {
                                                    _2386 = true;
                                                }
                                                if (_2386)
                                                {
                                                    _2470 = float3(0.0);
                                                    _2471 = 0.0;
                                                    _2472 = float3(0.0);
                                                    _2473 = false;
                                                    break;
                                                }
                                                float _2391 = dot(_1152.xyz, _2376) / _2377;
                                                float _2393 = _2391 * _2370.z;
                                                bool _2401;
                                                if (!(isnan(_2391) || isinf(_2391)))
                                                {
                                                    _2401 = _2391 <= 0.0;
                                                }
                                                else
                                                {
                                                    _2401 = true;
                                                }
                                                bool _2406;
                                                if (!_2401)
                                                {
                                                    _2406 = _2393 < Settings.clip.x;
                                                }
                                                else
                                                {
                                                    _2406 = true;
                                                }
                                                bool _2411;
                                                if (!_2406)
                                                {
                                                    _2411 = _2393 > Settings.clip.y;
                                                }
                                                else
                                                {
                                                    _2411 = true;
                                                }
                                                if (_2411)
                                                {
                                                    _2470 = float3(0.0);
                                                    _2471 = 0.0;
                                                    _2472 = float3(0.0);
                                                    _2473 = false;
                                                    break;
                                                }
                                                bool _2418;
                                                if (_1998)
                                                {
                                                    _2418 = _1153.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2418 = false;
                                                }
                                                bool _2423;
                                                if (_2418)
                                                {
                                                    _2423 = _1154.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2423 = false;
                                                }
                                                float _2426 = precise::max(_2423 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _2391 * 9.9999997473787516355514526367188e-06);
                                                float3 _2429 = (_2370 * _2391) + (_2376 * _2426);
                                                float3 _2430 = reflect(_2370, _2376);
                                                uint _2433 = uint(_1283.y);
                                                float3 _2440 = fast::normalize(cross(_2430, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_2430.y) < 0.949999988079071044921875))));
                                                float3 _2452 = fast::normalize(_2430 + (((_2440 * _175[_2433].x) + (cross(_2430, _2440) * _175[_2433].y)) * (_1283.x * _1283.x)));
                                                float3 _2456 = select(_2452, _2430, bool3(dot(_2452, _2376) <= 0.0));
                                                bool3 _2457 = isnan(_2429);
                                                bool3 _2458 = isinf(_2429);
                                                bool _2469;
                                                if (all(not(bool3(_2457.x || _2458.x, _2457.y || _2458.y, _2457.z || _2458.z))))
                                                {
                                                    bool3 _2464 = isnan(_2456);
                                                    bool3 _2465 = isinf(_2456);
                                                    _2469 = all(not(bool3(_2464.x || _2465.x, _2464.y || _2465.y, _2464.z || _2465.z)));
                                                }
                                                else
                                                {
                                                    _2469 = false;
                                                }
                                                _2470 = _2456;
                                                _2471 = _2426;
                                                _2472 = _2429;
                                                _2473 = _2469;
                                                break;
                                            } while(false);
                                            _2475 = _2470;
                                            _2476 = _2471;
                                            _2477 = _2472;
                                            _2478 = !_2473;
                                        }
                                        else
                                        {
                                            _2475 = float3(0.0);
                                            _2476 = 0.0;
                                            _2477 = float3(0.0);
                                            _2478 = true;
                                        }
                                        if (_2478)
                                        {
                                            _2718 = _2475;
                                            _2719 = _2476;
                                            _2720 = _2477;
                                            _2074 = false;
                                            break;
                                        }
                                        float3 _2482;
                                        float3 _2487;
                                        _2482 = _2475;
                                        _2487 = _2477;
                                        float3 _2483;
                                        float _2486;
                                        float3 _2488;
                                        float3 _2712;
                                        float _2713;
                                        float3 _2714;
                                        bool _2715;
                                        bool _2716;
                                        float _2485 = _2476;
                                        uint _2489 = 0u;
                                        for (;;)
                                        {
                                            if (_2489 < _929)
                                            {
                                                bool _2502;
                                                bool _2621;
                                                do
                                                {
                                                    _2502 = _205[_2489].a.w == 2.0;
                                                    bool _2507;
                                                    if (_2502)
                                                    {
                                                        _2507 = _205[_2489].b.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _2507 = false;
                                                    }
                                                    bool _2512;
                                                    if (_2507)
                                                    {
                                                        _2512 = _205[_2489].c.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _2512 = false;
                                                    }
                                                    bool _2528;
                                                    if (!_2512)
                                                    {
                                                        bool _2521;
                                                        if ((isunordered(_205[_2489].a.w, 1.0) || _205[_2489].a.w == 1.0))
                                                        {
                                                            _2521 = _205[_2489].b.w != 1.0;
                                                        }
                                                        else
                                                        {
                                                            _2521 = true;
                                                        }
                                                        bool _2527;
                                                        if (!_2521)
                                                        {
                                                            _2527 = _205[_2489].c.w != 1.0;
                                                        }
                                                        else
                                                        {
                                                            _2527 = true;
                                                        }
                                                        _2528 = _2527;
                                                    }
                                                    else
                                                    {
                                                        _2528 = false;
                                                    }
                                                    bool _2538;
                                                    if (!_2528)
                                                    {
                                                        bool4 _2532 = isnan(_205[_2489].a);
                                                        bool4 _2533 = isinf(_205[_2489].a);
                                                        _2538 = !all(not(bool4(_2532.x || _2533.x, _2532.y || _2533.y, _2532.z || _2533.z, _2532.w || _2533.w)));
                                                    }
                                                    else
                                                    {
                                                        _2538 = true;
                                                    }
                                                    bool _2548;
                                                    if (!_2538)
                                                    {
                                                        bool4 _2542 = isnan(_205[_2489].b);
                                                        bool4 _2543 = isinf(_205[_2489].b);
                                                        _2548 = !all(not(bool4(_2542.x || _2543.x, _2542.y || _2543.y, _2542.z || _2543.z, _2542.w || _2543.w)));
                                                    }
                                                    else
                                                    {
                                                        _2548 = true;
                                                    }
                                                    bool _2558;
                                                    if (!_2548)
                                                    {
                                                        bool4 _2552 = isnan(_205[_2489].c);
                                                        bool4 _2553 = isinf(_205[_2489].c);
                                                        _2558 = !all(not(bool4(_2552.x || _2553.x, _2552.y || _2553.y, _2552.z || _2553.z, _2552.w || _2553.w)));
                                                    }
                                                    else
                                                    {
                                                        _2558 = true;
                                                    }
                                                    bool _2566;
                                                    if (!_2558)
                                                    {
                                                        _2566 = any(abs(_205[_2489].a.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _2566 = true;
                                                    }
                                                    bool _2574;
                                                    if (!_2566)
                                                    {
                                                        _2574 = any(abs(_205[_2489].b.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _2574 = true;
                                                    }
                                                    bool _2582;
                                                    if (!_2574)
                                                    {
                                                        _2582 = any(abs(_205[_2489].c.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _2582 = true;
                                                    }
                                                    if (_2582)
                                                    {
                                                        _2621 = false;
                                                        break;
                                                    }
                                                    bool _2590;
                                                    if (_2512)
                                                    {
                                                        _2590 = any(_205[_2489].c.xyz != float3(0.0));
                                                    }
                                                    else
                                                    {
                                                        _2590 = false;
                                                    }
                                                    if (_2590)
                                                    {
                                                        _2621 = false;
                                                        break;
                                                    }
                                                    float3 _2603;
                                                    if (_2512)
                                                    {
                                                        _2603 = _205[_2489].b.xyz;
                                                    }
                                                    else
                                                    {
                                                        _2603 = cross(_205[_2489].b.xyz - _205[_2489].a.xyz, _205[_2489].c.xyz - _205[_2489].a.xyz);
                                                    }
                                                    float _2604 = dot(_2603, _2603);
                                                    bool3 _2605 = isnan(_2603);
                                                    bool3 _2606 = isinf(_2603);
                                                    bool _2616;
                                                    if (all(not(bool3(_2605.x || _2606.x, _2605.y || _2606.y, _2605.z || _2606.z))))
                                                    {
                                                        _2616 = !(isnan(_2604) || isinf(_2604));
                                                    }
                                                    else
                                                    {
                                                        _2616 = false;
                                                    }
                                                    bool _2620;
                                                    if (_2616)
                                                    {
                                                        _2620 = _2604 > 9.9999996826552253889678874634872e-21;
                                                    }
                                                    else
                                                    {
                                                        _2620 = false;
                                                    }
                                                    _2621 = _2620;
                                                    break;
                                                } while(false);
                                                if (!_2621)
                                                {
                                                    _2712 = _2482;
                                                    _2713 = _2485;
                                                    _2714 = _2487;
                                                    _2715 = false;
                                                    _2716 = true;
                                                    break;
                                                }
                                                bool _2629;
                                                if (_2502)
                                                {
                                                    _2629 = _205[_2489].b.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2629 = false;
                                                }
                                                bool _2634;
                                                if (_2629)
                                                {
                                                    _2634 = _205[_2489].c.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2634 = false;
                                                }
                                                float3 _2645;
                                                if (_2634)
                                                {
                                                    _2645 = _205[_2489].b.xyz;
                                                }
                                                else
                                                {
                                                    _2645 = cross(_205[_2489].b.xyz - _205[_2489].a.xyz, _205[_2489].c.xyz - _205[_2489].a.xyz);
                                                }
                                                float3 _2646 = fast::normalize(_2645);
                                                float3 _2652;
                                                if (dot(_2646, _2482) > 0.0)
                                                {
                                                    _2652 = -_2646;
                                                }
                                                else
                                                {
                                                    _2652 = _2646;
                                                }
                                                float _2653 = dot(_2482, _2652);
                                                bool _2662;
                                                if (!(isnan(_2653) || isinf(_2653)))
                                                {
                                                    _2662 = abs(_2653) <= 9.9999999600419720025001879548654e-13;
                                                }
                                                else
                                                {
                                                    _2662 = true;
                                                }
                                                if (_2662)
                                                {
                                                    _2712 = _2482;
                                                    _2713 = _2485;
                                                    _2714 = _2487;
                                                    _2715 = false;
                                                    _2716 = true;
                                                    break;
                                                }
                                                float _2668 = dot(_205[_2489].a.xyz - _2487, _2652) / _2653;
                                                bool _2676;
                                                if (!(isnan(_2668) || isinf(_2668)))
                                                {
                                                    _2676 = _2668 <= _2485;
                                                }
                                                else
                                                {
                                                    _2676 = true;
                                                }
                                                bool _2681;
                                                if (!_2676)
                                                {
                                                    _2681 = _2668 >= 65536.0;
                                                }
                                                else
                                                {
                                                    _2681 = true;
                                                }
                                                if (_2681)
                                                {
                                                    _2712 = _2482;
                                                    _2713 = _2485;
                                                    _2714 = _2487;
                                                    _2715 = false;
                                                    _2716 = true;
                                                    break;
                                                }
                                                float3 _2685 = _2487 + (_2482 * _2668);
                                                bool3 _2686 = isnan(_2685);
                                                bool3 _2687 = isinf(_2685);
                                                if (!all(not(bool3(_2686.x || _2687.x, _2686.y || _2687.y, _2686.z || _2687.z))))
                                                {
                                                    _2712 = _2482;
                                                    _2713 = _2485;
                                                    _2714 = _2487;
                                                    _2715 = false;
                                                    _2716 = true;
                                                    break;
                                                }
                                                _2486 = precise::max(0.0500000007450580596923828125, _2668 * 9.9999997473787516355514526367188e-06);
                                                _2488 = _2685 + (_2652 * _2486);
                                                _2483 = reflect(_2482, _2652);
                                                bool3 _2696 = isnan(_2488);
                                                bool3 _2697 = isinf(_2488);
                                                bool _2709;
                                                if (all(not(bool3(_2696.x || _2697.x, _2696.y || _2697.y, _2696.z || _2697.z))))
                                                {
                                                    bool3 _2703 = isnan(_2483);
                                                    bool3 _2704 = isinf(_2483);
                                                    _2709 = !all(not(bool3(_2703.x || _2704.x, _2703.y || _2704.y, _2703.z || _2704.z)));
                                                }
                                                else
                                                {
                                                    _2709 = true;
                                                }
                                                if (_2709)
                                                {
                                                    _2712 = _2483;
                                                    _2713 = _2486;
                                                    _2714 = _2488;
                                                    _2715 = false;
                                                    _2716 = true;
                                                    break;
                                                }
                                                _2482 = _2483;
                                                _2485 = _2486;
                                                _2487 = _2488;
                                                _2489++;
                                                continue;
                                            }
                                            else
                                            {
                                                _2712 = _2482;
                                                _2713 = _2485;
                                                _2714 = _2487;
                                                _2715 = _2073;
                                                _2716 = false;
                                                break;
                                            }
                                        }
                                        if (_2716)
                                        {
                                            _2718 = _2712;
                                            _2719 = _2713;
                                            _2720 = _2714;
                                            _2074 = _2715;
                                            break;
                                        }
                                        _2718 = _2712;
                                        _2719 = _2713;
                                        _2720 = _2714;
                                        _2074 = true;
                                        break;
                                    } while(false);
                                    if (!_2074)
                                    {
                                        _2772 = float2(0.0);
                                        _2773 = false;
                                        break;
                                    }
                                    bool _2724 = _1521.w == 0.0;
                                    float3 _2729;
                                    if (_2724)
                                    {
                                        _2729 = _1521.xyz;
                                    }
                                    else
                                    {
                                        _2729 = _1521.xyz - _2720;
                                    }
                                    float _2730 = dot(_2729, _2729);
                                    bool _2743;
                                    if (!(isnan(_2730) || isinf(_2730)))
                                    {
                                        float _2741;
                                        if (_2724)
                                        {
                                            _2741 = 9.9999996826552253889678874634872e-21;
                                        }
                                        else
                                        {
                                            _2741 = _2719 * _2719;
                                        }
                                        _2743 = _2730 <= _2741;
                                    }
                                    else
                                    {
                                        _2743 = true;
                                    }
                                    if (_2743)
                                    {
                                        _2772 = float2(0.0);
                                        _2773 = false;
                                        break;
                                    }
                                    float3 _2747 = _2729 * rsqrt(_2730);
                                    if (dot(_2747, _2718) <= 0.0)
                                    {
                                        _2772 = float2(0.0);
                                        _2773 = false;
                                        break;
                                    }
                                    float3 _2758 = fast::normalize(cross(_2718, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_2718.y) < 0.949999988079071044921875))));
                                    float2 _2766 = float2(dot(_2747, _2758), dot(_2747, cross(_2718, _2758))) * precise::max(Settings.projection.x, Settings.projection.y);
                                    bool2 _2767 = isnan(_2766);
                                    bool2 _2768 = isinf(_2766);
                                    _2772 = _2766;
                                    _2773 = all(not(bool2(_2767.x || _2768.x, _2767.y || _2768.y)));
                                    break;
                                } while(false);
                                if (!_2773)
                                {
                                    _5783 = _2062;
                                    _5784 = _785;
                                    _5785 = _2065;
                                    _5786 = _2067;
                                    _5787 = _2069;
                                    _5788 = _2071;
                                    _5789 = _2074;
                                    _5790 = float4(0.0);
                                    _5791 = true;
                                    break;
                                }
                                float _2781 = precise::max(abs(_2772.x), abs(_2772.y));
                                if (_2781 <= 0.00200000009499490261077880859375)
                                {
                                    bool _3567;
                                    float4 _3603;
                                    do
                                    {
                                        spvUnsafeArray<ReflectionSpecularPlane, 4> _203 = _206;
                                        float _3184;
                                        float3 _3566;
                                        do
                                        {
                                            bool _3022;
                                            if (_1525)
                                            {
                                                bool _3020;
                                                do
                                                {
                                                    bool _2797;
                                                    if (_1998)
                                                    {
                                                        _2797 = _1153.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _2797 = false;
                                                    }
                                                    bool _2802;
                                                    if (_2797)
                                                    {
                                                        _2802 = _1154.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _2802 = false;
                                                    }
                                                    bool _2818;
                                                    if (!_2802)
                                                    {
                                                        bool _2811;
                                                        if ((isunordered(_1152.w, 1.0) || _1152.w == 1.0))
                                                        {
                                                            _2811 = _1153.w != 1.0;
                                                        }
                                                        else
                                                        {
                                                            _2811 = true;
                                                        }
                                                        bool _2817;
                                                        if (!_2811)
                                                        {
                                                            _2817 = _1154.w != 1.0;
                                                        }
                                                        else
                                                        {
                                                            _2817 = true;
                                                        }
                                                        _2818 = _2817;
                                                    }
                                                    else
                                                    {
                                                        _2818 = false;
                                                    }
                                                    bool _2828;
                                                    if (!_2818)
                                                    {
                                                        bool4 _2822 = isnan(_1152);
                                                        bool4 _2823 = isinf(_1152);
                                                        _2828 = !all(not(bool4(_2822.x || _2823.x, _2822.y || _2823.y, _2822.z || _2823.z, _2822.w || _2823.w)));
                                                    }
                                                    else
                                                    {
                                                        _2828 = true;
                                                    }
                                                    bool _2838;
                                                    if (!_2828)
                                                    {
                                                        bool4 _2832 = isnan(_1153);
                                                        bool4 _2833 = isinf(_1153);
                                                        _2838 = !all(not(bool4(_2832.x || _2833.x, _2832.y || _2833.y, _2832.z || _2833.z, _2832.w || _2833.w)));
                                                    }
                                                    else
                                                    {
                                                        _2838 = true;
                                                    }
                                                    bool _2848;
                                                    if (!_2838)
                                                    {
                                                        bool4 _2842 = isnan(_1154);
                                                        bool4 _2843 = isinf(_1154);
                                                        _2848 = !all(not(bool4(_2842.x || _2843.x, _2842.y || _2843.y, _2842.z || _2843.z, _2842.w || _2843.w)));
                                                    }
                                                    else
                                                    {
                                                        _2848 = true;
                                                    }
                                                    bool _2858;
                                                    if (!_2848)
                                                    {
                                                        bool4 _2852 = isnan(Settings.projection);
                                                        bool4 _2853 = isinf(Settings.projection);
                                                        _2858 = !all(not(bool4(_2852.x || _2853.x, _2852.y || _2853.y, _2852.z || _2853.z, _2852.w || _2853.w)));
                                                    }
                                                    else
                                                    {
                                                        _2858 = true;
                                                    }
                                                    bool _2868;
                                                    if (!_2858)
                                                    {
                                                        bool4 _2862 = isnan(_1275);
                                                        bool4 _2863 = isinf(_1275);
                                                        _2868 = !all(not(bool4(_2862.x || _2863.x, _2862.y || _2863.y, _2862.z || _2863.z, _2862.w || _2863.w)));
                                                    }
                                                    else
                                                    {
                                                        _2868 = true;
                                                    }
                                                    bool _2878;
                                                    if (!_2868)
                                                    {
                                                        bool4 _2872 = isnan(_1283);
                                                        bool4 _2873 = isinf(_1283);
                                                        _2878 = !all(not(bool4(_2872.x || _2873.x, _2872.y || _2873.y, _2872.z || _2873.z, _2872.w || _2873.w)));
                                                    }
                                                    else
                                                    {
                                                        _2878 = true;
                                                    }
                                                    bool _2886;
                                                    if (!_2878)
                                                    {
                                                        _2886 = any(abs(_1152.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _2886 = true;
                                                    }
                                                    bool _2894;
                                                    if (!_2886)
                                                    {
                                                        _2894 = any(abs(_1153.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _2894 = true;
                                                    }
                                                    bool _2902;
                                                    if (!_2894)
                                                    {
                                                        _2902 = any(abs(_1154.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _2902 = true;
                                                    }
                                                    bool _2909;
                                                    if (!_2902)
                                                    {
                                                        _2909 = any(Settings.projection.xy <= float2(0.0));
                                                    }
                                                    else
                                                    {
                                                        _2909 = true;
                                                    }
                                                    bool _2916;
                                                    if (!_2909)
                                                    {
                                                        _2916 = any(abs(Settings.projection) > float4(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _2916 = true;
                                                    }
                                                    bool _2922;
                                                    if (!_2916)
                                                    {
                                                        _2922 = any(_2033 < float2(1.0));
                                                    }
                                                    else
                                                    {
                                                        _2922 = true;
                                                    }
                                                    bool _2928;
                                                    if (!_2922)
                                                    {
                                                        _2928 = any(_2033 > float2(16384.0));
                                                    }
                                                    else
                                                    {
                                                        _2928 = true;
                                                    }
                                                    bool _2933;
                                                    if (!_2928)
                                                    {
                                                        _2933 = Settings.clip.x <= 0.0;
                                                    }
                                                    else
                                                    {
                                                        _2933 = true;
                                                    }
                                                    bool _2938;
                                                    if (!_2933)
                                                    {
                                                        _2938 = Settings.clip.y <= Settings.clip.x;
                                                    }
                                                    else
                                                    {
                                                        _2938 = true;
                                                    }
                                                    bool _2943;
                                                    if (!_2938)
                                                    {
                                                        _2943 = Settings.clip.y > 999999995904.0;
                                                    }
                                                    else
                                                    {
                                                        _2943 = true;
                                                    }
                                                    bool _2949;
                                                    if (!_2943)
                                                    {
                                                        _2949 = _1283.x < 0.0;
                                                    }
                                                    else
                                                    {
                                                        _2949 = true;
                                                    }
                                                    bool _2955;
                                                    if (!_2949)
                                                    {
                                                        _2955 = _1283.x > 1.0;
                                                    }
                                                    else
                                                    {
                                                        _2955 = true;
                                                    }
                                                    bool _2961;
                                                    if (!_2955)
                                                    {
                                                        _2961 = _1283.y < 0.0;
                                                    }
                                                    else
                                                    {
                                                        _2961 = true;
                                                    }
                                                    bool _2967;
                                                    if (!_2961)
                                                    {
                                                        _2967 = _1283.y > 7.0;
                                                    }
                                                    else
                                                    {
                                                        _2967 = true;
                                                    }
                                                    bool _2974;
                                                    if (!_2967)
                                                    {
                                                        _2974 = floor(_1283.y) != _1283.y;
                                                    }
                                                    else
                                                    {
                                                        _2974 = true;
                                                    }
                                                    bool _2981;
                                                    if (!_2974)
                                                    {
                                                        _2981 = any(_1283.zw != float2(0.0));
                                                    }
                                                    else
                                                    {
                                                        _2981 = true;
                                                    }
                                                    if (_2981)
                                                    {
                                                        _3020 = false;
                                                        break;
                                                    }
                                                    bool _2989;
                                                    if (_2802)
                                                    {
                                                        _2989 = any(_1154.xyz != float3(0.0));
                                                    }
                                                    else
                                                    {
                                                        _2989 = false;
                                                    }
                                                    if (_2989)
                                                    {
                                                        _3020 = false;
                                                        break;
                                                    }
                                                    float3 _3002;
                                                    if (_2802)
                                                    {
                                                        _3002 = _1153.xyz;
                                                    }
                                                    else
                                                    {
                                                        _3002 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                    }
                                                    float _3003 = dot(_3002, _3002);
                                                    bool3 _3004 = isnan(_3002);
                                                    bool3 _3005 = isinf(_3002);
                                                    bool _3015;
                                                    if (all(not(bool3(_3004.x || _3005.x, _3004.y || _3005.y, _3004.z || _3005.z))))
                                                    {
                                                        _3015 = !(isnan(_3003) || isinf(_3003));
                                                    }
                                                    else
                                                    {
                                                        _3015 = false;
                                                    }
                                                    bool _3019;
                                                    if (_3015)
                                                    {
                                                        _3019 = _3003 > 9.9999996826552253889678874634872e-21;
                                                    }
                                                    else
                                                    {
                                                        _3019 = false;
                                                    }
                                                    _3020 = _3019;
                                                    break;
                                                } while(false);
                                                _3022 = !_3020;
                                            }
                                            else
                                            {
                                                _3022 = true;
                                            }
                                            float _3181;
                                            float3 _3182;
                                            float3 _3183;
                                            bool _3185;
                                            if (!_3022)
                                            {
                                                float _3175;
                                                float3 _3176;
                                                float3 _3177;
                                                float _3178;
                                                bool _3179;
                                                do
                                                {
                                                    bool2 _3028 = isnan(_2062);
                                                    bool2 _3029 = isinf(_2062);
                                                    bool _3037;
                                                    if (all(not(bool2(_3028.x || _3029.x, _3028.y || _3029.y))))
                                                    {
                                                        _3037 = any(_2062 < float2(0.0));
                                                    }
                                                    else
                                                    {
                                                        _3037 = true;
                                                    }
                                                    bool _3043;
                                                    if (!_3037)
                                                    {
                                                        _3043 = any(_2062 >= _2033);
                                                    }
                                                    else
                                                    {
                                                        _3043 = true;
                                                    }
                                                    if (_3043)
                                                    {
                                                        _3175 = 0.0;
                                                        _3176 = float3(0.0);
                                                        _3177 = float3(0.0);
                                                        _3178 = 0.0;
                                                        _3179 = false;
                                                        break;
                                                    }
                                                    bool _3050;
                                                    if (_1998)
                                                    {
                                                        _3050 = _1153.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _3050 = false;
                                                    }
                                                    bool _3055;
                                                    if (_3050)
                                                    {
                                                        _3055 = _1154.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _3055 = false;
                                                    }
                                                    float3 _3066;
                                                    if (_3055)
                                                    {
                                                        _3066 = _1153.xyz;
                                                    }
                                                    else
                                                    {
                                                        _3066 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                    }
                                                    float3 _3067 = fast::normalize(_3066);
                                                    float3 _3075 = fast::normalize(float3((_2062 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                                    float3 _3081;
                                                    if (dot(_3067, _3075) > 0.0)
                                                    {
                                                        _3081 = -_3067;
                                                    }
                                                    else
                                                    {
                                                        _3081 = _3067;
                                                    }
                                                    float _3082 = dot(_3075, _3081);
                                                    bool _3091;
                                                    if (!(isnan(_3082) || isinf(_3082)))
                                                    {
                                                        _3091 = abs(_3082) <= 9.9999999600419720025001879548654e-13;
                                                    }
                                                    else
                                                    {
                                                        _3091 = true;
                                                    }
                                                    if (_3091)
                                                    {
                                                        _3175 = 0.0;
                                                        _3176 = float3(0.0);
                                                        _3177 = float3(0.0);
                                                        _3178 = 0.0;
                                                        _3179 = false;
                                                        break;
                                                    }
                                                    float _3096 = dot(_1152.xyz, _3081) / _3082;
                                                    float _3098 = _3096 * _3075.z;
                                                    bool _3106;
                                                    if (!(isnan(_3096) || isinf(_3096)))
                                                    {
                                                        _3106 = _3096 <= 0.0;
                                                    }
                                                    else
                                                    {
                                                        _3106 = true;
                                                    }
                                                    bool _3111;
                                                    if (!_3106)
                                                    {
                                                        _3111 = _3098 < Settings.clip.x;
                                                    }
                                                    else
                                                    {
                                                        _3111 = true;
                                                    }
                                                    bool _3116;
                                                    if (!_3111)
                                                    {
                                                        _3116 = _3098 > Settings.clip.y;
                                                    }
                                                    else
                                                    {
                                                        _3116 = true;
                                                    }
                                                    if (_3116)
                                                    {
                                                        _3175 = 0.0;
                                                        _3176 = float3(0.0);
                                                        _3177 = float3(0.0);
                                                        _3178 = _3098;
                                                        _3179 = false;
                                                        break;
                                                    }
                                                    bool _3123;
                                                    if (_1998)
                                                    {
                                                        _3123 = _1153.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _3123 = false;
                                                    }
                                                    bool _3128;
                                                    if (_3123)
                                                    {
                                                        _3128 = _1154.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _3128 = false;
                                                    }
                                                    float _3131 = precise::max(_3128 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _3096 * 9.9999997473787516355514526367188e-06);
                                                    float3 _3134 = (_3075 * _3096) + (_3081 * _3131);
                                                    float3 _3135 = reflect(_3075, _3081);
                                                    uint _3138 = uint(_1283.y);
                                                    float3 _3145 = fast::normalize(cross(_3135, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_3135.y) < 0.949999988079071044921875))));
                                                    float3 _3157 = fast::normalize(_3135 + (((_3145 * _175[_3138].x) + (cross(_3135, _3145) * _175[_3138].y)) * (_1283.x * _1283.x)));
                                                    float3 _3161 = select(_3157, _3135, bool3(dot(_3157, _3081) <= 0.0));
                                                    bool3 _3162 = isnan(_3134);
                                                    bool3 _3163 = isinf(_3134);
                                                    bool _3174;
                                                    if (all(not(bool3(_3162.x || _3163.x, _3162.y || _3163.y, _3162.z || _3163.z))))
                                                    {
                                                        bool3 _3169 = isnan(_3161);
                                                        bool3 _3170 = isinf(_3161);
                                                        _3174 = all(not(bool3(_3169.x || _3170.x, _3169.y || _3170.y, _3169.z || _3170.z)));
                                                    }
                                                    else
                                                    {
                                                        _3174 = false;
                                                    }
                                                    _3175 = _3131;
                                                    _3176 = _3134;
                                                    _3177 = _3161;
                                                    _3178 = _3098;
                                                    _3179 = _3174;
                                                    break;
                                                } while(false);
                                                _3181 = _3175;
                                                _3182 = _3176;
                                                _3183 = _3177;
                                                _3184 = _3178;
                                                _3185 = !_3179;
                                            }
                                            else
                                            {
                                                _3181 = 0.0;
                                                _3182 = float3(0.0);
                                                _3183 = float3(0.0);
                                                _3184 = 0.0;
                                                _3185 = true;
                                            }
                                            if (_3185)
                                            {
                                                _3566 = _3182;
                                                _3567 = false;
                                                break;
                                            }
                                            bool _3260;
                                            do
                                            {
                                                bool _3194;
                                                if (_1998)
                                                {
                                                    _3194 = _1153.w == 2.0;
                                                }
                                                else
                                                {
                                                    _3194 = false;
                                                }
                                                bool _3199;
                                                if (_3194)
                                                {
                                                    _3199 = _1154.w == 2.0;
                                                }
                                                else
                                                {
                                                    _3199 = false;
                                                }
                                                if (_3199)
                                                {
                                                    _3260 = true;
                                                    break;
                                                }
                                                float3 _3212 = _1153.xyz - _1152.xyz;
                                                float3 _3214 = _1154.xyz - _1152.xyz;
                                                float3 _3215 = (float3((_2062 - Settings.projection.zw) / Settings.projection.xy, 1.0) * _3184) - _1152.xyz;
                                                float _3216 = dot(_3212, _3212);
                                                float _3217 = dot(_3212, _3214);
                                                float _3218 = dot(_3214, _3214);
                                                float _3219 = dot(_3215, _3212);
                                                float _3220 = dot(_3215, _3214);
                                                float _3223 = (_3216 * _3218) - (_3217 * _3217);
                                                bool _3231;
                                                if (!(isnan(_3223) || isinf(_3223)))
                                                {
                                                    _3231 = _3223 <= 9.9999996826552253889678874634872e-21;
                                                }
                                                else
                                                {
                                                    _3231 = true;
                                                }
                                                if (_3231)
                                                {
                                                    _3260 = false;
                                                    break;
                                                }
                                                float2 _3242 = float2((_3218 * _3219) - (_3217 * _3220), (_3216 * _3220) - (_3217 * _3219)) / float2(_3223);
                                                bool2 _3243 = isnan(_3242);
                                                bool2 _3244 = isinf(_3242);
                                                bool _3252;
                                                if (all(not(bool2(_3243.x || _3244.x, _3243.y || _3244.y))))
                                                {
                                                    _3252 = all(_3242 >= float2(-9.9999997473787516355514526367188e-06));
                                                }
                                                else
                                                {
                                                    _3252 = false;
                                                }
                                                bool _3259;
                                                if (_3252)
                                                {
                                                    _3259 = (_3242.x + _3242.y) <= 1.000010013580322265625;
                                                }
                                                else
                                                {
                                                    _3259 = false;
                                                }
                                                _3260 = _3259;
                                                break;
                                            } while(false);
                                            if (!_3260)
                                            {
                                                _3566 = _3182;
                                                _3567 = false;
                                                break;
                                            }
                                            float3 _3268;
                                            float3 _3270;
                                            _3268 = _3182;
                                            _3270 = _3183;
                                            float _3266;
                                            float3 _3269;
                                            float3 _3271;
                                            float3 _3562;
                                            bool _3563;
                                            bool _3564;
                                            float _3265 = _3181;
                                            uint _3272 = 0u;
                                            for (;;)
                                            {
                                                if (_3272 < _929)
                                                {
                                                    bool _3285;
                                                    bool _3404;
                                                    do
                                                    {
                                                        _3285 = _203[_3272].a.w == 2.0;
                                                        bool _3290;
                                                        if (_3285)
                                                        {
                                                            _3290 = _203[_3272].b.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _3290 = false;
                                                        }
                                                        bool _3295;
                                                        if (_3290)
                                                        {
                                                            _3295 = _203[_3272].c.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _3295 = false;
                                                        }
                                                        bool _3311;
                                                        if (!_3295)
                                                        {
                                                            bool _3304;
                                                            if ((isunordered(_203[_3272].a.w, 1.0) || _203[_3272].a.w == 1.0))
                                                            {
                                                                _3304 = _203[_3272].b.w != 1.0;
                                                            }
                                                            else
                                                            {
                                                                _3304 = true;
                                                            }
                                                            bool _3310;
                                                            if (!_3304)
                                                            {
                                                                _3310 = _203[_3272].c.w != 1.0;
                                                            }
                                                            else
                                                            {
                                                                _3310 = true;
                                                            }
                                                            _3311 = _3310;
                                                        }
                                                        else
                                                        {
                                                            _3311 = false;
                                                        }
                                                        bool _3321;
                                                        if (!_3311)
                                                        {
                                                            bool4 _3315 = isnan(_203[_3272].a);
                                                            bool4 _3316 = isinf(_203[_3272].a);
                                                            _3321 = !all(not(bool4(_3315.x || _3316.x, _3315.y || _3316.y, _3315.z || _3316.z, _3315.w || _3316.w)));
                                                        }
                                                        else
                                                        {
                                                            _3321 = true;
                                                        }
                                                        bool _3331;
                                                        if (!_3321)
                                                        {
                                                            bool4 _3325 = isnan(_203[_3272].b);
                                                            bool4 _3326 = isinf(_203[_3272].b);
                                                            _3331 = !all(not(bool4(_3325.x || _3326.x, _3325.y || _3326.y, _3325.z || _3326.z, _3325.w || _3326.w)));
                                                        }
                                                        else
                                                        {
                                                            _3331 = true;
                                                        }
                                                        bool _3341;
                                                        if (!_3331)
                                                        {
                                                            bool4 _3335 = isnan(_203[_3272].c);
                                                            bool4 _3336 = isinf(_203[_3272].c);
                                                            _3341 = !all(not(bool4(_3335.x || _3336.x, _3335.y || _3336.y, _3335.z || _3336.z, _3335.w || _3336.w)));
                                                        }
                                                        else
                                                        {
                                                            _3341 = true;
                                                        }
                                                        bool _3349;
                                                        if (!_3341)
                                                        {
                                                            _3349 = any(abs(_203[_3272].a.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _3349 = true;
                                                        }
                                                        bool _3357;
                                                        if (!_3349)
                                                        {
                                                            _3357 = any(abs(_203[_3272].b.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _3357 = true;
                                                        }
                                                        bool _3365;
                                                        if (!_3357)
                                                        {
                                                            _3365 = any(abs(_203[_3272].c.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _3365 = true;
                                                        }
                                                        if (_3365)
                                                        {
                                                            _3404 = false;
                                                            break;
                                                        }
                                                        bool _3373;
                                                        if (_3295)
                                                        {
                                                            _3373 = any(_203[_3272].c.xyz != float3(0.0));
                                                        }
                                                        else
                                                        {
                                                            _3373 = false;
                                                        }
                                                        if (_3373)
                                                        {
                                                            _3404 = false;
                                                            break;
                                                        }
                                                        float3 _3386;
                                                        if (_3295)
                                                        {
                                                            _3386 = _203[_3272].b.xyz;
                                                        }
                                                        else
                                                        {
                                                            _3386 = cross(_203[_3272].b.xyz - _203[_3272].a.xyz, _203[_3272].c.xyz - _203[_3272].a.xyz);
                                                        }
                                                        float _3387 = dot(_3386, _3386);
                                                        bool3 _3388 = isnan(_3386);
                                                        bool3 _3389 = isinf(_3386);
                                                        bool _3399;
                                                        if (all(not(bool3(_3388.x || _3389.x, _3388.y || _3389.y, _3388.z || _3389.z))))
                                                        {
                                                            _3399 = !(isnan(_3387) || isinf(_3387));
                                                        }
                                                        else
                                                        {
                                                            _3399 = false;
                                                        }
                                                        bool _3403;
                                                        if (_3399)
                                                        {
                                                            _3403 = _3387 > 9.9999996826552253889678874634872e-21;
                                                        }
                                                        else
                                                        {
                                                            _3403 = false;
                                                        }
                                                        _3404 = _3403;
                                                        break;
                                                    } while(false);
                                                    if (!_3404)
                                                    {
                                                        _3562 = _3268;
                                                        _3563 = false;
                                                        _3564 = true;
                                                        break;
                                                    }
                                                    bool _3412;
                                                    if (_3285)
                                                    {
                                                        _3412 = _203[_3272].b.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _3412 = false;
                                                    }
                                                    bool _3417;
                                                    if (_3412)
                                                    {
                                                        _3417 = _203[_3272].c.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _3417 = false;
                                                    }
                                                    float3 _3428;
                                                    if (_3417)
                                                    {
                                                        _3428 = _203[_3272].b.xyz;
                                                    }
                                                    else
                                                    {
                                                        _3428 = cross(_203[_3272].b.xyz - _203[_3272].a.xyz, _203[_3272].c.xyz - _203[_3272].a.xyz);
                                                    }
                                                    float3 _3429 = fast::normalize(_3428);
                                                    float3 _3435;
                                                    if (dot(_3429, _3270) > 0.0)
                                                    {
                                                        _3435 = -_3429;
                                                    }
                                                    else
                                                    {
                                                        _3435 = _3429;
                                                    }
                                                    float _3436 = dot(_3270, _3435);
                                                    bool _3445;
                                                    if (!(isnan(_3436) || isinf(_3436)))
                                                    {
                                                        _3445 = abs(_3436) <= 9.9999999600419720025001879548654e-13;
                                                    }
                                                    else
                                                    {
                                                        _3445 = true;
                                                    }
                                                    if (_3445)
                                                    {
                                                        _3562 = _3268;
                                                        _3563 = false;
                                                        _3564 = true;
                                                        break;
                                                    }
                                                    float _3451 = dot(_203[_3272].a.xyz - _3268, _3435) / _3436;
                                                    bool _3459;
                                                    if (!(isnan(_3451) || isinf(_3451)))
                                                    {
                                                        _3459 = _3451 <= _3265;
                                                    }
                                                    else
                                                    {
                                                        _3459 = true;
                                                    }
                                                    bool _3464;
                                                    if (!_3459)
                                                    {
                                                        _3464 = _3451 >= 65536.0;
                                                    }
                                                    else
                                                    {
                                                        _3464 = true;
                                                    }
                                                    if (_3464)
                                                    {
                                                        _3562 = _3268;
                                                        _3563 = false;
                                                        _3564 = true;
                                                        break;
                                                    }
                                                    float3 _3468 = _3268 + (_3270 * _3451);
                                                    bool3 _3469 = isnan(_3468);
                                                    bool3 _3470 = isinf(_3468);
                                                    bool _3541;
                                                    if (all(not(bool3(_3469.x || _3470.x, _3469.y || _3470.y, _3469.z || _3470.z))))
                                                    {
                                                        bool _3539;
                                                        do
                                                        {
                                                            bool _3482;
                                                            if (_3285)
                                                            {
                                                                _3482 = _203[_3272].b.w == 2.0;
                                                            }
                                                            else
                                                            {
                                                                _3482 = false;
                                                            }
                                                            bool _3487;
                                                            if (_3482)
                                                            {
                                                                _3487 = _203[_3272].c.w == 2.0;
                                                            }
                                                            else
                                                            {
                                                                _3487 = false;
                                                            }
                                                            if (_3487)
                                                            {
                                                                _3539 = true;
                                                                break;
                                                            }
                                                            float3 _3491 = _203[_3272].b.xyz - _203[_3272].a.xyz;
                                                            float3 _3493 = _203[_3272].c.xyz - _203[_3272].a.xyz;
                                                            float3 _3494 = _3468 - _203[_3272].a.xyz;
                                                            float _3495 = dot(_3491, _3491);
                                                            float _3496 = dot(_3491, _3493);
                                                            float _3497 = dot(_3493, _3493);
                                                            float _3498 = dot(_3494, _3491);
                                                            float _3499 = dot(_3494, _3493);
                                                            float _3502 = (_3495 * _3497) - (_3496 * _3496);
                                                            bool _3510;
                                                            if (!(isnan(_3502) || isinf(_3502)))
                                                            {
                                                                _3510 = _3502 <= 9.9999996826552253889678874634872e-21;
                                                            }
                                                            else
                                                            {
                                                                _3510 = true;
                                                            }
                                                            if (_3510)
                                                            {
                                                                _3539 = false;
                                                                break;
                                                            }
                                                            float2 _3521 = float2((_3497 * _3498) - (_3496 * _3499), (_3495 * _3499) - (_3496 * _3498)) / float2(_3502);
                                                            bool2 _3522 = isnan(_3521);
                                                            bool2 _3523 = isinf(_3521);
                                                            bool _3531;
                                                            if (all(not(bool2(_3522.x || _3523.x, _3522.y || _3523.y))))
                                                            {
                                                                _3531 = all(_3521 >= float2(-9.9999997473787516355514526367188e-06));
                                                            }
                                                            else
                                                            {
                                                                _3531 = false;
                                                            }
                                                            bool _3538;
                                                            if (_3531)
                                                            {
                                                                _3538 = (_3521.x + _3521.y) <= 1.000010013580322265625;
                                                            }
                                                            else
                                                            {
                                                                _3538 = false;
                                                            }
                                                            _3539 = _3538;
                                                            break;
                                                        } while(false);
                                                        _3541 = !_3539;
                                                    }
                                                    else
                                                    {
                                                        _3541 = true;
                                                    }
                                                    if (_3541)
                                                    {
                                                        _3562 = _3268;
                                                        _3563 = false;
                                                        _3564 = true;
                                                        break;
                                                    }
                                                    _3266 = precise::max(0.0500000007450580596923828125, _3451 * 9.9999997473787516355514526367188e-06);
                                                    _3269 = _3468 + (_3435 * _3266);
                                                    _3271 = reflect(_3270, _3435);
                                                    bool3 _3546 = isnan(_3269);
                                                    bool3 _3547 = isinf(_3269);
                                                    bool _3559;
                                                    if (all(not(bool3(_3546.x || _3547.x, _3546.y || _3547.y, _3546.z || _3547.z))))
                                                    {
                                                        bool3 _3553 = isnan(_3271);
                                                        bool3 _3554 = isinf(_3271);
                                                        _3559 = !all(not(bool3(_3553.x || _3554.x, _3553.y || _3554.y, _3553.z || _3554.z)));
                                                    }
                                                    else
                                                    {
                                                        _3559 = true;
                                                    }
                                                    if (_3559)
                                                    {
                                                        _3562 = _3269;
                                                        _3563 = false;
                                                        _3564 = true;
                                                        break;
                                                    }
                                                    _3265 = _3266;
                                                    _3268 = _3269;
                                                    _3270 = _3271;
                                                    _3272++;
                                                    continue;
                                                }
                                                else
                                                {
                                                    _3562 = _3268;
                                                    _3563 = _785;
                                                    _3564 = false;
                                                    break;
                                                }
                                            }
                                            if (_3564)
                                            {
                                                _3566 = _3562;
                                                _3567 = _3563;
                                                break;
                                            }
                                            _3566 = _3562;
                                            _3567 = true;
                                            break;
                                        } while(false);
                                        if (!_3567)
                                        {
                                            _3603 = float4(0.0);
                                            break;
                                        }
                                        if (_2023)
                                        {
                                            float3 _3575 = precise::max(abs(_3566), abs(_1521.xyz));
                                            float _3583 = length(_1521.xyz - _3566);
                                            bool _3597;
                                            if (!(isnan(_3583) || isinf(_3583)))
                                            {
                                                _3597 = ((precise::max(1.0, precise::max(_3575.x, precise::max(_3575.y, _3575.z))) * 9.9999999747524270787835121154785e-07) * precise::max(Settings.projection.x, Settings.projection.y)) > (_3583 * 0.01200000010430812835693359375);
                                            }
                                            else
                                            {
                                                _3597 = true;
                                            }
                                            if (_3597)
                                            {
                                                _3603 = float4(0.0);
                                                break;
                                            }
                                        }
                                        _3603 = float4(_2062, _3184, 1.0);
                                        break;
                                    } while(false);
                                    _5783 = _2062;
                                    _5784 = _3567;
                                    _5785 = _2065;
                                    _5786 = _2067;
                                    _5787 = _2069;
                                    _5788 = _2071;
                                    _5789 = _2074;
                                    _5790 = _3603;
                                    _5791 = true;
                                    break;
                                }
                                float _3607 = ((_2062.x + 0.25) < _1267) ? 0.25 : (-0.25);
                                float _3611 = ((_2062.y + 0.25) < _1270) ? 0.25 : (-0.25);
                                float2 _3613 = _2062 + float2(_3607, 0.0);
                                float2 _4306;
                                bool _4307;
                                do
                                {
                                    spvUnsafeArray<ReflectionSpecularPlane, 4> _201 = _206;
                                    float3 _4252;
                                    float _4253;
                                    float3 _4254;
                                    do
                                    {
                                        bool _3851;
                                        if (_1525)
                                        {
                                            bool _3849;
                                            do
                                            {
                                                bool _3626;
                                                if (_1998)
                                                {
                                                    _3626 = _1153.w == 2.0;
                                                }
                                                else
                                                {
                                                    _3626 = false;
                                                }
                                                bool _3631;
                                                if (_3626)
                                                {
                                                    _3631 = _1154.w == 2.0;
                                                }
                                                else
                                                {
                                                    _3631 = false;
                                                }
                                                bool _3647;
                                                if (!_3631)
                                                {
                                                    bool _3640;
                                                    if ((isunordered(_1152.w, 1.0) || _1152.w == 1.0))
                                                    {
                                                        _3640 = _1153.w != 1.0;
                                                    }
                                                    else
                                                    {
                                                        _3640 = true;
                                                    }
                                                    bool _3646;
                                                    if (!_3640)
                                                    {
                                                        _3646 = _1154.w != 1.0;
                                                    }
                                                    else
                                                    {
                                                        _3646 = true;
                                                    }
                                                    _3647 = _3646;
                                                }
                                                else
                                                {
                                                    _3647 = false;
                                                }
                                                bool _3657;
                                                if (!_3647)
                                                {
                                                    bool4 _3651 = isnan(_1152);
                                                    bool4 _3652 = isinf(_1152);
                                                    _3657 = !all(not(bool4(_3651.x || _3652.x, _3651.y || _3652.y, _3651.z || _3652.z, _3651.w || _3652.w)));
                                                }
                                                else
                                                {
                                                    _3657 = true;
                                                }
                                                bool _3667;
                                                if (!_3657)
                                                {
                                                    bool4 _3661 = isnan(_1153);
                                                    bool4 _3662 = isinf(_1153);
                                                    _3667 = !all(not(bool4(_3661.x || _3662.x, _3661.y || _3662.y, _3661.z || _3662.z, _3661.w || _3662.w)));
                                                }
                                                else
                                                {
                                                    _3667 = true;
                                                }
                                                bool _3677;
                                                if (!_3667)
                                                {
                                                    bool4 _3671 = isnan(_1154);
                                                    bool4 _3672 = isinf(_1154);
                                                    _3677 = !all(not(bool4(_3671.x || _3672.x, _3671.y || _3672.y, _3671.z || _3672.z, _3671.w || _3672.w)));
                                                }
                                                else
                                                {
                                                    _3677 = true;
                                                }
                                                bool _3687;
                                                if (!_3677)
                                                {
                                                    bool4 _3681 = isnan(Settings.projection);
                                                    bool4 _3682 = isinf(Settings.projection);
                                                    _3687 = !all(not(bool4(_3681.x || _3682.x, _3681.y || _3682.y, _3681.z || _3682.z, _3681.w || _3682.w)));
                                                }
                                                else
                                                {
                                                    _3687 = true;
                                                }
                                                bool _3697;
                                                if (!_3687)
                                                {
                                                    bool4 _3691 = isnan(_1275);
                                                    bool4 _3692 = isinf(_1275);
                                                    _3697 = !all(not(bool4(_3691.x || _3692.x, _3691.y || _3692.y, _3691.z || _3692.z, _3691.w || _3692.w)));
                                                }
                                                else
                                                {
                                                    _3697 = true;
                                                }
                                                bool _3707;
                                                if (!_3697)
                                                {
                                                    bool4 _3701 = isnan(_1283);
                                                    bool4 _3702 = isinf(_1283);
                                                    _3707 = !all(not(bool4(_3701.x || _3702.x, _3701.y || _3702.y, _3701.z || _3702.z, _3701.w || _3702.w)));
                                                }
                                                else
                                                {
                                                    _3707 = true;
                                                }
                                                bool _3715;
                                                if (!_3707)
                                                {
                                                    _3715 = any(abs(_1152.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _3715 = true;
                                                }
                                                bool _3723;
                                                if (!_3715)
                                                {
                                                    _3723 = any(abs(_1153.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _3723 = true;
                                                }
                                                bool _3731;
                                                if (!_3723)
                                                {
                                                    _3731 = any(abs(_1154.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _3731 = true;
                                                }
                                                bool _3738;
                                                if (!_3731)
                                                {
                                                    _3738 = any(Settings.projection.xy <= float2(0.0));
                                                }
                                                else
                                                {
                                                    _3738 = true;
                                                }
                                                bool _3745;
                                                if (!_3738)
                                                {
                                                    _3745 = any(abs(Settings.projection) > float4(999999995904.0));
                                                }
                                                else
                                                {
                                                    _3745 = true;
                                                }
                                                bool _3751;
                                                if (!_3745)
                                                {
                                                    _3751 = any(_2033 < float2(1.0));
                                                }
                                                else
                                                {
                                                    _3751 = true;
                                                }
                                                bool _3757;
                                                if (!_3751)
                                                {
                                                    _3757 = any(_2033 > float2(16384.0));
                                                }
                                                else
                                                {
                                                    _3757 = true;
                                                }
                                                bool _3762;
                                                if (!_3757)
                                                {
                                                    _3762 = Settings.clip.x <= 0.0;
                                                }
                                                else
                                                {
                                                    _3762 = true;
                                                }
                                                bool _3767;
                                                if (!_3762)
                                                {
                                                    _3767 = Settings.clip.y <= Settings.clip.x;
                                                }
                                                else
                                                {
                                                    _3767 = true;
                                                }
                                                bool _3772;
                                                if (!_3767)
                                                {
                                                    _3772 = Settings.clip.y > 999999995904.0;
                                                }
                                                else
                                                {
                                                    _3772 = true;
                                                }
                                                bool _3778;
                                                if (!_3772)
                                                {
                                                    _3778 = _1283.x < 0.0;
                                                }
                                                else
                                                {
                                                    _3778 = true;
                                                }
                                                bool _3784;
                                                if (!_3778)
                                                {
                                                    _3784 = _1283.x > 1.0;
                                                }
                                                else
                                                {
                                                    _3784 = true;
                                                }
                                                bool _3790;
                                                if (!_3784)
                                                {
                                                    _3790 = _1283.y < 0.0;
                                                }
                                                else
                                                {
                                                    _3790 = true;
                                                }
                                                bool _3796;
                                                if (!_3790)
                                                {
                                                    _3796 = _1283.y > 7.0;
                                                }
                                                else
                                                {
                                                    _3796 = true;
                                                }
                                                bool _3803;
                                                if (!_3796)
                                                {
                                                    _3803 = floor(_1283.y) != _1283.y;
                                                }
                                                else
                                                {
                                                    _3803 = true;
                                                }
                                                bool _3810;
                                                if (!_3803)
                                                {
                                                    _3810 = any(_1283.zw != float2(0.0));
                                                }
                                                else
                                                {
                                                    _3810 = true;
                                                }
                                                if (_3810)
                                                {
                                                    _3849 = false;
                                                    break;
                                                }
                                                bool _3818;
                                                if (_3631)
                                                {
                                                    _3818 = any(_1154.xyz != float3(0.0));
                                                }
                                                else
                                                {
                                                    _3818 = false;
                                                }
                                                if (_3818)
                                                {
                                                    _3849 = false;
                                                    break;
                                                }
                                                float3 _3831;
                                                if (_3631)
                                                {
                                                    _3831 = _1153.xyz;
                                                }
                                                else
                                                {
                                                    _3831 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                }
                                                float _3832 = dot(_3831, _3831);
                                                bool3 _3833 = isnan(_3831);
                                                bool3 _3834 = isinf(_3831);
                                                bool _3844;
                                                if (all(not(bool3(_3833.x || _3834.x, _3833.y || _3834.y, _3833.z || _3834.z))))
                                                {
                                                    _3844 = !(isnan(_3832) || isinf(_3832));
                                                }
                                                else
                                                {
                                                    _3844 = false;
                                                }
                                                bool _3848;
                                                if (_3844)
                                                {
                                                    _3848 = _3832 > 9.9999996826552253889678874634872e-21;
                                                }
                                                else
                                                {
                                                    _3848 = false;
                                                }
                                                _3849 = _3848;
                                                break;
                                            } while(false);
                                            _3851 = !_3849;
                                        }
                                        else
                                        {
                                            _3851 = true;
                                        }
                                        float3 _4009;
                                        float _4010;
                                        float3 _4011;
                                        bool _4012;
                                        if (!_3851)
                                        {
                                            float3 _4004;
                                            float _4005;
                                            float3 _4006;
                                            bool _4007;
                                            do
                                            {
                                                bool2 _3857 = isnan(_3613);
                                                bool2 _3858 = isinf(_3613);
                                                bool _3866;
                                                if (all(not(bool2(_3857.x || _3858.x, _3857.y || _3858.y))))
                                                {
                                                    _3866 = any(_3613 < float2(0.0));
                                                }
                                                else
                                                {
                                                    _3866 = true;
                                                }
                                                bool _3872;
                                                if (!_3866)
                                                {
                                                    _3872 = any(_3613 >= _2033);
                                                }
                                                else
                                                {
                                                    _3872 = true;
                                                }
                                                if (_3872)
                                                {
                                                    _4004 = float3(0.0);
                                                    _4005 = 0.0;
                                                    _4006 = float3(0.0);
                                                    _4007 = false;
                                                    break;
                                                }
                                                bool _3879;
                                                if (_1998)
                                                {
                                                    _3879 = _1153.w == 2.0;
                                                }
                                                else
                                                {
                                                    _3879 = false;
                                                }
                                                bool _3884;
                                                if (_3879)
                                                {
                                                    _3884 = _1154.w == 2.0;
                                                }
                                                else
                                                {
                                                    _3884 = false;
                                                }
                                                float3 _3895;
                                                if (_3884)
                                                {
                                                    _3895 = _1153.xyz;
                                                }
                                                else
                                                {
                                                    _3895 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                }
                                                float3 _3896 = fast::normalize(_3895);
                                                float3 _3904 = fast::normalize(float3((_3613 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                                float3 _3910;
                                                if (dot(_3896, _3904) > 0.0)
                                                {
                                                    _3910 = -_3896;
                                                }
                                                else
                                                {
                                                    _3910 = _3896;
                                                }
                                                float _3911 = dot(_3904, _3910);
                                                bool _3920;
                                                if (!(isnan(_3911) || isinf(_3911)))
                                                {
                                                    _3920 = abs(_3911) <= 9.9999999600419720025001879548654e-13;
                                                }
                                                else
                                                {
                                                    _3920 = true;
                                                }
                                                if (_3920)
                                                {
                                                    _4004 = float3(0.0);
                                                    _4005 = 0.0;
                                                    _4006 = float3(0.0);
                                                    _4007 = false;
                                                    break;
                                                }
                                                float _3925 = dot(_1152.xyz, _3910) / _3911;
                                                float _3927 = _3925 * _3904.z;
                                                bool _3935;
                                                if (!(isnan(_3925) || isinf(_3925)))
                                                {
                                                    _3935 = _3925 <= 0.0;
                                                }
                                                else
                                                {
                                                    _3935 = true;
                                                }
                                                bool _3940;
                                                if (!_3935)
                                                {
                                                    _3940 = _3927 < Settings.clip.x;
                                                }
                                                else
                                                {
                                                    _3940 = true;
                                                }
                                                bool _3945;
                                                if (!_3940)
                                                {
                                                    _3945 = _3927 > Settings.clip.y;
                                                }
                                                else
                                                {
                                                    _3945 = true;
                                                }
                                                if (_3945)
                                                {
                                                    _4004 = float3(0.0);
                                                    _4005 = 0.0;
                                                    _4006 = float3(0.0);
                                                    _4007 = false;
                                                    break;
                                                }
                                                bool _3952;
                                                if (_1998)
                                                {
                                                    _3952 = _1153.w == 2.0;
                                                }
                                                else
                                                {
                                                    _3952 = false;
                                                }
                                                bool _3957;
                                                if (_3952)
                                                {
                                                    _3957 = _1154.w == 2.0;
                                                }
                                                else
                                                {
                                                    _3957 = false;
                                                }
                                                float _3960 = precise::max(_3957 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _3925 * 9.9999997473787516355514526367188e-06);
                                                float3 _3963 = (_3904 * _3925) + (_3910 * _3960);
                                                float3 _3964 = reflect(_3904, _3910);
                                                uint _3967 = uint(_1283.y);
                                                float3 _3974 = fast::normalize(cross(_3964, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_3964.y) < 0.949999988079071044921875))));
                                                float3 _3986 = fast::normalize(_3964 + (((_3974 * _175[_3967].x) + (cross(_3964, _3974) * _175[_3967].y)) * (_1283.x * _1283.x)));
                                                float3 _3990 = select(_3986, _3964, bool3(dot(_3986, _3910) <= 0.0));
                                                bool3 _3991 = isnan(_3963);
                                                bool3 _3992 = isinf(_3963);
                                                bool _4003;
                                                if (all(not(bool3(_3991.x || _3992.x, _3991.y || _3992.y, _3991.z || _3992.z))))
                                                {
                                                    bool3 _3998 = isnan(_3990);
                                                    bool3 _3999 = isinf(_3990);
                                                    _4003 = all(not(bool3(_3998.x || _3999.x, _3998.y || _3999.y, _3998.z || _3999.z)));
                                                }
                                                else
                                                {
                                                    _4003 = false;
                                                }
                                                _4004 = _3990;
                                                _4005 = _3960;
                                                _4006 = _3963;
                                                _4007 = _4003;
                                                break;
                                            } while(false);
                                            _4009 = _4004;
                                            _4010 = _4005;
                                            _4011 = _4006;
                                            _4012 = !_4007;
                                        }
                                        else
                                        {
                                            _4009 = float3(0.0);
                                            _4010 = 0.0;
                                            _4011 = float3(0.0);
                                            _4012 = true;
                                        }
                                        if (_4012)
                                        {
                                            _4252 = _4009;
                                            _4253 = _4010;
                                            _4254 = _4011;
                                            _2072 = false;
                                            break;
                                        }
                                        float3 _4016;
                                        float3 _4021;
                                        _4016 = _4009;
                                        _4021 = _4011;
                                        float3 _4017;
                                        float _4020;
                                        float3 _4022;
                                        float3 _4246;
                                        float _4247;
                                        float3 _4248;
                                        bool _4249;
                                        bool _4250;
                                        float _4019 = _4010;
                                        uint _4023 = 0u;
                                        for (;;)
                                        {
                                            if (_4023 < _929)
                                            {
                                                bool _4036;
                                                bool _4155;
                                                do
                                                {
                                                    _4036 = _201[_4023].a.w == 2.0;
                                                    bool _4041;
                                                    if (_4036)
                                                    {
                                                        _4041 = _201[_4023].b.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4041 = false;
                                                    }
                                                    bool _4046;
                                                    if (_4041)
                                                    {
                                                        _4046 = _201[_4023].c.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4046 = false;
                                                    }
                                                    bool _4062;
                                                    if (!_4046)
                                                    {
                                                        bool _4055;
                                                        if ((isunordered(_201[_4023].a.w, 1.0) || _201[_4023].a.w == 1.0))
                                                        {
                                                            _4055 = _201[_4023].b.w != 1.0;
                                                        }
                                                        else
                                                        {
                                                            _4055 = true;
                                                        }
                                                        bool _4061;
                                                        if (!_4055)
                                                        {
                                                            _4061 = _201[_4023].c.w != 1.0;
                                                        }
                                                        else
                                                        {
                                                            _4061 = true;
                                                        }
                                                        _4062 = _4061;
                                                    }
                                                    else
                                                    {
                                                        _4062 = false;
                                                    }
                                                    bool _4072;
                                                    if (!_4062)
                                                    {
                                                        bool4 _4066 = isnan(_201[_4023].a);
                                                        bool4 _4067 = isinf(_201[_4023].a);
                                                        _4072 = !all(not(bool4(_4066.x || _4067.x, _4066.y || _4067.y, _4066.z || _4067.z, _4066.w || _4067.w)));
                                                    }
                                                    else
                                                    {
                                                        _4072 = true;
                                                    }
                                                    bool _4082;
                                                    if (!_4072)
                                                    {
                                                        bool4 _4076 = isnan(_201[_4023].b);
                                                        bool4 _4077 = isinf(_201[_4023].b);
                                                        _4082 = !all(not(bool4(_4076.x || _4077.x, _4076.y || _4077.y, _4076.z || _4077.z, _4076.w || _4077.w)));
                                                    }
                                                    else
                                                    {
                                                        _4082 = true;
                                                    }
                                                    bool _4092;
                                                    if (!_4082)
                                                    {
                                                        bool4 _4086 = isnan(_201[_4023].c);
                                                        bool4 _4087 = isinf(_201[_4023].c);
                                                        _4092 = !all(not(bool4(_4086.x || _4087.x, _4086.y || _4087.y, _4086.z || _4087.z, _4086.w || _4087.w)));
                                                    }
                                                    else
                                                    {
                                                        _4092 = true;
                                                    }
                                                    bool _4100;
                                                    if (!_4092)
                                                    {
                                                        _4100 = any(abs(_201[_4023].a.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _4100 = true;
                                                    }
                                                    bool _4108;
                                                    if (!_4100)
                                                    {
                                                        _4108 = any(abs(_201[_4023].b.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _4108 = true;
                                                    }
                                                    bool _4116;
                                                    if (!_4108)
                                                    {
                                                        _4116 = any(abs(_201[_4023].c.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _4116 = true;
                                                    }
                                                    if (_4116)
                                                    {
                                                        _4155 = false;
                                                        break;
                                                    }
                                                    bool _4124;
                                                    if (_4046)
                                                    {
                                                        _4124 = any(_201[_4023].c.xyz != float3(0.0));
                                                    }
                                                    else
                                                    {
                                                        _4124 = false;
                                                    }
                                                    if (_4124)
                                                    {
                                                        _4155 = false;
                                                        break;
                                                    }
                                                    float3 _4137;
                                                    if (_4046)
                                                    {
                                                        _4137 = _201[_4023].b.xyz;
                                                    }
                                                    else
                                                    {
                                                        _4137 = cross(_201[_4023].b.xyz - _201[_4023].a.xyz, _201[_4023].c.xyz - _201[_4023].a.xyz);
                                                    }
                                                    float _4138 = dot(_4137, _4137);
                                                    bool3 _4139 = isnan(_4137);
                                                    bool3 _4140 = isinf(_4137);
                                                    bool _4150;
                                                    if (all(not(bool3(_4139.x || _4140.x, _4139.y || _4140.y, _4139.z || _4140.z))))
                                                    {
                                                        _4150 = !(isnan(_4138) || isinf(_4138));
                                                    }
                                                    else
                                                    {
                                                        _4150 = false;
                                                    }
                                                    bool _4154;
                                                    if (_4150)
                                                    {
                                                        _4154 = _4138 > 9.9999996826552253889678874634872e-21;
                                                    }
                                                    else
                                                    {
                                                        _4154 = false;
                                                    }
                                                    _4155 = _4154;
                                                    break;
                                                } while(false);
                                                if (!_4155)
                                                {
                                                    _4246 = _4016;
                                                    _4247 = _4019;
                                                    _4248 = _4021;
                                                    _4249 = false;
                                                    _4250 = true;
                                                    break;
                                                }
                                                bool _4163;
                                                if (_4036)
                                                {
                                                    _4163 = _201[_4023].b.w == 2.0;
                                                }
                                                else
                                                {
                                                    _4163 = false;
                                                }
                                                bool _4168;
                                                if (_4163)
                                                {
                                                    _4168 = _201[_4023].c.w == 2.0;
                                                }
                                                else
                                                {
                                                    _4168 = false;
                                                }
                                                float3 _4179;
                                                if (_4168)
                                                {
                                                    _4179 = _201[_4023].b.xyz;
                                                }
                                                else
                                                {
                                                    _4179 = cross(_201[_4023].b.xyz - _201[_4023].a.xyz, _201[_4023].c.xyz - _201[_4023].a.xyz);
                                                }
                                                float3 _4180 = fast::normalize(_4179);
                                                float3 _4186;
                                                if (dot(_4180, _4016) > 0.0)
                                                {
                                                    _4186 = -_4180;
                                                }
                                                else
                                                {
                                                    _4186 = _4180;
                                                }
                                                float _4187 = dot(_4016, _4186);
                                                bool _4196;
                                                if (!(isnan(_4187) || isinf(_4187)))
                                                {
                                                    _4196 = abs(_4187) <= 9.9999999600419720025001879548654e-13;
                                                }
                                                else
                                                {
                                                    _4196 = true;
                                                }
                                                if (_4196)
                                                {
                                                    _4246 = _4016;
                                                    _4247 = _4019;
                                                    _4248 = _4021;
                                                    _4249 = false;
                                                    _4250 = true;
                                                    break;
                                                }
                                                float _4202 = dot(_201[_4023].a.xyz - _4021, _4186) / _4187;
                                                bool _4210;
                                                if (!(isnan(_4202) || isinf(_4202)))
                                                {
                                                    _4210 = _4202 <= _4019;
                                                }
                                                else
                                                {
                                                    _4210 = true;
                                                }
                                                bool _4215;
                                                if (!_4210)
                                                {
                                                    _4215 = _4202 >= 65536.0;
                                                }
                                                else
                                                {
                                                    _4215 = true;
                                                }
                                                if (_4215)
                                                {
                                                    _4246 = _4016;
                                                    _4247 = _4019;
                                                    _4248 = _4021;
                                                    _4249 = false;
                                                    _4250 = true;
                                                    break;
                                                }
                                                float3 _4219 = _4021 + (_4016 * _4202);
                                                bool3 _4220 = isnan(_4219);
                                                bool3 _4221 = isinf(_4219);
                                                if (!all(not(bool3(_4220.x || _4221.x, _4220.y || _4221.y, _4220.z || _4221.z))))
                                                {
                                                    _4246 = _4016;
                                                    _4247 = _4019;
                                                    _4248 = _4021;
                                                    _4249 = false;
                                                    _4250 = true;
                                                    break;
                                                }
                                                _4020 = precise::max(0.0500000007450580596923828125, _4202 * 9.9999997473787516355514526367188e-06);
                                                _4022 = _4219 + (_4186 * _4020);
                                                _4017 = reflect(_4016, _4186);
                                                bool3 _4230 = isnan(_4022);
                                                bool3 _4231 = isinf(_4022);
                                                bool _4243;
                                                if (all(not(bool3(_4230.x || _4231.x, _4230.y || _4231.y, _4230.z || _4231.z))))
                                                {
                                                    bool3 _4237 = isnan(_4017);
                                                    bool3 _4238 = isinf(_4017);
                                                    _4243 = !all(not(bool3(_4237.x || _4238.x, _4237.y || _4238.y, _4237.z || _4238.z)));
                                                }
                                                else
                                                {
                                                    _4243 = true;
                                                }
                                                if (_4243)
                                                {
                                                    _4246 = _4017;
                                                    _4247 = _4020;
                                                    _4248 = _4022;
                                                    _4249 = false;
                                                    _4250 = true;
                                                    break;
                                                }
                                                _4016 = _4017;
                                                _4019 = _4020;
                                                _4021 = _4022;
                                                _4023++;
                                                continue;
                                            }
                                            else
                                            {
                                                _4246 = _4016;
                                                _4247 = _4019;
                                                _4248 = _4021;
                                                _4249 = _2071;
                                                _4250 = false;
                                                break;
                                            }
                                        }
                                        if (_4250)
                                        {
                                            _4252 = _4246;
                                            _4253 = _4247;
                                            _4254 = _4248;
                                            _2072 = _4249;
                                            break;
                                        }
                                        _4252 = _4246;
                                        _4253 = _4247;
                                        _4254 = _4248;
                                        _2072 = true;
                                        break;
                                    } while(false);
                                    if (!_2072)
                                    {
                                        _4306 = float2(0.0);
                                        _4307 = false;
                                        break;
                                    }
                                    bool _4258 = _1521.w == 0.0;
                                    float3 _4263;
                                    if (_4258)
                                    {
                                        _4263 = _1521.xyz;
                                    }
                                    else
                                    {
                                        _4263 = _1521.xyz - _4254;
                                    }
                                    float _4264 = dot(_4263, _4263);
                                    bool _4277;
                                    if (!(isnan(_4264) || isinf(_4264)))
                                    {
                                        float _4275;
                                        if (_4258)
                                        {
                                            _4275 = 9.9999996826552253889678874634872e-21;
                                        }
                                        else
                                        {
                                            _4275 = _4253 * _4253;
                                        }
                                        _4277 = _4264 <= _4275;
                                    }
                                    else
                                    {
                                        _4277 = true;
                                    }
                                    if (_4277)
                                    {
                                        _4306 = float2(0.0);
                                        _4307 = false;
                                        break;
                                    }
                                    float3 _4281 = _4263 * rsqrt(_4264);
                                    if (dot(_4281, _4252) <= 0.0)
                                    {
                                        _4306 = float2(0.0);
                                        _4307 = false;
                                        break;
                                    }
                                    float3 _4292 = fast::normalize(cross(_4252, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_4252.y) < 0.949999988079071044921875))));
                                    float2 _4300 = float2(dot(_4281, _4292), dot(_4281, cross(_4252, _4292))) * precise::max(Settings.projection.x, Settings.projection.y);
                                    bool2 _4301 = isnan(_4300);
                                    bool2 _4302 = isinf(_4300);
                                    _4306 = _4300;
                                    _4307 = all(not(bool2(_4301.x || _4302.x, _4301.y || _4302.y)));
                                    break;
                                } while(false);
                                bool _5008;
                                if (_4307)
                                {
                                    float2 _4311 = _2062 + float2(0.0, _3611);
                                    bool _4953;
                                    float2 _5005;
                                    bool _5006;
                                    do
                                    {
                                        spvUnsafeArray<ReflectionSpecularPlane, 4> _199 = _206;
                                        float3 _4950;
                                        float _4951;
                                        float3 _4952;
                                        do
                                        {
                                            bool _4549;
                                            if (_1525)
                                            {
                                                bool _4547;
                                                do
                                                {
                                                    bool _4324;
                                                    if (_1998)
                                                    {
                                                        _4324 = _1153.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4324 = false;
                                                    }
                                                    bool _4329;
                                                    if (_4324)
                                                    {
                                                        _4329 = _1154.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4329 = false;
                                                    }
                                                    bool _4345;
                                                    if (!_4329)
                                                    {
                                                        bool _4338;
                                                        if ((isunordered(_1152.w, 1.0) || _1152.w == 1.0))
                                                        {
                                                            _4338 = _1153.w != 1.0;
                                                        }
                                                        else
                                                        {
                                                            _4338 = true;
                                                        }
                                                        bool _4344;
                                                        if (!_4338)
                                                        {
                                                            _4344 = _1154.w != 1.0;
                                                        }
                                                        else
                                                        {
                                                            _4344 = true;
                                                        }
                                                        _4345 = _4344;
                                                    }
                                                    else
                                                    {
                                                        _4345 = false;
                                                    }
                                                    bool _4355;
                                                    if (!_4345)
                                                    {
                                                        bool4 _4349 = isnan(_1152);
                                                        bool4 _4350 = isinf(_1152);
                                                        _4355 = !all(not(bool4(_4349.x || _4350.x, _4349.y || _4350.y, _4349.z || _4350.z, _4349.w || _4350.w)));
                                                    }
                                                    else
                                                    {
                                                        _4355 = true;
                                                    }
                                                    bool _4365;
                                                    if (!_4355)
                                                    {
                                                        bool4 _4359 = isnan(_1153);
                                                        bool4 _4360 = isinf(_1153);
                                                        _4365 = !all(not(bool4(_4359.x || _4360.x, _4359.y || _4360.y, _4359.z || _4360.z, _4359.w || _4360.w)));
                                                    }
                                                    else
                                                    {
                                                        _4365 = true;
                                                    }
                                                    bool _4375;
                                                    if (!_4365)
                                                    {
                                                        bool4 _4369 = isnan(_1154);
                                                        bool4 _4370 = isinf(_1154);
                                                        _4375 = !all(not(bool4(_4369.x || _4370.x, _4369.y || _4370.y, _4369.z || _4370.z, _4369.w || _4370.w)));
                                                    }
                                                    else
                                                    {
                                                        _4375 = true;
                                                    }
                                                    bool _4385;
                                                    if (!_4375)
                                                    {
                                                        bool4 _4379 = isnan(Settings.projection);
                                                        bool4 _4380 = isinf(Settings.projection);
                                                        _4385 = !all(not(bool4(_4379.x || _4380.x, _4379.y || _4380.y, _4379.z || _4380.z, _4379.w || _4380.w)));
                                                    }
                                                    else
                                                    {
                                                        _4385 = true;
                                                    }
                                                    bool _4395;
                                                    if (!_4385)
                                                    {
                                                        bool4 _4389 = isnan(_1275);
                                                        bool4 _4390 = isinf(_1275);
                                                        _4395 = !all(not(bool4(_4389.x || _4390.x, _4389.y || _4390.y, _4389.z || _4390.z, _4389.w || _4390.w)));
                                                    }
                                                    else
                                                    {
                                                        _4395 = true;
                                                    }
                                                    bool _4405;
                                                    if (!_4395)
                                                    {
                                                        bool4 _4399 = isnan(_1283);
                                                        bool4 _4400 = isinf(_1283);
                                                        _4405 = !all(not(bool4(_4399.x || _4400.x, _4399.y || _4400.y, _4399.z || _4400.z, _4399.w || _4400.w)));
                                                    }
                                                    else
                                                    {
                                                        _4405 = true;
                                                    }
                                                    bool _4413;
                                                    if (!_4405)
                                                    {
                                                        _4413 = any(abs(_1152.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _4413 = true;
                                                    }
                                                    bool _4421;
                                                    if (!_4413)
                                                    {
                                                        _4421 = any(abs(_1153.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _4421 = true;
                                                    }
                                                    bool _4429;
                                                    if (!_4421)
                                                    {
                                                        _4429 = any(abs(_1154.xyz) > float3(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _4429 = true;
                                                    }
                                                    bool _4436;
                                                    if (!_4429)
                                                    {
                                                        _4436 = any(Settings.projection.xy <= float2(0.0));
                                                    }
                                                    else
                                                    {
                                                        _4436 = true;
                                                    }
                                                    bool _4443;
                                                    if (!_4436)
                                                    {
                                                        _4443 = any(abs(Settings.projection) > float4(999999995904.0));
                                                    }
                                                    else
                                                    {
                                                        _4443 = true;
                                                    }
                                                    bool _4449;
                                                    if (!_4443)
                                                    {
                                                        _4449 = any(_2033 < float2(1.0));
                                                    }
                                                    else
                                                    {
                                                        _4449 = true;
                                                    }
                                                    bool _4455;
                                                    if (!_4449)
                                                    {
                                                        _4455 = any(_2033 > float2(16384.0));
                                                    }
                                                    else
                                                    {
                                                        _4455 = true;
                                                    }
                                                    bool _4460;
                                                    if (!_4455)
                                                    {
                                                        _4460 = Settings.clip.x <= 0.0;
                                                    }
                                                    else
                                                    {
                                                        _4460 = true;
                                                    }
                                                    bool _4465;
                                                    if (!_4460)
                                                    {
                                                        _4465 = Settings.clip.y <= Settings.clip.x;
                                                    }
                                                    else
                                                    {
                                                        _4465 = true;
                                                    }
                                                    bool _4470;
                                                    if (!_4465)
                                                    {
                                                        _4470 = Settings.clip.y > 999999995904.0;
                                                    }
                                                    else
                                                    {
                                                        _4470 = true;
                                                    }
                                                    bool _4476;
                                                    if (!_4470)
                                                    {
                                                        _4476 = _1283.x < 0.0;
                                                    }
                                                    else
                                                    {
                                                        _4476 = true;
                                                    }
                                                    bool _4482;
                                                    if (!_4476)
                                                    {
                                                        _4482 = _1283.x > 1.0;
                                                    }
                                                    else
                                                    {
                                                        _4482 = true;
                                                    }
                                                    bool _4488;
                                                    if (!_4482)
                                                    {
                                                        _4488 = _1283.y < 0.0;
                                                    }
                                                    else
                                                    {
                                                        _4488 = true;
                                                    }
                                                    bool _4494;
                                                    if (!_4488)
                                                    {
                                                        _4494 = _1283.y > 7.0;
                                                    }
                                                    else
                                                    {
                                                        _4494 = true;
                                                    }
                                                    bool _4501;
                                                    if (!_4494)
                                                    {
                                                        _4501 = floor(_1283.y) != _1283.y;
                                                    }
                                                    else
                                                    {
                                                        _4501 = true;
                                                    }
                                                    bool _4508;
                                                    if (!_4501)
                                                    {
                                                        _4508 = any(_1283.zw != float2(0.0));
                                                    }
                                                    else
                                                    {
                                                        _4508 = true;
                                                    }
                                                    if (_4508)
                                                    {
                                                        _4547 = false;
                                                        break;
                                                    }
                                                    bool _4516;
                                                    if (_4329)
                                                    {
                                                        _4516 = any(_1154.xyz != float3(0.0));
                                                    }
                                                    else
                                                    {
                                                        _4516 = false;
                                                    }
                                                    if (_4516)
                                                    {
                                                        _4547 = false;
                                                        break;
                                                    }
                                                    float3 _4529;
                                                    if (_4329)
                                                    {
                                                        _4529 = _1153.xyz;
                                                    }
                                                    else
                                                    {
                                                        _4529 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                    }
                                                    float _4530 = dot(_4529, _4529);
                                                    bool3 _4531 = isnan(_4529);
                                                    bool3 _4532 = isinf(_4529);
                                                    bool _4542;
                                                    if (all(not(bool3(_4531.x || _4532.x, _4531.y || _4532.y, _4531.z || _4532.z))))
                                                    {
                                                        _4542 = !(isnan(_4530) || isinf(_4530));
                                                    }
                                                    else
                                                    {
                                                        _4542 = false;
                                                    }
                                                    bool _4546;
                                                    if (_4542)
                                                    {
                                                        _4546 = _4530 > 9.9999996826552253889678874634872e-21;
                                                    }
                                                    else
                                                    {
                                                        _4546 = false;
                                                    }
                                                    _4547 = _4546;
                                                    break;
                                                } while(false);
                                                _4549 = !_4547;
                                            }
                                            else
                                            {
                                                _4549 = true;
                                            }
                                            float3 _4707;
                                            float _4708;
                                            float3 _4709;
                                            bool _4710;
                                            if (!_4549)
                                            {
                                                float3 _4702;
                                                float _4703;
                                                float3 _4704;
                                                bool _4705;
                                                do
                                                {
                                                    bool2 _4555 = isnan(_4311);
                                                    bool2 _4556 = isinf(_4311);
                                                    bool _4564;
                                                    if (all(not(bool2(_4555.x || _4556.x, _4555.y || _4556.y))))
                                                    {
                                                        _4564 = any(_4311 < float2(0.0));
                                                    }
                                                    else
                                                    {
                                                        _4564 = true;
                                                    }
                                                    bool _4570;
                                                    if (!_4564)
                                                    {
                                                        _4570 = any(_4311 >= _2033);
                                                    }
                                                    else
                                                    {
                                                        _4570 = true;
                                                    }
                                                    if (_4570)
                                                    {
                                                        _4702 = float3(0.0);
                                                        _4703 = 0.0;
                                                        _4704 = float3(0.0);
                                                        _4705 = false;
                                                        break;
                                                    }
                                                    bool _4577;
                                                    if (_1998)
                                                    {
                                                        _4577 = _1153.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4577 = false;
                                                    }
                                                    bool _4582;
                                                    if (_4577)
                                                    {
                                                        _4582 = _1154.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4582 = false;
                                                    }
                                                    float3 _4593;
                                                    if (_4582)
                                                    {
                                                        _4593 = _1153.xyz;
                                                    }
                                                    else
                                                    {
                                                        _4593 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                    }
                                                    float3 _4594 = fast::normalize(_4593);
                                                    float3 _4602 = fast::normalize(float3((_4311 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                                    float3 _4608;
                                                    if (dot(_4594, _4602) > 0.0)
                                                    {
                                                        _4608 = -_4594;
                                                    }
                                                    else
                                                    {
                                                        _4608 = _4594;
                                                    }
                                                    float _4609 = dot(_4602, _4608);
                                                    bool _4618;
                                                    if (!(isnan(_4609) || isinf(_4609)))
                                                    {
                                                        _4618 = abs(_4609) <= 9.9999999600419720025001879548654e-13;
                                                    }
                                                    else
                                                    {
                                                        _4618 = true;
                                                    }
                                                    if (_4618)
                                                    {
                                                        _4702 = float3(0.0);
                                                        _4703 = 0.0;
                                                        _4704 = float3(0.0);
                                                        _4705 = false;
                                                        break;
                                                    }
                                                    float _4623 = dot(_1152.xyz, _4608) / _4609;
                                                    float _4625 = _4623 * _4602.z;
                                                    bool _4633;
                                                    if (!(isnan(_4623) || isinf(_4623)))
                                                    {
                                                        _4633 = _4623 <= 0.0;
                                                    }
                                                    else
                                                    {
                                                        _4633 = true;
                                                    }
                                                    bool _4638;
                                                    if (!_4633)
                                                    {
                                                        _4638 = _4625 < Settings.clip.x;
                                                    }
                                                    else
                                                    {
                                                        _4638 = true;
                                                    }
                                                    bool _4643;
                                                    if (!_4638)
                                                    {
                                                        _4643 = _4625 > Settings.clip.y;
                                                    }
                                                    else
                                                    {
                                                        _4643 = true;
                                                    }
                                                    if (_4643)
                                                    {
                                                        _4702 = float3(0.0);
                                                        _4703 = 0.0;
                                                        _4704 = float3(0.0);
                                                        _4705 = false;
                                                        break;
                                                    }
                                                    bool _4650;
                                                    if (_1998)
                                                    {
                                                        _4650 = _1153.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4650 = false;
                                                    }
                                                    bool _4655;
                                                    if (_4650)
                                                    {
                                                        _4655 = _1154.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4655 = false;
                                                    }
                                                    float _4658 = precise::max(_4655 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _4623 * 9.9999997473787516355514526367188e-06);
                                                    float3 _4661 = (_4602 * _4623) + (_4608 * _4658);
                                                    float3 _4662 = reflect(_4602, _4608);
                                                    uint _4665 = uint(_1283.y);
                                                    float3 _4672 = fast::normalize(cross(_4662, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_4662.y) < 0.949999988079071044921875))));
                                                    float3 _4684 = fast::normalize(_4662 + (((_4672 * _175[_4665].x) + (cross(_4662, _4672) * _175[_4665].y)) * (_1283.x * _1283.x)));
                                                    float3 _4688 = select(_4684, _4662, bool3(dot(_4684, _4608) <= 0.0));
                                                    bool3 _4689 = isnan(_4661);
                                                    bool3 _4690 = isinf(_4661);
                                                    bool _4701;
                                                    if (all(not(bool3(_4689.x || _4690.x, _4689.y || _4690.y, _4689.z || _4690.z))))
                                                    {
                                                        bool3 _4696 = isnan(_4688);
                                                        bool3 _4697 = isinf(_4688);
                                                        _4701 = all(not(bool3(_4696.x || _4697.x, _4696.y || _4697.y, _4696.z || _4697.z)));
                                                    }
                                                    else
                                                    {
                                                        _4701 = false;
                                                    }
                                                    _4702 = _4688;
                                                    _4703 = _4658;
                                                    _4704 = _4661;
                                                    _4705 = _4701;
                                                    break;
                                                } while(false);
                                                _4707 = _4702;
                                                _4708 = _4703;
                                                _4709 = _4704;
                                                _4710 = !_4705;
                                            }
                                            else
                                            {
                                                _4707 = float3(0.0);
                                                _4708 = 0.0;
                                                _4709 = float3(0.0);
                                                _4710 = true;
                                            }
                                            if (_4710)
                                            {
                                                _4950 = _4707;
                                                _4951 = _4708;
                                                _4952 = _4709;
                                                _4953 = false;
                                                break;
                                            }
                                            float3 _4714;
                                            float3 _4719;
                                            _4714 = _4707;
                                            _4719 = _4709;
                                            float3 _4715;
                                            float _4718;
                                            float3 _4720;
                                            float3 _4944;
                                            float _4945;
                                            float3 _4946;
                                            bool _4947;
                                            bool _4948;
                                            float _4717 = _4708;
                                            uint _4721 = 0u;
                                            for (;;)
                                            {
                                                if (_4721 < _929)
                                                {
                                                    bool _4734;
                                                    bool _4853;
                                                    do
                                                    {
                                                        _4734 = _199[_4721].a.w == 2.0;
                                                        bool _4739;
                                                        if (_4734)
                                                        {
                                                            _4739 = _199[_4721].b.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _4739 = false;
                                                        }
                                                        bool _4744;
                                                        if (_4739)
                                                        {
                                                            _4744 = _199[_4721].c.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _4744 = false;
                                                        }
                                                        bool _4760;
                                                        if (!_4744)
                                                        {
                                                            bool _4753;
                                                            if ((isunordered(_199[_4721].a.w, 1.0) || _199[_4721].a.w == 1.0))
                                                            {
                                                                _4753 = _199[_4721].b.w != 1.0;
                                                            }
                                                            else
                                                            {
                                                                _4753 = true;
                                                            }
                                                            bool _4759;
                                                            if (!_4753)
                                                            {
                                                                _4759 = _199[_4721].c.w != 1.0;
                                                            }
                                                            else
                                                            {
                                                                _4759 = true;
                                                            }
                                                            _4760 = _4759;
                                                        }
                                                        else
                                                        {
                                                            _4760 = false;
                                                        }
                                                        bool _4770;
                                                        if (!_4760)
                                                        {
                                                            bool4 _4764 = isnan(_199[_4721].a);
                                                            bool4 _4765 = isinf(_199[_4721].a);
                                                            _4770 = !all(not(bool4(_4764.x || _4765.x, _4764.y || _4765.y, _4764.z || _4765.z, _4764.w || _4765.w)));
                                                        }
                                                        else
                                                        {
                                                            _4770 = true;
                                                        }
                                                        bool _4780;
                                                        if (!_4770)
                                                        {
                                                            bool4 _4774 = isnan(_199[_4721].b);
                                                            bool4 _4775 = isinf(_199[_4721].b);
                                                            _4780 = !all(not(bool4(_4774.x || _4775.x, _4774.y || _4775.y, _4774.z || _4775.z, _4774.w || _4775.w)));
                                                        }
                                                        else
                                                        {
                                                            _4780 = true;
                                                        }
                                                        bool _4790;
                                                        if (!_4780)
                                                        {
                                                            bool4 _4784 = isnan(_199[_4721].c);
                                                            bool4 _4785 = isinf(_199[_4721].c);
                                                            _4790 = !all(not(bool4(_4784.x || _4785.x, _4784.y || _4785.y, _4784.z || _4785.z, _4784.w || _4785.w)));
                                                        }
                                                        else
                                                        {
                                                            _4790 = true;
                                                        }
                                                        bool _4798;
                                                        if (!_4790)
                                                        {
                                                            _4798 = any(abs(_199[_4721].a.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _4798 = true;
                                                        }
                                                        bool _4806;
                                                        if (!_4798)
                                                        {
                                                            _4806 = any(abs(_199[_4721].b.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _4806 = true;
                                                        }
                                                        bool _4814;
                                                        if (!_4806)
                                                        {
                                                            _4814 = any(abs(_199[_4721].c.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _4814 = true;
                                                        }
                                                        if (_4814)
                                                        {
                                                            _4853 = false;
                                                            break;
                                                        }
                                                        bool _4822;
                                                        if (_4744)
                                                        {
                                                            _4822 = any(_199[_4721].c.xyz != float3(0.0));
                                                        }
                                                        else
                                                        {
                                                            _4822 = false;
                                                        }
                                                        if (_4822)
                                                        {
                                                            _4853 = false;
                                                            break;
                                                        }
                                                        float3 _4835;
                                                        if (_4744)
                                                        {
                                                            _4835 = _199[_4721].b.xyz;
                                                        }
                                                        else
                                                        {
                                                            _4835 = cross(_199[_4721].b.xyz - _199[_4721].a.xyz, _199[_4721].c.xyz - _199[_4721].a.xyz);
                                                        }
                                                        float _4836 = dot(_4835, _4835);
                                                        bool3 _4837 = isnan(_4835);
                                                        bool3 _4838 = isinf(_4835);
                                                        bool _4848;
                                                        if (all(not(bool3(_4837.x || _4838.x, _4837.y || _4838.y, _4837.z || _4838.z))))
                                                        {
                                                            _4848 = !(isnan(_4836) || isinf(_4836));
                                                        }
                                                        else
                                                        {
                                                            _4848 = false;
                                                        }
                                                        bool _4852;
                                                        if (_4848)
                                                        {
                                                            _4852 = _4836 > 9.9999996826552253889678874634872e-21;
                                                        }
                                                        else
                                                        {
                                                            _4852 = false;
                                                        }
                                                        _4853 = _4852;
                                                        break;
                                                    } while(false);
                                                    if (!_4853)
                                                    {
                                                        _4944 = _4714;
                                                        _4945 = _4717;
                                                        _4946 = _4719;
                                                        _4947 = false;
                                                        _4948 = true;
                                                        break;
                                                    }
                                                    bool _4861;
                                                    if (_4734)
                                                    {
                                                        _4861 = _199[_4721].b.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4861 = false;
                                                    }
                                                    bool _4866;
                                                    if (_4861)
                                                    {
                                                        _4866 = _199[_4721].c.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _4866 = false;
                                                    }
                                                    float3 _4877;
                                                    if (_4866)
                                                    {
                                                        _4877 = _199[_4721].b.xyz;
                                                    }
                                                    else
                                                    {
                                                        _4877 = cross(_199[_4721].b.xyz - _199[_4721].a.xyz, _199[_4721].c.xyz - _199[_4721].a.xyz);
                                                    }
                                                    float3 _4878 = fast::normalize(_4877);
                                                    float3 _4884;
                                                    if (dot(_4878, _4714) > 0.0)
                                                    {
                                                        _4884 = -_4878;
                                                    }
                                                    else
                                                    {
                                                        _4884 = _4878;
                                                    }
                                                    float _4885 = dot(_4714, _4884);
                                                    bool _4894;
                                                    if (!(isnan(_4885) || isinf(_4885)))
                                                    {
                                                        _4894 = abs(_4885) <= 9.9999999600419720025001879548654e-13;
                                                    }
                                                    else
                                                    {
                                                        _4894 = true;
                                                    }
                                                    if (_4894)
                                                    {
                                                        _4944 = _4714;
                                                        _4945 = _4717;
                                                        _4946 = _4719;
                                                        _4947 = false;
                                                        _4948 = true;
                                                        break;
                                                    }
                                                    float _4900 = dot(_199[_4721].a.xyz - _4719, _4884) / _4885;
                                                    bool _4908;
                                                    if (!(isnan(_4900) || isinf(_4900)))
                                                    {
                                                        _4908 = _4900 <= _4717;
                                                    }
                                                    else
                                                    {
                                                        _4908 = true;
                                                    }
                                                    bool _4913;
                                                    if (!_4908)
                                                    {
                                                        _4913 = _4900 >= 65536.0;
                                                    }
                                                    else
                                                    {
                                                        _4913 = true;
                                                    }
                                                    if (_4913)
                                                    {
                                                        _4944 = _4714;
                                                        _4945 = _4717;
                                                        _4946 = _4719;
                                                        _4947 = false;
                                                        _4948 = true;
                                                        break;
                                                    }
                                                    float3 _4917 = _4719 + (_4714 * _4900);
                                                    bool3 _4918 = isnan(_4917);
                                                    bool3 _4919 = isinf(_4917);
                                                    if (!all(not(bool3(_4918.x || _4919.x, _4918.y || _4919.y, _4918.z || _4919.z))))
                                                    {
                                                        _4944 = _4714;
                                                        _4945 = _4717;
                                                        _4946 = _4719;
                                                        _4947 = false;
                                                        _4948 = true;
                                                        break;
                                                    }
                                                    _4718 = precise::max(0.0500000007450580596923828125, _4900 * 9.9999997473787516355514526367188e-06);
                                                    _4720 = _4917 + (_4884 * _4718);
                                                    _4715 = reflect(_4714, _4884);
                                                    bool3 _4928 = isnan(_4720);
                                                    bool3 _4929 = isinf(_4720);
                                                    bool _4941;
                                                    if (all(not(bool3(_4928.x || _4929.x, _4928.y || _4929.y, _4928.z || _4929.z))))
                                                    {
                                                        bool3 _4935 = isnan(_4715);
                                                        bool3 _4936 = isinf(_4715);
                                                        _4941 = !all(not(bool3(_4935.x || _4936.x, _4935.y || _4936.y, _4935.z || _4936.z)));
                                                    }
                                                    else
                                                    {
                                                        _4941 = true;
                                                    }
                                                    if (_4941)
                                                    {
                                                        _4944 = _4715;
                                                        _4945 = _4718;
                                                        _4946 = _4720;
                                                        _4947 = false;
                                                        _4948 = true;
                                                        break;
                                                    }
                                                    _4714 = _4715;
                                                    _4717 = _4718;
                                                    _4719 = _4720;
                                                    _4721++;
                                                    continue;
                                                }
                                                else
                                                {
                                                    _4944 = _4714;
                                                    _4945 = _4717;
                                                    _4946 = _4719;
                                                    _4947 = _2069;
                                                    _4948 = false;
                                                    break;
                                                }
                                            }
                                            if (_4948)
                                            {
                                                _4950 = _4944;
                                                _4951 = _4945;
                                                _4952 = _4946;
                                                _4953 = _4947;
                                                break;
                                            }
                                            _4950 = _4944;
                                            _4951 = _4945;
                                            _4952 = _4946;
                                            _4953 = true;
                                            break;
                                        } while(false);
                                        if (!_4953)
                                        {
                                            _5005 = float2(0.0);
                                            _5006 = false;
                                            break;
                                        }
                                        bool _4957 = _1521.w == 0.0;
                                        float3 _4962;
                                        if (_4957)
                                        {
                                            _4962 = _1521.xyz;
                                        }
                                        else
                                        {
                                            _4962 = _1521.xyz - _4952;
                                        }
                                        float _4963 = dot(_4962, _4962);
                                        bool _4976;
                                        if (!(isnan(_4963) || isinf(_4963)))
                                        {
                                            float _4974;
                                            if (_4957)
                                            {
                                                _4974 = 9.9999996826552253889678874634872e-21;
                                            }
                                            else
                                            {
                                                _4974 = _4951 * _4951;
                                            }
                                            _4976 = _4963 <= _4974;
                                        }
                                        else
                                        {
                                            _4976 = true;
                                        }
                                        if (_4976)
                                        {
                                            _5005 = float2(0.0);
                                            _5006 = false;
                                            break;
                                        }
                                        float3 _4980 = _4962 * rsqrt(_4963);
                                        if (dot(_4980, _4950) <= 0.0)
                                        {
                                            _5005 = float2(0.0);
                                            _5006 = false;
                                            break;
                                        }
                                        float3 _4991 = fast::normalize(cross(_4950, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_4950.y) < 0.949999988079071044921875))));
                                        float2 _4999 = float2(dot(_4980, _4991), dot(_4980, cross(_4950, _4991))) * precise::max(Settings.projection.x, Settings.projection.y);
                                        bool2 _5000 = isnan(_4999);
                                        bool2 _5001 = isinf(_4999);
                                        _5005 = _4999;
                                        _5006 = all(not(bool2(_5000.x || _5001.x, _5000.y || _5001.y)));
                                        break;
                                    } while(false);
                                    _2070 = _4953;
                                    _2068 = _5005;
                                    _5008 = !_5006;
                                }
                                else
                                {
                                    _2070 = _2069;
                                    _2068 = _2067;
                                    _5008 = true;
                                }
                                if (_5008)
                                {
                                    _5783 = _2062;
                                    _5784 = _785;
                                    _5785 = _2065;
                                    _5786 = _2068;
                                    _5787 = _2070;
                                    _5788 = _2072;
                                    _5789 = _2074;
                                    _5790 = float4(0.0);
                                    _5791 = true;
                                    break;
                                }
                                float2 _5013 = (_4306 - _2772) / float2(_3607);
                                float2 _5016 = (_2068 - _2772) / float2(_3611);
                                float _5017 = _5013.x;
                                float _5018 = _5016.y;
                                float _5020 = _5016.x;
                                float _5021 = _5013.y;
                                float _5023 = (_5017 * _5018) - (_5020 * _5021);
                                bool _5032;
                                if (!(isnan(_5023) || isinf(_5023)))
                                {
                                    _5032 = abs(_5023) < 9.9999999392252902907785028219223e-09;
                                }
                                else
                                {
                                    _5032 = true;
                                }
                                if (_5032)
                                {
                                    _5783 = _2062;
                                    _5784 = _785;
                                    _5785 = _2065;
                                    _5786 = _2068;
                                    _5787 = _2070;
                                    _5788 = _2072;
                                    _5789 = _2074;
                                    _5790 = float4(0.0);
                                    _5791 = true;
                                    break;
                                }
                                float2 _5044 = float2((_5018 * _2772.x) - (_5020 * _2772.y), ((-_5021) * _2772.x) + (_5017 * _2772.y)) / float2(_5023);
                                bool2 _5045 = isnan(_5044);
                                bool2 _5046 = isinf(_5044);
                                if (!all(not(bool2(_5045.x || _5046.x, _5045.y || _5046.y))))
                                {
                                    _5783 = _2062;
                                    _5784 = _785;
                                    _5785 = _2065;
                                    _5786 = _2068;
                                    _5787 = _2070;
                                    _5788 = _2072;
                                    _5789 = _2074;
                                    _5790 = float4(0.0);
                                    _5791 = true;
                                    break;
                                }
                                float2 _5063;
                                _5063 = _5044 * precise::min(1.0, _2060 / precise::max(precise::max(abs(_5044.x), abs(_5044.y)), 9.9999999600419720025001879548654e-13));
                                float2 _5064;
                                bool _5067;
                                bool _5779;
                                bool _5066 = _2065;
                                uint _5068 = 0u;
                                for (;;)
                                {
                                    if (_5068 < 6u)
                                    {
                                        float2 _5073 = _2062 - _5063;
                                        float2 _5766;
                                        bool _5767;
                                        do
                                        {
                                            spvUnsafeArray<ReflectionSpecularPlane, 4> _197 = _206;
                                            float3 _5712;
                                            float _5713;
                                            float3 _5714;
                                            do
                                            {
                                                bool _5311;
                                                if (_1525)
                                                {
                                                    bool _5309;
                                                    do
                                                    {
                                                        bool _5086;
                                                        if (_1998)
                                                        {
                                                            _5086 = _1153.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _5086 = false;
                                                        }
                                                        bool _5091;
                                                        if (_5086)
                                                        {
                                                            _5091 = _1154.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _5091 = false;
                                                        }
                                                        bool _5107;
                                                        if (!_5091)
                                                        {
                                                            bool _5100;
                                                            if ((isunordered(_1152.w, 1.0) || _1152.w == 1.0))
                                                            {
                                                                _5100 = _1153.w != 1.0;
                                                            }
                                                            else
                                                            {
                                                                _5100 = true;
                                                            }
                                                            bool _5106;
                                                            if (!_5100)
                                                            {
                                                                _5106 = _1154.w != 1.0;
                                                            }
                                                            else
                                                            {
                                                                _5106 = true;
                                                            }
                                                            _5107 = _5106;
                                                        }
                                                        else
                                                        {
                                                            _5107 = false;
                                                        }
                                                        bool _5117;
                                                        if (!_5107)
                                                        {
                                                            bool4 _5111 = isnan(_1152);
                                                            bool4 _5112 = isinf(_1152);
                                                            _5117 = !all(not(bool4(_5111.x || _5112.x, _5111.y || _5112.y, _5111.z || _5112.z, _5111.w || _5112.w)));
                                                        }
                                                        else
                                                        {
                                                            _5117 = true;
                                                        }
                                                        bool _5127;
                                                        if (!_5117)
                                                        {
                                                            bool4 _5121 = isnan(_1153);
                                                            bool4 _5122 = isinf(_1153);
                                                            _5127 = !all(not(bool4(_5121.x || _5122.x, _5121.y || _5122.y, _5121.z || _5122.z, _5121.w || _5122.w)));
                                                        }
                                                        else
                                                        {
                                                            _5127 = true;
                                                        }
                                                        bool _5137;
                                                        if (!_5127)
                                                        {
                                                            bool4 _5131 = isnan(_1154);
                                                            bool4 _5132 = isinf(_1154);
                                                            _5137 = !all(not(bool4(_5131.x || _5132.x, _5131.y || _5132.y, _5131.z || _5132.z, _5131.w || _5132.w)));
                                                        }
                                                        else
                                                        {
                                                            _5137 = true;
                                                        }
                                                        bool _5147;
                                                        if (!_5137)
                                                        {
                                                            bool4 _5141 = isnan(Settings.projection);
                                                            bool4 _5142 = isinf(Settings.projection);
                                                            _5147 = !all(not(bool4(_5141.x || _5142.x, _5141.y || _5142.y, _5141.z || _5142.z, _5141.w || _5142.w)));
                                                        }
                                                        else
                                                        {
                                                            _5147 = true;
                                                        }
                                                        bool _5157;
                                                        if (!_5147)
                                                        {
                                                            bool4 _5151 = isnan(_1275);
                                                            bool4 _5152 = isinf(_1275);
                                                            _5157 = !all(not(bool4(_5151.x || _5152.x, _5151.y || _5152.y, _5151.z || _5152.z, _5151.w || _5152.w)));
                                                        }
                                                        else
                                                        {
                                                            _5157 = true;
                                                        }
                                                        bool _5167;
                                                        if (!_5157)
                                                        {
                                                            bool4 _5161 = isnan(_1283);
                                                            bool4 _5162 = isinf(_1283);
                                                            _5167 = !all(not(bool4(_5161.x || _5162.x, _5161.y || _5162.y, _5161.z || _5162.z, _5161.w || _5162.w)));
                                                        }
                                                        else
                                                        {
                                                            _5167 = true;
                                                        }
                                                        bool _5175;
                                                        if (!_5167)
                                                        {
                                                            _5175 = any(abs(_1152.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _5175 = true;
                                                        }
                                                        bool _5183;
                                                        if (!_5175)
                                                        {
                                                            _5183 = any(abs(_1153.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _5183 = true;
                                                        }
                                                        bool _5191;
                                                        if (!_5183)
                                                        {
                                                            _5191 = any(abs(_1154.xyz) > float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _5191 = true;
                                                        }
                                                        bool _5198;
                                                        if (!_5191)
                                                        {
                                                            _5198 = any(Settings.projection.xy <= float2(0.0));
                                                        }
                                                        else
                                                        {
                                                            _5198 = true;
                                                        }
                                                        bool _5205;
                                                        if (!_5198)
                                                        {
                                                            _5205 = any(abs(Settings.projection) > float4(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _5205 = true;
                                                        }
                                                        bool _5211;
                                                        if (!_5205)
                                                        {
                                                            _5211 = any(_2033 < float2(1.0));
                                                        }
                                                        else
                                                        {
                                                            _5211 = true;
                                                        }
                                                        bool _5217;
                                                        if (!_5211)
                                                        {
                                                            _5217 = any(_2033 > float2(16384.0));
                                                        }
                                                        else
                                                        {
                                                            _5217 = true;
                                                        }
                                                        bool _5222;
                                                        if (!_5217)
                                                        {
                                                            _5222 = Settings.clip.x <= 0.0;
                                                        }
                                                        else
                                                        {
                                                            _5222 = true;
                                                        }
                                                        bool _5227;
                                                        if (!_5222)
                                                        {
                                                            _5227 = Settings.clip.y <= Settings.clip.x;
                                                        }
                                                        else
                                                        {
                                                            _5227 = true;
                                                        }
                                                        bool _5232;
                                                        if (!_5227)
                                                        {
                                                            _5232 = Settings.clip.y > 999999995904.0;
                                                        }
                                                        else
                                                        {
                                                            _5232 = true;
                                                        }
                                                        bool _5238;
                                                        if (!_5232)
                                                        {
                                                            _5238 = _1283.x < 0.0;
                                                        }
                                                        else
                                                        {
                                                            _5238 = true;
                                                        }
                                                        bool _5244;
                                                        if (!_5238)
                                                        {
                                                            _5244 = _1283.x > 1.0;
                                                        }
                                                        else
                                                        {
                                                            _5244 = true;
                                                        }
                                                        bool _5250;
                                                        if (!_5244)
                                                        {
                                                            _5250 = _1283.y < 0.0;
                                                        }
                                                        else
                                                        {
                                                            _5250 = true;
                                                        }
                                                        bool _5256;
                                                        if (!_5250)
                                                        {
                                                            _5256 = _1283.y > 7.0;
                                                        }
                                                        else
                                                        {
                                                            _5256 = true;
                                                        }
                                                        bool _5263;
                                                        if (!_5256)
                                                        {
                                                            _5263 = floor(_1283.y) != _1283.y;
                                                        }
                                                        else
                                                        {
                                                            _5263 = true;
                                                        }
                                                        bool _5270;
                                                        if (!_5263)
                                                        {
                                                            _5270 = any(_1283.zw != float2(0.0));
                                                        }
                                                        else
                                                        {
                                                            _5270 = true;
                                                        }
                                                        if (_5270)
                                                        {
                                                            _5309 = false;
                                                            break;
                                                        }
                                                        bool _5278;
                                                        if (_5091)
                                                        {
                                                            _5278 = any(_1154.xyz != float3(0.0));
                                                        }
                                                        else
                                                        {
                                                            _5278 = false;
                                                        }
                                                        if (_5278)
                                                        {
                                                            _5309 = false;
                                                            break;
                                                        }
                                                        float3 _5291;
                                                        if (_5091)
                                                        {
                                                            _5291 = _1153.xyz;
                                                        }
                                                        else
                                                        {
                                                            _5291 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                        }
                                                        float _5292 = dot(_5291, _5291);
                                                        bool3 _5293 = isnan(_5291);
                                                        bool3 _5294 = isinf(_5291);
                                                        bool _5304;
                                                        if (all(not(bool3(_5293.x || _5294.x, _5293.y || _5294.y, _5293.z || _5294.z))))
                                                        {
                                                            _5304 = !(isnan(_5292) || isinf(_5292));
                                                        }
                                                        else
                                                        {
                                                            _5304 = false;
                                                        }
                                                        bool _5308;
                                                        if (_5304)
                                                        {
                                                            _5308 = _5292 > 9.9999996826552253889678874634872e-21;
                                                        }
                                                        else
                                                        {
                                                            _5308 = false;
                                                        }
                                                        _5309 = _5308;
                                                        break;
                                                    } while(false);
                                                    _5311 = !_5309;
                                                }
                                                else
                                                {
                                                    _5311 = true;
                                                }
                                                float3 _5469;
                                                float _5470;
                                                float3 _5471;
                                                bool _5472;
                                                if (!_5311)
                                                {
                                                    float3 _5464;
                                                    float _5465;
                                                    float3 _5466;
                                                    bool _5467;
                                                    do
                                                    {
                                                        bool2 _5317 = isnan(_5073);
                                                        bool2 _5318 = isinf(_5073);
                                                        bool _5326;
                                                        if (all(not(bool2(_5317.x || _5318.x, _5317.y || _5318.y))))
                                                        {
                                                            _5326 = any(_5073 < float2(0.0));
                                                        }
                                                        else
                                                        {
                                                            _5326 = true;
                                                        }
                                                        bool _5332;
                                                        if (!_5326)
                                                        {
                                                            _5332 = any(_5073 >= _2033);
                                                        }
                                                        else
                                                        {
                                                            _5332 = true;
                                                        }
                                                        if (_5332)
                                                        {
                                                            _5464 = float3(0.0);
                                                            _5465 = 0.0;
                                                            _5466 = float3(0.0);
                                                            _5467 = false;
                                                            break;
                                                        }
                                                        bool _5339;
                                                        if (_1998)
                                                        {
                                                            _5339 = _1153.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _5339 = false;
                                                        }
                                                        bool _5344;
                                                        if (_5339)
                                                        {
                                                            _5344 = _1154.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _5344 = false;
                                                        }
                                                        float3 _5355;
                                                        if (_5344)
                                                        {
                                                            _5355 = _1153.xyz;
                                                        }
                                                        else
                                                        {
                                                            _5355 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                                        }
                                                        float3 _5356 = fast::normalize(_5355);
                                                        float3 _5364 = fast::normalize(float3((_5073 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                                        float3 _5370;
                                                        if (dot(_5356, _5364) > 0.0)
                                                        {
                                                            _5370 = -_5356;
                                                        }
                                                        else
                                                        {
                                                            _5370 = _5356;
                                                        }
                                                        float _5371 = dot(_5364, _5370);
                                                        bool _5380;
                                                        if (!(isnan(_5371) || isinf(_5371)))
                                                        {
                                                            _5380 = abs(_5371) <= 9.9999999600419720025001879548654e-13;
                                                        }
                                                        else
                                                        {
                                                            _5380 = true;
                                                        }
                                                        if (_5380)
                                                        {
                                                            _5464 = float3(0.0);
                                                            _5465 = 0.0;
                                                            _5466 = float3(0.0);
                                                            _5467 = false;
                                                            break;
                                                        }
                                                        float _5385 = dot(_1152.xyz, _5370) / _5371;
                                                        float _5387 = _5385 * _5364.z;
                                                        bool _5395;
                                                        if (!(isnan(_5385) || isinf(_5385)))
                                                        {
                                                            _5395 = _5385 <= 0.0;
                                                        }
                                                        else
                                                        {
                                                            _5395 = true;
                                                        }
                                                        bool _5400;
                                                        if (!_5395)
                                                        {
                                                            _5400 = _5387 < Settings.clip.x;
                                                        }
                                                        else
                                                        {
                                                            _5400 = true;
                                                        }
                                                        bool _5405;
                                                        if (!_5400)
                                                        {
                                                            _5405 = _5387 > Settings.clip.y;
                                                        }
                                                        else
                                                        {
                                                            _5405 = true;
                                                        }
                                                        if (_5405)
                                                        {
                                                            _5464 = float3(0.0);
                                                            _5465 = 0.0;
                                                            _5466 = float3(0.0);
                                                            _5467 = false;
                                                            break;
                                                        }
                                                        bool _5412;
                                                        if (_1998)
                                                        {
                                                            _5412 = _1153.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _5412 = false;
                                                        }
                                                        bool _5417;
                                                        if (_5412)
                                                        {
                                                            _5417 = _1154.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _5417 = false;
                                                        }
                                                        float _5420 = precise::max(_5417 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _5385 * 9.9999997473787516355514526367188e-06);
                                                        float3 _5423 = (_5364 * _5385) + (_5370 * _5420);
                                                        float3 _5424 = reflect(_5364, _5370);
                                                        uint _5427 = uint(_1283.y);
                                                        float3 _5434 = fast::normalize(cross(_5424, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_5424.y) < 0.949999988079071044921875))));
                                                        float3 _5446 = fast::normalize(_5424 + (((_5434 * _175[_5427].x) + (cross(_5424, _5434) * _175[_5427].y)) * (_1283.x * _1283.x)));
                                                        float3 _5450 = select(_5446, _5424, bool3(dot(_5446, _5370) <= 0.0));
                                                        bool3 _5451 = isnan(_5423);
                                                        bool3 _5452 = isinf(_5423);
                                                        bool _5463;
                                                        if (all(not(bool3(_5451.x || _5452.x, _5451.y || _5452.y, _5451.z || _5452.z))))
                                                        {
                                                            bool3 _5458 = isnan(_5450);
                                                            bool3 _5459 = isinf(_5450);
                                                            _5463 = all(not(bool3(_5458.x || _5459.x, _5458.y || _5459.y, _5458.z || _5459.z)));
                                                        }
                                                        else
                                                        {
                                                            _5463 = false;
                                                        }
                                                        _5464 = _5450;
                                                        _5465 = _5420;
                                                        _5466 = _5423;
                                                        _5467 = _5463;
                                                        break;
                                                    } while(false);
                                                    _5469 = _5464;
                                                    _5470 = _5465;
                                                    _5471 = _5466;
                                                    _5472 = !_5467;
                                                }
                                                else
                                                {
                                                    _5469 = float3(0.0);
                                                    _5470 = 0.0;
                                                    _5471 = float3(0.0);
                                                    _5472 = true;
                                                }
                                                if (_5472)
                                                {
                                                    _5712 = _5469;
                                                    _5713 = _5470;
                                                    _5714 = _5471;
                                                    _5067 = false;
                                                    break;
                                                }
                                                float3 _5476;
                                                float3 _5481;
                                                _5476 = _5469;
                                                _5481 = _5471;
                                                float3 _5477;
                                                float _5480;
                                                float3 _5482;
                                                float3 _5706;
                                                float _5707;
                                                float3 _5708;
                                                bool _5709;
                                                bool _5710;
                                                float _5479 = _5470;
                                                uint _5483 = 0u;
                                                for (;;)
                                                {
                                                    if (_5483 < _929)
                                                    {
                                                        bool _5496;
                                                        bool _5615;
                                                        do
                                                        {
                                                            _5496 = _197[_5483].a.w == 2.0;
                                                            bool _5501;
                                                            if (_5496)
                                                            {
                                                                _5501 = _197[_5483].b.w == 2.0;
                                                            }
                                                            else
                                                            {
                                                                _5501 = false;
                                                            }
                                                            bool _5506;
                                                            if (_5501)
                                                            {
                                                                _5506 = _197[_5483].c.w == 2.0;
                                                            }
                                                            else
                                                            {
                                                                _5506 = false;
                                                            }
                                                            bool _5522;
                                                            if (!_5506)
                                                            {
                                                                bool _5515;
                                                                if ((isunordered(_197[_5483].a.w, 1.0) || _197[_5483].a.w == 1.0))
                                                                {
                                                                    _5515 = _197[_5483].b.w != 1.0;
                                                                }
                                                                else
                                                                {
                                                                    _5515 = true;
                                                                }
                                                                bool _5521;
                                                                if (!_5515)
                                                                {
                                                                    _5521 = _197[_5483].c.w != 1.0;
                                                                }
                                                                else
                                                                {
                                                                    _5521 = true;
                                                                }
                                                                _5522 = _5521;
                                                            }
                                                            else
                                                            {
                                                                _5522 = false;
                                                            }
                                                            bool _5532;
                                                            if (!_5522)
                                                            {
                                                                bool4 _5526 = isnan(_197[_5483].a);
                                                                bool4 _5527 = isinf(_197[_5483].a);
                                                                _5532 = !all(not(bool4(_5526.x || _5527.x, _5526.y || _5527.y, _5526.z || _5527.z, _5526.w || _5527.w)));
                                                            }
                                                            else
                                                            {
                                                                _5532 = true;
                                                            }
                                                            bool _5542;
                                                            if (!_5532)
                                                            {
                                                                bool4 _5536 = isnan(_197[_5483].b);
                                                                bool4 _5537 = isinf(_197[_5483].b);
                                                                _5542 = !all(not(bool4(_5536.x || _5537.x, _5536.y || _5537.y, _5536.z || _5537.z, _5536.w || _5537.w)));
                                                            }
                                                            else
                                                            {
                                                                _5542 = true;
                                                            }
                                                            bool _5552;
                                                            if (!_5542)
                                                            {
                                                                bool4 _5546 = isnan(_197[_5483].c);
                                                                bool4 _5547 = isinf(_197[_5483].c);
                                                                _5552 = !all(not(bool4(_5546.x || _5547.x, _5546.y || _5547.y, _5546.z || _5547.z, _5546.w || _5547.w)));
                                                            }
                                                            else
                                                            {
                                                                _5552 = true;
                                                            }
                                                            bool _5560;
                                                            if (!_5552)
                                                            {
                                                                _5560 = any(abs(_197[_5483].a.xyz) > float3(999999995904.0));
                                                            }
                                                            else
                                                            {
                                                                _5560 = true;
                                                            }
                                                            bool _5568;
                                                            if (!_5560)
                                                            {
                                                                _5568 = any(abs(_197[_5483].b.xyz) > float3(999999995904.0));
                                                            }
                                                            else
                                                            {
                                                                _5568 = true;
                                                            }
                                                            bool _5576;
                                                            if (!_5568)
                                                            {
                                                                _5576 = any(abs(_197[_5483].c.xyz) > float3(999999995904.0));
                                                            }
                                                            else
                                                            {
                                                                _5576 = true;
                                                            }
                                                            if (_5576)
                                                            {
                                                                _5615 = false;
                                                                break;
                                                            }
                                                            bool _5584;
                                                            if (_5506)
                                                            {
                                                                _5584 = any(_197[_5483].c.xyz != float3(0.0));
                                                            }
                                                            else
                                                            {
                                                                _5584 = false;
                                                            }
                                                            if (_5584)
                                                            {
                                                                _5615 = false;
                                                                break;
                                                            }
                                                            float3 _5597;
                                                            if (_5506)
                                                            {
                                                                _5597 = _197[_5483].b.xyz;
                                                            }
                                                            else
                                                            {
                                                                _5597 = cross(_197[_5483].b.xyz - _197[_5483].a.xyz, _197[_5483].c.xyz - _197[_5483].a.xyz);
                                                            }
                                                            float _5598 = dot(_5597, _5597);
                                                            bool3 _5599 = isnan(_5597);
                                                            bool3 _5600 = isinf(_5597);
                                                            bool _5610;
                                                            if (all(not(bool3(_5599.x || _5600.x, _5599.y || _5600.y, _5599.z || _5600.z))))
                                                            {
                                                                _5610 = !(isnan(_5598) || isinf(_5598));
                                                            }
                                                            else
                                                            {
                                                                _5610 = false;
                                                            }
                                                            bool _5614;
                                                            if (_5610)
                                                            {
                                                                _5614 = _5598 > 9.9999996826552253889678874634872e-21;
                                                            }
                                                            else
                                                            {
                                                                _5614 = false;
                                                            }
                                                            _5615 = _5614;
                                                            break;
                                                        } while(false);
                                                        if (!_5615)
                                                        {
                                                            _5706 = _5476;
                                                            _5707 = _5479;
                                                            _5708 = _5481;
                                                            _5709 = false;
                                                            _5710 = true;
                                                            break;
                                                        }
                                                        bool _5623;
                                                        if (_5496)
                                                        {
                                                            _5623 = _197[_5483].b.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _5623 = false;
                                                        }
                                                        bool _5628;
                                                        if (_5623)
                                                        {
                                                            _5628 = _197[_5483].c.w == 2.0;
                                                        }
                                                        else
                                                        {
                                                            _5628 = false;
                                                        }
                                                        float3 _5639;
                                                        if (_5628)
                                                        {
                                                            _5639 = _197[_5483].b.xyz;
                                                        }
                                                        else
                                                        {
                                                            _5639 = cross(_197[_5483].b.xyz - _197[_5483].a.xyz, _197[_5483].c.xyz - _197[_5483].a.xyz);
                                                        }
                                                        float3 _5640 = fast::normalize(_5639);
                                                        float3 _5646;
                                                        if (dot(_5640, _5476) > 0.0)
                                                        {
                                                            _5646 = -_5640;
                                                        }
                                                        else
                                                        {
                                                            _5646 = _5640;
                                                        }
                                                        float _5647 = dot(_5476, _5646);
                                                        bool _5656;
                                                        if (!(isnan(_5647) || isinf(_5647)))
                                                        {
                                                            _5656 = abs(_5647) <= 9.9999999600419720025001879548654e-13;
                                                        }
                                                        else
                                                        {
                                                            _5656 = true;
                                                        }
                                                        if (_5656)
                                                        {
                                                            _5706 = _5476;
                                                            _5707 = _5479;
                                                            _5708 = _5481;
                                                            _5709 = false;
                                                            _5710 = true;
                                                            break;
                                                        }
                                                        float _5662 = dot(_197[_5483].a.xyz - _5481, _5646) / _5647;
                                                        bool _5670;
                                                        if (!(isnan(_5662) || isinf(_5662)))
                                                        {
                                                            _5670 = _5662 <= _5479;
                                                        }
                                                        else
                                                        {
                                                            _5670 = true;
                                                        }
                                                        bool _5675;
                                                        if (!_5670)
                                                        {
                                                            _5675 = _5662 >= 65536.0;
                                                        }
                                                        else
                                                        {
                                                            _5675 = true;
                                                        }
                                                        if (_5675)
                                                        {
                                                            _5706 = _5476;
                                                            _5707 = _5479;
                                                            _5708 = _5481;
                                                            _5709 = false;
                                                            _5710 = true;
                                                            break;
                                                        }
                                                        float3 _5679 = _5481 + (_5476 * _5662);
                                                        bool3 _5680 = isnan(_5679);
                                                        bool3 _5681 = isinf(_5679);
                                                        if (!all(not(bool3(_5680.x || _5681.x, _5680.y || _5681.y, _5680.z || _5681.z))))
                                                        {
                                                            _5706 = _5476;
                                                            _5707 = _5479;
                                                            _5708 = _5481;
                                                            _5709 = false;
                                                            _5710 = true;
                                                            break;
                                                        }
                                                        _5480 = precise::max(0.0500000007450580596923828125, _5662 * 9.9999997473787516355514526367188e-06);
                                                        _5482 = _5679 + (_5646 * _5480);
                                                        _5477 = reflect(_5476, _5646);
                                                        bool3 _5690 = isnan(_5482);
                                                        bool3 _5691 = isinf(_5482);
                                                        bool _5703;
                                                        if (all(not(bool3(_5690.x || _5691.x, _5690.y || _5691.y, _5690.z || _5691.z))))
                                                        {
                                                            bool3 _5697 = isnan(_5477);
                                                            bool3 _5698 = isinf(_5477);
                                                            _5703 = !all(not(bool3(_5697.x || _5698.x, _5697.y || _5698.y, _5697.z || _5698.z)));
                                                        }
                                                        else
                                                        {
                                                            _5703 = true;
                                                        }
                                                        if (_5703)
                                                        {
                                                            _5706 = _5477;
                                                            _5707 = _5480;
                                                            _5708 = _5482;
                                                            _5709 = false;
                                                            _5710 = true;
                                                            break;
                                                        }
                                                        _5476 = _5477;
                                                        _5479 = _5480;
                                                        _5481 = _5482;
                                                        _5483++;
                                                        continue;
                                                    }
                                                    else
                                                    {
                                                        _5706 = _5476;
                                                        _5707 = _5479;
                                                        _5708 = _5481;
                                                        _5709 = _5066;
                                                        _5710 = false;
                                                        break;
                                                    }
                                                }
                                                if (_5710)
                                                {
                                                    _5712 = _5706;
                                                    _5713 = _5707;
                                                    _5714 = _5708;
                                                    _5067 = _5709;
                                                    break;
                                                }
                                                _5712 = _5706;
                                                _5713 = _5707;
                                                _5714 = _5708;
                                                _5067 = true;
                                                break;
                                            } while(false);
                                            if (!_5067)
                                            {
                                                _5766 = float2(0.0);
                                                _5767 = false;
                                                break;
                                            }
                                            bool _5718 = _1521.w == 0.0;
                                            float3 _5723;
                                            if (_5718)
                                            {
                                                _5723 = _1521.xyz;
                                            }
                                            else
                                            {
                                                _5723 = _1521.xyz - _5714;
                                            }
                                            float _5724 = dot(_5723, _5723);
                                            bool _5737;
                                            if (!(isnan(_5724) || isinf(_5724)))
                                            {
                                                float _5735;
                                                if (_5718)
                                                {
                                                    _5735 = 9.9999996826552253889678874634872e-21;
                                                }
                                                else
                                                {
                                                    _5735 = _5713 * _5713;
                                                }
                                                _5737 = _5724 <= _5735;
                                            }
                                            else
                                            {
                                                _5737 = true;
                                            }
                                            if (_5737)
                                            {
                                                _5766 = float2(0.0);
                                                _5767 = false;
                                                break;
                                            }
                                            float3 _5741 = _5723 * rsqrt(_5724);
                                            if (dot(_5741, _5712) <= 0.0)
                                            {
                                                _5766 = float2(0.0);
                                                _5767 = false;
                                                break;
                                            }
                                            float3 _5752 = fast::normalize(cross(_5712, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_5712.y) < 0.949999988079071044921875))));
                                            float2 _5760 = float2(dot(_5741, _5752), dot(_5741, cross(_5712, _5752))) * precise::max(Settings.projection.x, Settings.projection.y);
                                            bool2 _5761 = isnan(_5760);
                                            bool2 _5762 = isinf(_5760);
                                            _5766 = _5760;
                                            _5767 = all(not(bool2(_5761.x || _5762.x, _5761.y || _5762.y)));
                                            break;
                                        } while(false);
                                        bool _5776;
                                        if (_5767)
                                        {
                                            _5776 = precise::max(abs(_5766.x), abs(_5766.y)) < _2781;
                                        }
                                        else
                                        {
                                            _5776 = false;
                                        }
                                        if (_5776)
                                        {
                                            _2063 = _5073;
                                            _2066 = _5067;
                                            _5779 = true;
                                            break;
                                        }
                                        _5064 = _5063 * 0.5;
                                        _5063 = _5064;
                                        _5066 = _5067;
                                        _5068++;
                                        continue;
                                    }
                                    else
                                    {
                                        _2063 = _2062;
                                        _2066 = _5066;
                                        _5779 = false;
                                        break;
                                    }
                                }
                                if (!_5779)
                                {
                                    _5783 = _2063;
                                    _5784 = _785;
                                    _5785 = _2066;
                                    _5786 = _2068;
                                    _5787 = _2070;
                                    _5788 = _2072;
                                    _5789 = _2074;
                                    _5790 = float4(0.0);
                                    _5791 = true;
                                    break;
                                }
                                _2062 = _2063;
                                _2065 = _2066;
                                _2067 = _2068;
                                _2069 = _2070;
                                _2071 = _2072;
                                _2073 = _2074;
                                _2075++;
                                continue;
                            }
                            else
                            {
                                _5783 = _2062;
                                _5784 = _785;
                                _5785 = _2065;
                                _5786 = _2067;
                                _5787 = _2069;
                                _5788 = _2071;
                                _5789 = _2073;
                                _5790 = _1944;
                                _5791 = _1945;
                                break;
                            }
                        }
                        if (_5791)
                        {
                            _7318 = _781;
                            _7319 = _783;
                            _7320 = _5784;
                            _7321 = _5785;
                            _7322 = _5786;
                            _7323 = _5787;
                            _7324 = _5788;
                            _7325 = _5789;
                            _7326 = _5790;
                            break;
                        }
                        bool _6434;
                        float2 _6486;
                        bool _6487;
                        do
                        {
                            spvUnsafeArray<ReflectionSpecularPlane, 4> _195 = _206;
                            float3 _6431;
                            float _6432;
                            float3 _6433;
                            do
                            {
                                bool _6030;
                                if (_1525)
                                {
                                    bool _6028;
                                    do
                                    {
                                        bool _5805;
                                        if (_1998)
                                        {
                                            _5805 = _1153.w == 2.0;
                                        }
                                        else
                                        {
                                            _5805 = false;
                                        }
                                        bool _5810;
                                        if (_5805)
                                        {
                                            _5810 = _1154.w == 2.0;
                                        }
                                        else
                                        {
                                            _5810 = false;
                                        }
                                        bool _5826;
                                        if (!_5810)
                                        {
                                            bool _5819;
                                            if ((isunordered(_1152.w, 1.0) || _1152.w == 1.0))
                                            {
                                                _5819 = _1153.w != 1.0;
                                            }
                                            else
                                            {
                                                _5819 = true;
                                            }
                                            bool _5825;
                                            if (!_5819)
                                            {
                                                _5825 = _1154.w != 1.0;
                                            }
                                            else
                                            {
                                                _5825 = true;
                                            }
                                            _5826 = _5825;
                                        }
                                        else
                                        {
                                            _5826 = false;
                                        }
                                        bool _5836;
                                        if (!_5826)
                                        {
                                            bool4 _5830 = isnan(_1152);
                                            bool4 _5831 = isinf(_1152);
                                            _5836 = !all(not(bool4(_5830.x || _5831.x, _5830.y || _5831.y, _5830.z || _5831.z, _5830.w || _5831.w)));
                                        }
                                        else
                                        {
                                            _5836 = true;
                                        }
                                        bool _5846;
                                        if (!_5836)
                                        {
                                            bool4 _5840 = isnan(_1153);
                                            bool4 _5841 = isinf(_1153);
                                            _5846 = !all(not(bool4(_5840.x || _5841.x, _5840.y || _5841.y, _5840.z || _5841.z, _5840.w || _5841.w)));
                                        }
                                        else
                                        {
                                            _5846 = true;
                                        }
                                        bool _5856;
                                        if (!_5846)
                                        {
                                            bool4 _5850 = isnan(_1154);
                                            bool4 _5851 = isinf(_1154);
                                            _5856 = !all(not(bool4(_5850.x || _5851.x, _5850.y || _5851.y, _5850.z || _5851.z, _5850.w || _5851.w)));
                                        }
                                        else
                                        {
                                            _5856 = true;
                                        }
                                        bool _5866;
                                        if (!_5856)
                                        {
                                            bool4 _5860 = isnan(Settings.projection);
                                            bool4 _5861 = isinf(Settings.projection);
                                            _5866 = !all(not(bool4(_5860.x || _5861.x, _5860.y || _5861.y, _5860.z || _5861.z, _5860.w || _5861.w)));
                                        }
                                        else
                                        {
                                            _5866 = true;
                                        }
                                        bool _5876;
                                        if (!_5866)
                                        {
                                            bool4 _5870 = isnan(_1275);
                                            bool4 _5871 = isinf(_1275);
                                            _5876 = !all(not(bool4(_5870.x || _5871.x, _5870.y || _5871.y, _5870.z || _5871.z, _5870.w || _5871.w)));
                                        }
                                        else
                                        {
                                            _5876 = true;
                                        }
                                        bool _5886;
                                        if (!_5876)
                                        {
                                            bool4 _5880 = isnan(_1283);
                                            bool4 _5881 = isinf(_1283);
                                            _5886 = !all(not(bool4(_5880.x || _5881.x, _5880.y || _5881.y, _5880.z || _5881.z, _5880.w || _5881.w)));
                                        }
                                        else
                                        {
                                            _5886 = true;
                                        }
                                        bool _5894;
                                        if (!_5886)
                                        {
                                            _5894 = any(abs(_1152.xyz) > float3(999999995904.0));
                                        }
                                        else
                                        {
                                            _5894 = true;
                                        }
                                        bool _5902;
                                        if (!_5894)
                                        {
                                            _5902 = any(abs(_1153.xyz) > float3(999999995904.0));
                                        }
                                        else
                                        {
                                            _5902 = true;
                                        }
                                        bool _5910;
                                        if (!_5902)
                                        {
                                            _5910 = any(abs(_1154.xyz) > float3(999999995904.0));
                                        }
                                        else
                                        {
                                            _5910 = true;
                                        }
                                        bool _5917;
                                        if (!_5910)
                                        {
                                            _5917 = any(Settings.projection.xy <= float2(0.0));
                                        }
                                        else
                                        {
                                            _5917 = true;
                                        }
                                        bool _5924;
                                        if (!_5917)
                                        {
                                            _5924 = any(abs(Settings.projection) > float4(999999995904.0));
                                        }
                                        else
                                        {
                                            _5924 = true;
                                        }
                                        bool _5930;
                                        if (!_5924)
                                        {
                                            _5930 = any(_2033 < float2(1.0));
                                        }
                                        else
                                        {
                                            _5930 = true;
                                        }
                                        bool _5936;
                                        if (!_5930)
                                        {
                                            _5936 = any(_2033 > float2(16384.0));
                                        }
                                        else
                                        {
                                            _5936 = true;
                                        }
                                        bool _5941;
                                        if (!_5936)
                                        {
                                            _5941 = Settings.clip.x <= 0.0;
                                        }
                                        else
                                        {
                                            _5941 = true;
                                        }
                                        bool _5946;
                                        if (!_5941)
                                        {
                                            _5946 = Settings.clip.y <= Settings.clip.x;
                                        }
                                        else
                                        {
                                            _5946 = true;
                                        }
                                        bool _5951;
                                        if (!_5946)
                                        {
                                            _5951 = Settings.clip.y > 999999995904.0;
                                        }
                                        else
                                        {
                                            _5951 = true;
                                        }
                                        bool _5957;
                                        if (!_5951)
                                        {
                                            _5957 = _1283.x < 0.0;
                                        }
                                        else
                                        {
                                            _5957 = true;
                                        }
                                        bool _5963;
                                        if (!_5957)
                                        {
                                            _5963 = _1283.x > 1.0;
                                        }
                                        else
                                        {
                                            _5963 = true;
                                        }
                                        bool _5969;
                                        if (!_5963)
                                        {
                                            _5969 = _1283.y < 0.0;
                                        }
                                        else
                                        {
                                            _5969 = true;
                                        }
                                        bool _5975;
                                        if (!_5969)
                                        {
                                            _5975 = _1283.y > 7.0;
                                        }
                                        else
                                        {
                                            _5975 = true;
                                        }
                                        bool _5982;
                                        if (!_5975)
                                        {
                                            _5982 = floor(_1283.y) != _1283.y;
                                        }
                                        else
                                        {
                                            _5982 = true;
                                        }
                                        bool _5989;
                                        if (!_5982)
                                        {
                                            _5989 = any(_1283.zw != float2(0.0));
                                        }
                                        else
                                        {
                                            _5989 = true;
                                        }
                                        if (_5989)
                                        {
                                            _6028 = false;
                                            break;
                                        }
                                        bool _5997;
                                        if (_5810)
                                        {
                                            _5997 = any(_1154.xyz != float3(0.0));
                                        }
                                        else
                                        {
                                            _5997 = false;
                                        }
                                        if (_5997)
                                        {
                                            _6028 = false;
                                            break;
                                        }
                                        float3 _6010;
                                        if (_5810)
                                        {
                                            _6010 = _1153.xyz;
                                        }
                                        else
                                        {
                                            _6010 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                        }
                                        float _6011 = dot(_6010, _6010);
                                        bool3 _6012 = isnan(_6010);
                                        bool3 _6013 = isinf(_6010);
                                        bool _6023;
                                        if (all(not(bool3(_6012.x || _6013.x, _6012.y || _6013.y, _6012.z || _6013.z))))
                                        {
                                            _6023 = !(isnan(_6011) || isinf(_6011));
                                        }
                                        else
                                        {
                                            _6023 = false;
                                        }
                                        bool _6027;
                                        if (_6023)
                                        {
                                            _6027 = _6011 > 9.9999996826552253889678874634872e-21;
                                        }
                                        else
                                        {
                                            _6027 = false;
                                        }
                                        _6028 = _6027;
                                        break;
                                    } while(false);
                                    _6030 = !_6028;
                                }
                                else
                                {
                                    _6030 = true;
                                }
                                float3 _6188;
                                float _6189;
                                float3 _6190;
                                bool _6191;
                                if (!_6030)
                                {
                                    float3 _6183;
                                    float _6184;
                                    float3 _6185;
                                    bool _6186;
                                    do
                                    {
                                        bool2 _6036 = isnan(_5783);
                                        bool2 _6037 = isinf(_5783);
                                        bool _6045;
                                        if (all(not(bool2(_6036.x || _6037.x, _6036.y || _6037.y))))
                                        {
                                            _6045 = any(_5783 < float2(0.0));
                                        }
                                        else
                                        {
                                            _6045 = true;
                                        }
                                        bool _6051;
                                        if (!_6045)
                                        {
                                            _6051 = any(_5783 >= _2033);
                                        }
                                        else
                                        {
                                            _6051 = true;
                                        }
                                        if (_6051)
                                        {
                                            _6183 = float3(0.0);
                                            _6184 = 0.0;
                                            _6185 = float3(0.0);
                                            _6186 = false;
                                            break;
                                        }
                                        bool _6058;
                                        if (_1998)
                                        {
                                            _6058 = _1153.w == 2.0;
                                        }
                                        else
                                        {
                                            _6058 = false;
                                        }
                                        bool _6063;
                                        if (_6058)
                                        {
                                            _6063 = _1154.w == 2.0;
                                        }
                                        else
                                        {
                                            _6063 = false;
                                        }
                                        float3 _6074;
                                        if (_6063)
                                        {
                                            _6074 = _1153.xyz;
                                        }
                                        else
                                        {
                                            _6074 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                        }
                                        float3 _6075 = fast::normalize(_6074);
                                        float3 _6083 = fast::normalize(float3((_5783 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                        float3 _6089;
                                        if (dot(_6075, _6083) > 0.0)
                                        {
                                            _6089 = -_6075;
                                        }
                                        else
                                        {
                                            _6089 = _6075;
                                        }
                                        float _6090 = dot(_6083, _6089);
                                        bool _6099;
                                        if (!(isnan(_6090) || isinf(_6090)))
                                        {
                                            _6099 = abs(_6090) <= 9.9999999600419720025001879548654e-13;
                                        }
                                        else
                                        {
                                            _6099 = true;
                                        }
                                        if (_6099)
                                        {
                                            _6183 = float3(0.0);
                                            _6184 = 0.0;
                                            _6185 = float3(0.0);
                                            _6186 = false;
                                            break;
                                        }
                                        float _6104 = dot(_1152.xyz, _6089) / _6090;
                                        float _6106 = _6104 * _6083.z;
                                        bool _6114;
                                        if (!(isnan(_6104) || isinf(_6104)))
                                        {
                                            _6114 = _6104 <= 0.0;
                                        }
                                        else
                                        {
                                            _6114 = true;
                                        }
                                        bool _6119;
                                        if (!_6114)
                                        {
                                            _6119 = _6106 < Settings.clip.x;
                                        }
                                        else
                                        {
                                            _6119 = true;
                                        }
                                        bool _6124;
                                        if (!_6119)
                                        {
                                            _6124 = _6106 > Settings.clip.y;
                                        }
                                        else
                                        {
                                            _6124 = true;
                                        }
                                        if (_6124)
                                        {
                                            _6183 = float3(0.0);
                                            _6184 = 0.0;
                                            _6185 = float3(0.0);
                                            _6186 = false;
                                            break;
                                        }
                                        bool _6131;
                                        if (_1998)
                                        {
                                            _6131 = _1153.w == 2.0;
                                        }
                                        else
                                        {
                                            _6131 = false;
                                        }
                                        bool _6136;
                                        if (_6131)
                                        {
                                            _6136 = _1154.w == 2.0;
                                        }
                                        else
                                        {
                                            _6136 = false;
                                        }
                                        float _6139 = precise::max(_6136 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _6104 * 9.9999997473787516355514526367188e-06);
                                        float3 _6142 = (_6083 * _6104) + (_6089 * _6139);
                                        float3 _6143 = reflect(_6083, _6089);
                                        uint _6146 = uint(_1283.y);
                                        float3 _6153 = fast::normalize(cross(_6143, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_6143.y) < 0.949999988079071044921875))));
                                        float3 _6165 = fast::normalize(_6143 + (((_6153 * _175[_6146].x) + (cross(_6143, _6153) * _175[_6146].y)) * (_1283.x * _1283.x)));
                                        float3 _6169 = select(_6165, _6143, bool3(dot(_6165, _6089) <= 0.0));
                                        bool3 _6170 = isnan(_6142);
                                        bool3 _6171 = isinf(_6142);
                                        bool _6182;
                                        if (all(not(bool3(_6170.x || _6171.x, _6170.y || _6171.y, _6170.z || _6171.z))))
                                        {
                                            bool3 _6177 = isnan(_6169);
                                            bool3 _6178 = isinf(_6169);
                                            _6182 = all(not(bool3(_6177.x || _6178.x, _6177.y || _6178.y, _6177.z || _6178.z)));
                                        }
                                        else
                                        {
                                            _6182 = false;
                                        }
                                        _6183 = _6169;
                                        _6184 = _6139;
                                        _6185 = _6142;
                                        _6186 = _6182;
                                        break;
                                    } while(false);
                                    _6188 = _6183;
                                    _6189 = _6184;
                                    _6190 = _6185;
                                    _6191 = !_6186;
                                }
                                else
                                {
                                    _6188 = float3(0.0);
                                    _6189 = 0.0;
                                    _6190 = float3(0.0);
                                    _6191 = true;
                                }
                                if (_6191)
                                {
                                    _6431 = _6188;
                                    _6432 = _6189;
                                    _6433 = _6190;
                                    _6434 = false;
                                    break;
                                }
                                float3 _6195;
                                float3 _6200;
                                _6195 = _6188;
                                _6200 = _6190;
                                float3 _6196;
                                float _6199;
                                float3 _6201;
                                float3 _6425;
                                float _6426;
                                float3 _6427;
                                bool _6428;
                                bool _6429;
                                float _6198 = _6189;
                                uint _6202 = 0u;
                                for (;;)
                                {
                                    if (_6202 < _929)
                                    {
                                        bool _6215;
                                        bool _6334;
                                        do
                                        {
                                            _6215 = _195[_6202].a.w == 2.0;
                                            bool _6220;
                                            if (_6215)
                                            {
                                                _6220 = _195[_6202].b.w == 2.0;
                                            }
                                            else
                                            {
                                                _6220 = false;
                                            }
                                            bool _6225;
                                            if (_6220)
                                            {
                                                _6225 = _195[_6202].c.w == 2.0;
                                            }
                                            else
                                            {
                                                _6225 = false;
                                            }
                                            bool _6241;
                                            if (!_6225)
                                            {
                                                bool _6234;
                                                if ((isunordered(_195[_6202].a.w, 1.0) || _195[_6202].a.w == 1.0))
                                                {
                                                    _6234 = _195[_6202].b.w != 1.0;
                                                }
                                                else
                                                {
                                                    _6234 = true;
                                                }
                                                bool _6240;
                                                if (!_6234)
                                                {
                                                    _6240 = _195[_6202].c.w != 1.0;
                                                }
                                                else
                                                {
                                                    _6240 = true;
                                                }
                                                _6241 = _6240;
                                            }
                                            else
                                            {
                                                _6241 = false;
                                            }
                                            bool _6251;
                                            if (!_6241)
                                            {
                                                bool4 _6245 = isnan(_195[_6202].a);
                                                bool4 _6246 = isinf(_195[_6202].a);
                                                _6251 = !all(not(bool4(_6245.x || _6246.x, _6245.y || _6246.y, _6245.z || _6246.z, _6245.w || _6246.w)));
                                            }
                                            else
                                            {
                                                _6251 = true;
                                            }
                                            bool _6261;
                                            if (!_6251)
                                            {
                                                bool4 _6255 = isnan(_195[_6202].b);
                                                bool4 _6256 = isinf(_195[_6202].b);
                                                _6261 = !all(not(bool4(_6255.x || _6256.x, _6255.y || _6256.y, _6255.z || _6256.z, _6255.w || _6256.w)));
                                            }
                                            else
                                            {
                                                _6261 = true;
                                            }
                                            bool _6271;
                                            if (!_6261)
                                            {
                                                bool4 _6265 = isnan(_195[_6202].c);
                                                bool4 _6266 = isinf(_195[_6202].c);
                                                _6271 = !all(not(bool4(_6265.x || _6266.x, _6265.y || _6266.y, _6265.z || _6266.z, _6265.w || _6266.w)));
                                            }
                                            else
                                            {
                                                _6271 = true;
                                            }
                                            bool _6279;
                                            if (!_6271)
                                            {
                                                _6279 = any(abs(_195[_6202].a.xyz) > float3(999999995904.0));
                                            }
                                            else
                                            {
                                                _6279 = true;
                                            }
                                            bool _6287;
                                            if (!_6279)
                                            {
                                                _6287 = any(abs(_195[_6202].b.xyz) > float3(999999995904.0));
                                            }
                                            else
                                            {
                                                _6287 = true;
                                            }
                                            bool _6295;
                                            if (!_6287)
                                            {
                                                _6295 = any(abs(_195[_6202].c.xyz) > float3(999999995904.0));
                                            }
                                            else
                                            {
                                                _6295 = true;
                                            }
                                            if (_6295)
                                            {
                                                _6334 = false;
                                                break;
                                            }
                                            bool _6303;
                                            if (_6225)
                                            {
                                                _6303 = any(_195[_6202].c.xyz != float3(0.0));
                                            }
                                            else
                                            {
                                                _6303 = false;
                                            }
                                            if (_6303)
                                            {
                                                _6334 = false;
                                                break;
                                            }
                                            float3 _6316;
                                            if (_6225)
                                            {
                                                _6316 = _195[_6202].b.xyz;
                                            }
                                            else
                                            {
                                                _6316 = cross(_195[_6202].b.xyz - _195[_6202].a.xyz, _195[_6202].c.xyz - _195[_6202].a.xyz);
                                            }
                                            float _6317 = dot(_6316, _6316);
                                            bool3 _6318 = isnan(_6316);
                                            bool3 _6319 = isinf(_6316);
                                            bool _6329;
                                            if (all(not(bool3(_6318.x || _6319.x, _6318.y || _6319.y, _6318.z || _6319.z))))
                                            {
                                                _6329 = !(isnan(_6317) || isinf(_6317));
                                            }
                                            else
                                            {
                                                _6329 = false;
                                            }
                                            bool _6333;
                                            if (_6329)
                                            {
                                                _6333 = _6317 > 9.9999996826552253889678874634872e-21;
                                            }
                                            else
                                            {
                                                _6333 = false;
                                            }
                                            _6334 = _6333;
                                            break;
                                        } while(false);
                                        if (!_6334)
                                        {
                                            _6425 = _6195;
                                            _6426 = _6198;
                                            _6427 = _6200;
                                            _6428 = false;
                                            _6429 = true;
                                            break;
                                        }
                                        bool _6342;
                                        if (_6215)
                                        {
                                            _6342 = _195[_6202].b.w == 2.0;
                                        }
                                        else
                                        {
                                            _6342 = false;
                                        }
                                        bool _6347;
                                        if (_6342)
                                        {
                                            _6347 = _195[_6202].c.w == 2.0;
                                        }
                                        else
                                        {
                                            _6347 = false;
                                        }
                                        float3 _6358;
                                        if (_6347)
                                        {
                                            _6358 = _195[_6202].b.xyz;
                                        }
                                        else
                                        {
                                            _6358 = cross(_195[_6202].b.xyz - _195[_6202].a.xyz, _195[_6202].c.xyz - _195[_6202].a.xyz);
                                        }
                                        float3 _6359 = fast::normalize(_6358);
                                        float3 _6365;
                                        if (dot(_6359, _6195) > 0.0)
                                        {
                                            _6365 = -_6359;
                                        }
                                        else
                                        {
                                            _6365 = _6359;
                                        }
                                        float _6366 = dot(_6195, _6365);
                                        bool _6375;
                                        if (!(isnan(_6366) || isinf(_6366)))
                                        {
                                            _6375 = abs(_6366) <= 9.9999999600419720025001879548654e-13;
                                        }
                                        else
                                        {
                                            _6375 = true;
                                        }
                                        if (_6375)
                                        {
                                            _6425 = _6195;
                                            _6426 = _6198;
                                            _6427 = _6200;
                                            _6428 = false;
                                            _6429 = true;
                                            break;
                                        }
                                        float _6381 = dot(_195[_6202].a.xyz - _6200, _6365) / _6366;
                                        bool _6389;
                                        if (!(isnan(_6381) || isinf(_6381)))
                                        {
                                            _6389 = _6381 <= _6198;
                                        }
                                        else
                                        {
                                            _6389 = true;
                                        }
                                        bool _6394;
                                        if (!_6389)
                                        {
                                            _6394 = _6381 >= 65536.0;
                                        }
                                        else
                                        {
                                            _6394 = true;
                                        }
                                        if (_6394)
                                        {
                                            _6425 = _6195;
                                            _6426 = _6198;
                                            _6427 = _6200;
                                            _6428 = false;
                                            _6429 = true;
                                            break;
                                        }
                                        float3 _6398 = _6200 + (_6195 * _6381);
                                        bool3 _6399 = isnan(_6398);
                                        bool3 _6400 = isinf(_6398);
                                        if (!all(not(bool3(_6399.x || _6400.x, _6399.y || _6400.y, _6399.z || _6400.z))))
                                        {
                                            _6425 = _6195;
                                            _6426 = _6198;
                                            _6427 = _6200;
                                            _6428 = false;
                                            _6429 = true;
                                            break;
                                        }
                                        _6199 = precise::max(0.0500000007450580596923828125, _6381 * 9.9999997473787516355514526367188e-06);
                                        _6201 = _6398 + (_6365 * _6199);
                                        _6196 = reflect(_6195, _6365);
                                        bool3 _6409 = isnan(_6201);
                                        bool3 _6410 = isinf(_6201);
                                        bool _6422;
                                        if (all(not(bool3(_6409.x || _6410.x, _6409.y || _6410.y, _6409.z || _6410.z))))
                                        {
                                            bool3 _6416 = isnan(_6196);
                                            bool3 _6417 = isinf(_6196);
                                            _6422 = !all(not(bool3(_6416.x || _6417.x, _6416.y || _6417.y, _6416.z || _6417.z)));
                                        }
                                        else
                                        {
                                            _6422 = true;
                                        }
                                        if (_6422)
                                        {
                                            _6425 = _6196;
                                            _6426 = _6199;
                                            _6427 = _6201;
                                            _6428 = false;
                                            _6429 = true;
                                            break;
                                        }
                                        _6195 = _6196;
                                        _6198 = _6199;
                                        _6200 = _6201;
                                        _6202++;
                                        continue;
                                    }
                                    else
                                    {
                                        _6425 = _6195;
                                        _6426 = _6198;
                                        _6427 = _6200;
                                        _6428 = _783;
                                        _6429 = false;
                                        break;
                                    }
                                }
                                if (_6429)
                                {
                                    _6431 = _6425;
                                    _6432 = _6426;
                                    _6433 = _6427;
                                    _6434 = _6428;
                                    break;
                                }
                                _6431 = _6425;
                                _6432 = _6426;
                                _6433 = _6427;
                                _6434 = true;
                                break;
                            } while(false);
                            if (!_6434)
                            {
                                _6486 = float2(0.0);
                                _6487 = false;
                                break;
                            }
                            bool _6438 = _1521.w == 0.0;
                            float3 _6443;
                            if (_6438)
                            {
                                _6443 = _1521.xyz;
                            }
                            else
                            {
                                _6443 = _1521.xyz - _6433;
                            }
                            float _6444 = dot(_6443, _6443);
                            bool _6457;
                            if (!(isnan(_6444) || isinf(_6444)))
                            {
                                float _6455;
                                if (_6438)
                                {
                                    _6455 = 9.9999996826552253889678874634872e-21;
                                }
                                else
                                {
                                    _6455 = _6432 * _6432;
                                }
                                _6457 = _6444 <= _6455;
                            }
                            else
                            {
                                _6457 = true;
                            }
                            if (_6457)
                            {
                                _6486 = float2(0.0);
                                _6487 = false;
                                break;
                            }
                            float3 _6461 = _6443 * rsqrt(_6444);
                            if (dot(_6461, _6431) <= 0.0)
                            {
                                _6486 = float2(0.0);
                                _6487 = false;
                                break;
                            }
                            float3 _6472 = fast::normalize(cross(_6431, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_6431.y) < 0.949999988079071044921875))));
                            float2 _6480 = float2(dot(_6461, _6472), dot(_6461, cross(_6431, _6472))) * precise::max(Settings.projection.x, Settings.projection.y);
                            bool2 _6481 = isnan(_6480);
                            bool2 _6482 = isinf(_6480);
                            _6486 = _6480;
                            _6487 = all(not(bool2(_6481.x || _6482.x, _6481.y || _6482.y)));
                            break;
                        } while(false);
                        bool _6496;
                        if (_6487)
                        {
                            _6496 = precise::max(abs(_6486.x), abs(_6486.y)) <= 0.00200000009499490261077880859375;
                        }
                        else
                        {
                            _6496 = false;
                        }
                        if (_6496)
                        {
                            bool _7281;
                            float4 _7317;
                            do
                            {
                                spvUnsafeArray<ReflectionSpecularPlane, 4> _193 = _206;
                                float _6898;
                                float3 _7280;
                                do
                                {
                                    bool _6736;
                                    if (_1525)
                                    {
                                        bool _6734;
                                        do
                                        {
                                            bool _6511;
                                            if (_1998)
                                            {
                                                _6511 = _1153.w == 2.0;
                                            }
                                            else
                                            {
                                                _6511 = false;
                                            }
                                            bool _6516;
                                            if (_6511)
                                            {
                                                _6516 = _1154.w == 2.0;
                                            }
                                            else
                                            {
                                                _6516 = false;
                                            }
                                            bool _6532;
                                            if (!_6516)
                                            {
                                                bool _6525;
                                                if ((isunordered(_1152.w, 1.0) || _1152.w == 1.0))
                                                {
                                                    _6525 = _1153.w != 1.0;
                                                }
                                                else
                                                {
                                                    _6525 = true;
                                                }
                                                bool _6531;
                                                if (!_6525)
                                                {
                                                    _6531 = _1154.w != 1.0;
                                                }
                                                else
                                                {
                                                    _6531 = true;
                                                }
                                                _6532 = _6531;
                                            }
                                            else
                                            {
                                                _6532 = false;
                                            }
                                            bool _6542;
                                            if (!_6532)
                                            {
                                                bool4 _6536 = isnan(_1152);
                                                bool4 _6537 = isinf(_1152);
                                                _6542 = !all(not(bool4(_6536.x || _6537.x, _6536.y || _6537.y, _6536.z || _6537.z, _6536.w || _6537.w)));
                                            }
                                            else
                                            {
                                                _6542 = true;
                                            }
                                            bool _6552;
                                            if (!_6542)
                                            {
                                                bool4 _6546 = isnan(_1153);
                                                bool4 _6547 = isinf(_1153);
                                                _6552 = !all(not(bool4(_6546.x || _6547.x, _6546.y || _6547.y, _6546.z || _6547.z, _6546.w || _6547.w)));
                                            }
                                            else
                                            {
                                                _6552 = true;
                                            }
                                            bool _6562;
                                            if (!_6552)
                                            {
                                                bool4 _6556 = isnan(_1154);
                                                bool4 _6557 = isinf(_1154);
                                                _6562 = !all(not(bool4(_6556.x || _6557.x, _6556.y || _6557.y, _6556.z || _6557.z, _6556.w || _6557.w)));
                                            }
                                            else
                                            {
                                                _6562 = true;
                                            }
                                            bool _6572;
                                            if (!_6562)
                                            {
                                                bool4 _6566 = isnan(Settings.projection);
                                                bool4 _6567 = isinf(Settings.projection);
                                                _6572 = !all(not(bool4(_6566.x || _6567.x, _6566.y || _6567.y, _6566.z || _6567.z, _6566.w || _6567.w)));
                                            }
                                            else
                                            {
                                                _6572 = true;
                                            }
                                            bool _6582;
                                            if (!_6572)
                                            {
                                                bool4 _6576 = isnan(_1275);
                                                bool4 _6577 = isinf(_1275);
                                                _6582 = !all(not(bool4(_6576.x || _6577.x, _6576.y || _6577.y, _6576.z || _6577.z, _6576.w || _6577.w)));
                                            }
                                            else
                                            {
                                                _6582 = true;
                                            }
                                            bool _6592;
                                            if (!_6582)
                                            {
                                                bool4 _6586 = isnan(_1283);
                                                bool4 _6587 = isinf(_1283);
                                                _6592 = !all(not(bool4(_6586.x || _6587.x, _6586.y || _6587.y, _6586.z || _6587.z, _6586.w || _6587.w)));
                                            }
                                            else
                                            {
                                                _6592 = true;
                                            }
                                            bool _6600;
                                            if (!_6592)
                                            {
                                                _6600 = any(abs(_1152.xyz) > float3(999999995904.0));
                                            }
                                            else
                                            {
                                                _6600 = true;
                                            }
                                            bool _6608;
                                            if (!_6600)
                                            {
                                                _6608 = any(abs(_1153.xyz) > float3(999999995904.0));
                                            }
                                            else
                                            {
                                                _6608 = true;
                                            }
                                            bool _6616;
                                            if (!_6608)
                                            {
                                                _6616 = any(abs(_1154.xyz) > float3(999999995904.0));
                                            }
                                            else
                                            {
                                                _6616 = true;
                                            }
                                            bool _6623;
                                            if (!_6616)
                                            {
                                                _6623 = any(Settings.projection.xy <= float2(0.0));
                                            }
                                            else
                                            {
                                                _6623 = true;
                                            }
                                            bool _6630;
                                            if (!_6623)
                                            {
                                                _6630 = any(abs(Settings.projection) > float4(999999995904.0));
                                            }
                                            else
                                            {
                                                _6630 = true;
                                            }
                                            bool _6636;
                                            if (!_6630)
                                            {
                                                _6636 = any(_2033 < float2(1.0));
                                            }
                                            else
                                            {
                                                _6636 = true;
                                            }
                                            bool _6642;
                                            if (!_6636)
                                            {
                                                _6642 = any(_2033 > float2(16384.0));
                                            }
                                            else
                                            {
                                                _6642 = true;
                                            }
                                            bool _6647;
                                            if (!_6642)
                                            {
                                                _6647 = Settings.clip.x <= 0.0;
                                            }
                                            else
                                            {
                                                _6647 = true;
                                            }
                                            bool _6652;
                                            if (!_6647)
                                            {
                                                _6652 = Settings.clip.y <= Settings.clip.x;
                                            }
                                            else
                                            {
                                                _6652 = true;
                                            }
                                            bool _6657;
                                            if (!_6652)
                                            {
                                                _6657 = Settings.clip.y > 999999995904.0;
                                            }
                                            else
                                            {
                                                _6657 = true;
                                            }
                                            bool _6663;
                                            if (!_6657)
                                            {
                                                _6663 = _1283.x < 0.0;
                                            }
                                            else
                                            {
                                                _6663 = true;
                                            }
                                            bool _6669;
                                            if (!_6663)
                                            {
                                                _6669 = _1283.x > 1.0;
                                            }
                                            else
                                            {
                                                _6669 = true;
                                            }
                                            bool _6675;
                                            if (!_6669)
                                            {
                                                _6675 = _1283.y < 0.0;
                                            }
                                            else
                                            {
                                                _6675 = true;
                                            }
                                            bool _6681;
                                            if (!_6675)
                                            {
                                                _6681 = _1283.y > 7.0;
                                            }
                                            else
                                            {
                                                _6681 = true;
                                            }
                                            bool _6688;
                                            if (!_6681)
                                            {
                                                _6688 = floor(_1283.y) != _1283.y;
                                            }
                                            else
                                            {
                                                _6688 = true;
                                            }
                                            bool _6695;
                                            if (!_6688)
                                            {
                                                _6695 = any(_1283.zw != float2(0.0));
                                            }
                                            else
                                            {
                                                _6695 = true;
                                            }
                                            if (_6695)
                                            {
                                                _6734 = false;
                                                break;
                                            }
                                            bool _6703;
                                            if (_6516)
                                            {
                                                _6703 = any(_1154.xyz != float3(0.0));
                                            }
                                            else
                                            {
                                                _6703 = false;
                                            }
                                            if (_6703)
                                            {
                                                _6734 = false;
                                                break;
                                            }
                                            float3 _6716;
                                            if (_6516)
                                            {
                                                _6716 = _1153.xyz;
                                            }
                                            else
                                            {
                                                _6716 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                            }
                                            float _6717 = dot(_6716, _6716);
                                            bool3 _6718 = isnan(_6716);
                                            bool3 _6719 = isinf(_6716);
                                            bool _6729;
                                            if (all(not(bool3(_6718.x || _6719.x, _6718.y || _6719.y, _6718.z || _6719.z))))
                                            {
                                                _6729 = !(isnan(_6717) || isinf(_6717));
                                            }
                                            else
                                            {
                                                _6729 = false;
                                            }
                                            bool _6733;
                                            if (_6729)
                                            {
                                                _6733 = _6717 > 9.9999996826552253889678874634872e-21;
                                            }
                                            else
                                            {
                                                _6733 = false;
                                            }
                                            _6734 = _6733;
                                            break;
                                        } while(false);
                                        _6736 = !_6734;
                                    }
                                    else
                                    {
                                        _6736 = true;
                                    }
                                    float _6895;
                                    float3 _6896;
                                    float3 _6897;
                                    bool _6899;
                                    if (!_6736)
                                    {
                                        float _6889;
                                        float3 _6890;
                                        float3 _6891;
                                        float _6892;
                                        bool _6893;
                                        do
                                        {
                                            bool2 _6742 = isnan(_5783);
                                            bool2 _6743 = isinf(_5783);
                                            bool _6751;
                                            if (all(not(bool2(_6742.x || _6743.x, _6742.y || _6743.y))))
                                            {
                                                _6751 = any(_5783 < float2(0.0));
                                            }
                                            else
                                            {
                                                _6751 = true;
                                            }
                                            bool _6757;
                                            if (!_6751)
                                            {
                                                _6757 = any(_5783 >= _2033);
                                            }
                                            else
                                            {
                                                _6757 = true;
                                            }
                                            if (_6757)
                                            {
                                                _6889 = 0.0;
                                                _6890 = float3(0.0);
                                                _6891 = float3(0.0);
                                                _6892 = 0.0;
                                                _6893 = false;
                                                break;
                                            }
                                            bool _6764;
                                            if (_1998)
                                            {
                                                _6764 = _1153.w == 2.0;
                                            }
                                            else
                                            {
                                                _6764 = false;
                                            }
                                            bool _6769;
                                            if (_6764)
                                            {
                                                _6769 = _1154.w == 2.0;
                                            }
                                            else
                                            {
                                                _6769 = false;
                                            }
                                            float3 _6780;
                                            if (_6769)
                                            {
                                                _6780 = _1153.xyz;
                                            }
                                            else
                                            {
                                                _6780 = cross(_1153.xyz - _1152.xyz, _1154.xyz - _1152.xyz);
                                            }
                                            float3 _6781 = fast::normalize(_6780);
                                            float3 _6789 = fast::normalize(float3((_5783 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                            float3 _6795;
                                            if (dot(_6781, _6789) > 0.0)
                                            {
                                                _6795 = -_6781;
                                            }
                                            else
                                            {
                                                _6795 = _6781;
                                            }
                                            float _6796 = dot(_6789, _6795);
                                            bool _6805;
                                            if (!(isnan(_6796) || isinf(_6796)))
                                            {
                                                _6805 = abs(_6796) <= 9.9999999600419720025001879548654e-13;
                                            }
                                            else
                                            {
                                                _6805 = true;
                                            }
                                            if (_6805)
                                            {
                                                _6889 = 0.0;
                                                _6890 = float3(0.0);
                                                _6891 = float3(0.0);
                                                _6892 = 0.0;
                                                _6893 = false;
                                                break;
                                            }
                                            float _6810 = dot(_1152.xyz, _6795) / _6796;
                                            float _6812 = _6810 * _6789.z;
                                            bool _6820;
                                            if (!(isnan(_6810) || isinf(_6810)))
                                            {
                                                _6820 = _6810 <= 0.0;
                                            }
                                            else
                                            {
                                                _6820 = true;
                                            }
                                            bool _6825;
                                            if (!_6820)
                                            {
                                                _6825 = _6812 < Settings.clip.x;
                                            }
                                            else
                                            {
                                                _6825 = true;
                                            }
                                            bool _6830;
                                            if (!_6825)
                                            {
                                                _6830 = _6812 > Settings.clip.y;
                                            }
                                            else
                                            {
                                                _6830 = true;
                                            }
                                            if (_6830)
                                            {
                                                _6889 = 0.0;
                                                _6890 = float3(0.0);
                                                _6891 = float3(0.0);
                                                _6892 = _6812;
                                                _6893 = false;
                                                break;
                                            }
                                            bool _6837;
                                            if (_1998)
                                            {
                                                _6837 = _1153.w == 2.0;
                                            }
                                            else
                                            {
                                                _6837 = false;
                                            }
                                            bool _6842;
                                            if (_6837)
                                            {
                                                _6842 = _1154.w == 2.0;
                                            }
                                            else
                                            {
                                                _6842 = false;
                                            }
                                            float _6845 = precise::max(_6842 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _6810 * 9.9999997473787516355514526367188e-06);
                                            float3 _6848 = (_6789 * _6810) + (_6795 * _6845);
                                            float3 _6849 = reflect(_6789, _6795);
                                            uint _6852 = uint(_1283.y);
                                            float3 _6859 = fast::normalize(cross(_6849, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_6849.y) < 0.949999988079071044921875))));
                                            float3 _6871 = fast::normalize(_6849 + (((_6859 * _175[_6852].x) + (cross(_6849, _6859) * _175[_6852].y)) * (_1283.x * _1283.x)));
                                            float3 _6875 = select(_6871, _6849, bool3(dot(_6871, _6795) <= 0.0));
                                            bool3 _6876 = isnan(_6848);
                                            bool3 _6877 = isinf(_6848);
                                            bool _6888;
                                            if (all(not(bool3(_6876.x || _6877.x, _6876.y || _6877.y, _6876.z || _6877.z))))
                                            {
                                                bool3 _6883 = isnan(_6875);
                                                bool3 _6884 = isinf(_6875);
                                                _6888 = all(not(bool3(_6883.x || _6884.x, _6883.y || _6884.y, _6883.z || _6884.z)));
                                            }
                                            else
                                            {
                                                _6888 = false;
                                            }
                                            _6889 = _6845;
                                            _6890 = _6848;
                                            _6891 = _6875;
                                            _6892 = _6812;
                                            _6893 = _6888;
                                            break;
                                        } while(false);
                                        _6895 = _6889;
                                        _6896 = _6890;
                                        _6897 = _6891;
                                        _6898 = _6892;
                                        _6899 = !_6893;
                                    }
                                    else
                                    {
                                        _6895 = 0.0;
                                        _6896 = float3(0.0);
                                        _6897 = float3(0.0);
                                        _6898 = 0.0;
                                        _6899 = true;
                                    }
                                    if (_6899)
                                    {
                                        _7280 = _6896;
                                        _7281 = false;
                                        break;
                                    }
                                    bool _6974;
                                    do
                                    {
                                        bool _6908;
                                        if (_1998)
                                        {
                                            _6908 = _1153.w == 2.0;
                                        }
                                        else
                                        {
                                            _6908 = false;
                                        }
                                        bool _6913;
                                        if (_6908)
                                        {
                                            _6913 = _1154.w == 2.0;
                                        }
                                        else
                                        {
                                            _6913 = false;
                                        }
                                        if (_6913)
                                        {
                                            _6974 = true;
                                            break;
                                        }
                                        float3 _6926 = _1153.xyz - _1152.xyz;
                                        float3 _6928 = _1154.xyz - _1152.xyz;
                                        float3 _6929 = (float3((_5783 - Settings.projection.zw) / Settings.projection.xy, 1.0) * _6898) - _1152.xyz;
                                        float _6930 = dot(_6926, _6926);
                                        float _6931 = dot(_6926, _6928);
                                        float _6932 = dot(_6928, _6928);
                                        float _6933 = dot(_6929, _6926);
                                        float _6934 = dot(_6929, _6928);
                                        float _6937 = (_6930 * _6932) - (_6931 * _6931);
                                        bool _6945;
                                        if (!(isnan(_6937) || isinf(_6937)))
                                        {
                                            _6945 = _6937 <= 9.9999996826552253889678874634872e-21;
                                        }
                                        else
                                        {
                                            _6945 = true;
                                        }
                                        if (_6945)
                                        {
                                            _6974 = false;
                                            break;
                                        }
                                        float2 _6956 = float2((_6932 * _6933) - (_6931 * _6934), (_6930 * _6934) - (_6931 * _6933)) / float2(_6937);
                                        bool2 _6957 = isnan(_6956);
                                        bool2 _6958 = isinf(_6956);
                                        bool _6966;
                                        if (all(not(bool2(_6957.x || _6958.x, _6957.y || _6958.y))))
                                        {
                                            _6966 = all(_6956 >= float2(-9.9999997473787516355514526367188e-06));
                                        }
                                        else
                                        {
                                            _6966 = false;
                                        }
                                        bool _6973;
                                        if (_6966)
                                        {
                                            _6973 = (_6956.x + _6956.y) <= 1.000010013580322265625;
                                        }
                                        else
                                        {
                                            _6973 = false;
                                        }
                                        _6974 = _6973;
                                        break;
                                    } while(false);
                                    if (!_6974)
                                    {
                                        _7280 = _6896;
                                        _7281 = false;
                                        break;
                                    }
                                    float3 _6982;
                                    float3 _6984;
                                    _6982 = _6896;
                                    _6984 = _6897;
                                    float _6980;
                                    float3 _6983;
                                    float3 _6985;
                                    float3 _7276;
                                    bool _7277;
                                    bool _7278;
                                    float _6979 = _6895;
                                    uint _6986 = 0u;
                                    for (;;)
                                    {
                                        if (_6986 < _929)
                                        {
                                            bool _6999;
                                            bool _7118;
                                            do
                                            {
                                                _6999 = _193[_6986].a.w == 2.0;
                                                bool _7004;
                                                if (_6999)
                                                {
                                                    _7004 = _193[_6986].b.w == 2.0;
                                                }
                                                else
                                                {
                                                    _7004 = false;
                                                }
                                                bool _7009;
                                                if (_7004)
                                                {
                                                    _7009 = _193[_6986].c.w == 2.0;
                                                }
                                                else
                                                {
                                                    _7009 = false;
                                                }
                                                bool _7025;
                                                if (!_7009)
                                                {
                                                    bool _7018;
                                                    if ((isunordered(_193[_6986].a.w, 1.0) || _193[_6986].a.w == 1.0))
                                                    {
                                                        _7018 = _193[_6986].b.w != 1.0;
                                                    }
                                                    else
                                                    {
                                                        _7018 = true;
                                                    }
                                                    bool _7024;
                                                    if (!_7018)
                                                    {
                                                        _7024 = _193[_6986].c.w != 1.0;
                                                    }
                                                    else
                                                    {
                                                        _7024 = true;
                                                    }
                                                    _7025 = _7024;
                                                }
                                                else
                                                {
                                                    _7025 = false;
                                                }
                                                bool _7035;
                                                if (!_7025)
                                                {
                                                    bool4 _7029 = isnan(_193[_6986].a);
                                                    bool4 _7030 = isinf(_193[_6986].a);
                                                    _7035 = !all(not(bool4(_7029.x || _7030.x, _7029.y || _7030.y, _7029.z || _7030.z, _7029.w || _7030.w)));
                                                }
                                                else
                                                {
                                                    _7035 = true;
                                                }
                                                bool _7045;
                                                if (!_7035)
                                                {
                                                    bool4 _7039 = isnan(_193[_6986].b);
                                                    bool4 _7040 = isinf(_193[_6986].b);
                                                    _7045 = !all(not(bool4(_7039.x || _7040.x, _7039.y || _7040.y, _7039.z || _7040.z, _7039.w || _7040.w)));
                                                }
                                                else
                                                {
                                                    _7045 = true;
                                                }
                                                bool _7055;
                                                if (!_7045)
                                                {
                                                    bool4 _7049 = isnan(_193[_6986].c);
                                                    bool4 _7050 = isinf(_193[_6986].c);
                                                    _7055 = !all(not(bool4(_7049.x || _7050.x, _7049.y || _7050.y, _7049.z || _7050.z, _7049.w || _7050.w)));
                                                }
                                                else
                                                {
                                                    _7055 = true;
                                                }
                                                bool _7063;
                                                if (!_7055)
                                                {
                                                    _7063 = any(abs(_193[_6986].a.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _7063 = true;
                                                }
                                                bool _7071;
                                                if (!_7063)
                                                {
                                                    _7071 = any(abs(_193[_6986].b.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _7071 = true;
                                                }
                                                bool _7079;
                                                if (!_7071)
                                                {
                                                    _7079 = any(abs(_193[_6986].c.xyz) > float3(999999995904.0));
                                                }
                                                else
                                                {
                                                    _7079 = true;
                                                }
                                                if (_7079)
                                                {
                                                    _7118 = false;
                                                    break;
                                                }
                                                bool _7087;
                                                if (_7009)
                                                {
                                                    _7087 = any(_193[_6986].c.xyz != float3(0.0));
                                                }
                                                else
                                                {
                                                    _7087 = false;
                                                }
                                                if (_7087)
                                                {
                                                    _7118 = false;
                                                    break;
                                                }
                                                float3 _7100;
                                                if (_7009)
                                                {
                                                    _7100 = _193[_6986].b.xyz;
                                                }
                                                else
                                                {
                                                    _7100 = cross(_193[_6986].b.xyz - _193[_6986].a.xyz, _193[_6986].c.xyz - _193[_6986].a.xyz);
                                                }
                                                float _7101 = dot(_7100, _7100);
                                                bool3 _7102 = isnan(_7100);
                                                bool3 _7103 = isinf(_7100);
                                                bool _7113;
                                                if (all(not(bool3(_7102.x || _7103.x, _7102.y || _7103.y, _7102.z || _7103.z))))
                                                {
                                                    _7113 = !(isnan(_7101) || isinf(_7101));
                                                }
                                                else
                                                {
                                                    _7113 = false;
                                                }
                                                bool _7117;
                                                if (_7113)
                                                {
                                                    _7117 = _7101 > 9.9999996826552253889678874634872e-21;
                                                }
                                                else
                                                {
                                                    _7117 = false;
                                                }
                                                _7118 = _7117;
                                                break;
                                            } while(false);
                                            if (!_7118)
                                            {
                                                _7276 = _6982;
                                                _7277 = false;
                                                _7278 = true;
                                                break;
                                            }
                                            bool _7126;
                                            if (_6999)
                                            {
                                                _7126 = _193[_6986].b.w == 2.0;
                                            }
                                            else
                                            {
                                                _7126 = false;
                                            }
                                            bool _7131;
                                            if (_7126)
                                            {
                                                _7131 = _193[_6986].c.w == 2.0;
                                            }
                                            else
                                            {
                                                _7131 = false;
                                            }
                                            float3 _7142;
                                            if (_7131)
                                            {
                                                _7142 = _193[_6986].b.xyz;
                                            }
                                            else
                                            {
                                                _7142 = cross(_193[_6986].b.xyz - _193[_6986].a.xyz, _193[_6986].c.xyz - _193[_6986].a.xyz);
                                            }
                                            float3 _7143 = fast::normalize(_7142);
                                            float3 _7149;
                                            if (dot(_7143, _6984) > 0.0)
                                            {
                                                _7149 = -_7143;
                                            }
                                            else
                                            {
                                                _7149 = _7143;
                                            }
                                            float _7150 = dot(_6984, _7149);
                                            bool _7159;
                                            if (!(isnan(_7150) || isinf(_7150)))
                                            {
                                                _7159 = abs(_7150) <= 9.9999999600419720025001879548654e-13;
                                            }
                                            else
                                            {
                                                _7159 = true;
                                            }
                                            if (_7159)
                                            {
                                                _7276 = _6982;
                                                _7277 = false;
                                                _7278 = true;
                                                break;
                                            }
                                            float _7165 = dot(_193[_6986].a.xyz - _6982, _7149) / _7150;
                                            bool _7173;
                                            if (!(isnan(_7165) || isinf(_7165)))
                                            {
                                                _7173 = _7165 <= _6979;
                                            }
                                            else
                                            {
                                                _7173 = true;
                                            }
                                            bool _7178;
                                            if (!_7173)
                                            {
                                                _7178 = _7165 >= 65536.0;
                                            }
                                            else
                                            {
                                                _7178 = true;
                                            }
                                            if (_7178)
                                            {
                                                _7276 = _6982;
                                                _7277 = false;
                                                _7278 = true;
                                                break;
                                            }
                                            float3 _7182 = _6982 + (_6984 * _7165);
                                            bool3 _7183 = isnan(_7182);
                                            bool3 _7184 = isinf(_7182);
                                            bool _7255;
                                            if (all(not(bool3(_7183.x || _7184.x, _7183.y || _7184.y, _7183.z || _7184.z))))
                                            {
                                                bool _7253;
                                                do
                                                {
                                                    bool _7196;
                                                    if (_6999)
                                                    {
                                                        _7196 = _193[_6986].b.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _7196 = false;
                                                    }
                                                    bool _7201;
                                                    if (_7196)
                                                    {
                                                        _7201 = _193[_6986].c.w == 2.0;
                                                    }
                                                    else
                                                    {
                                                        _7201 = false;
                                                    }
                                                    if (_7201)
                                                    {
                                                        _7253 = true;
                                                        break;
                                                    }
                                                    float3 _7205 = _193[_6986].b.xyz - _193[_6986].a.xyz;
                                                    float3 _7207 = _193[_6986].c.xyz - _193[_6986].a.xyz;
                                                    float3 _7208 = _7182 - _193[_6986].a.xyz;
                                                    float _7209 = dot(_7205, _7205);
                                                    float _7210 = dot(_7205, _7207);
                                                    float _7211 = dot(_7207, _7207);
                                                    float _7212 = dot(_7208, _7205);
                                                    float _7213 = dot(_7208, _7207);
                                                    float _7216 = (_7209 * _7211) - (_7210 * _7210);
                                                    bool _7224;
                                                    if (!(isnan(_7216) || isinf(_7216)))
                                                    {
                                                        _7224 = _7216 <= 9.9999996826552253889678874634872e-21;
                                                    }
                                                    else
                                                    {
                                                        _7224 = true;
                                                    }
                                                    if (_7224)
                                                    {
                                                        _7253 = false;
                                                        break;
                                                    }
                                                    float2 _7235 = float2((_7211 * _7212) - (_7210 * _7213), (_7209 * _7213) - (_7210 * _7212)) / float2(_7216);
                                                    bool2 _7236 = isnan(_7235);
                                                    bool2 _7237 = isinf(_7235);
                                                    bool _7245;
                                                    if (all(not(bool2(_7236.x || _7237.x, _7236.y || _7237.y))))
                                                    {
                                                        _7245 = all(_7235 >= float2(-9.9999997473787516355514526367188e-06));
                                                    }
                                                    else
                                                    {
                                                        _7245 = false;
                                                    }
                                                    bool _7252;
                                                    if (_7245)
                                                    {
                                                        _7252 = (_7235.x + _7235.y) <= 1.000010013580322265625;
                                                    }
                                                    else
                                                    {
                                                        _7252 = false;
                                                    }
                                                    _7253 = _7252;
                                                    break;
                                                } while(false);
                                                _7255 = !_7253;
                                            }
                                            else
                                            {
                                                _7255 = true;
                                            }
                                            if (_7255)
                                            {
                                                _7276 = _6982;
                                                _7277 = false;
                                                _7278 = true;
                                                break;
                                            }
                                            _6980 = precise::max(0.0500000007450580596923828125, _7165 * 9.9999997473787516355514526367188e-06);
                                            _6983 = _7182 + (_7149 * _6980);
                                            _6985 = reflect(_6984, _7149);
                                            bool3 _7260 = isnan(_6983);
                                            bool3 _7261 = isinf(_6983);
                                            bool _7273;
                                            if (all(not(bool3(_7260.x || _7261.x, _7260.y || _7261.y, _7260.z || _7261.z))))
                                            {
                                                bool3 _7267 = isnan(_6985);
                                                bool3 _7268 = isinf(_6985);
                                                _7273 = !all(not(bool3(_7267.x || _7268.x, _7267.y || _7268.y, _7267.z || _7268.z)));
                                            }
                                            else
                                            {
                                                _7273 = true;
                                            }
                                            if (_7273)
                                            {
                                                _7276 = _6983;
                                                _7277 = false;
                                                _7278 = true;
                                                break;
                                            }
                                            _6979 = _6980;
                                            _6982 = _6983;
                                            _6984 = _6985;
                                            _6986++;
                                            continue;
                                        }
                                        else
                                        {
                                            _7276 = _6982;
                                            _7277 = _781;
                                            _7278 = false;
                                            break;
                                        }
                                    }
                                    if (_7278)
                                    {
                                        _7280 = _7276;
                                        _7281 = _7277;
                                        break;
                                    }
                                    _7280 = _7276;
                                    _7281 = true;
                                    break;
                                } while(false);
                                if (!_7281)
                                {
                                    _7317 = float4(0.0);
                                    break;
                                }
                                if (_2023)
                                {
                                    float3 _7289 = precise::max(abs(_7280), abs(_1521.xyz));
                                    float _7297 = length(_1521.xyz - _7280);
                                    bool _7311;
                                    if (!(isnan(_7297) || isinf(_7297)))
                                    {
                                        _7311 = ((precise::max(1.0, precise::max(_7289.x, precise::max(_7289.y, _7289.z))) * 9.9999999747524270787835121154785e-07) * precise::max(Settings.projection.x, Settings.projection.y)) > (_7297 * 0.01200000010430812835693359375);
                                    }
                                    else
                                    {
                                        _7311 = true;
                                    }
                                    if (_7311)
                                    {
                                        _7317 = float4(0.0);
                                        break;
                                    }
                                }
                                _7317 = float4(_5783, _6898, 1.0);
                                break;
                            } while(false);
                            _7318 = _7281;
                            _7319 = _6434;
                            _7320 = _5784;
                            _7321 = _5785;
                            _7322 = _5786;
                            _7323 = _5787;
                            _7324 = _5788;
                            _7325 = _5789;
                            _7326 = _7317;
                            break;
                        }
                        _7318 = _781;
                        _7319 = _6434;
                        _7320 = _5784;
                        _7321 = _5785;
                        _7322 = _5786;
                        _7323 = _5787;
                        _7324 = _5788;
                        _7325 = _5789;
                        _7326 = float4(0.0);
                        break;
                    } while(false);
                    bool _8197;
                    bool _8198;
                    float3 _8199;
                    bool _8200;
                    do
                    {
                        bool _7337;
                        if ((Settings.flags & 1u) != 0u)
                        {
                            _7337 = Settings.weight == 0.0;
                        }
                        else
                        {
                            _7337 = true;
                        }
                        bool _7343;
                        if (!_7337)
                        {
                            _7343 = _7326.w != 1.0;
                        }
                        else
                        {
                            _7343 = true;
                        }
                        bool _7353;
                        if (!_7343)
                        {
                            bool4 _7347 = isnan(_7326);
                            bool4 _7348 = isinf(_7326);
                            _7353 = !all(not(bool4(_7347.x || _7348.x, _7347.y || _7348.y, _7347.z || _7348.z, _7347.w || _7348.w)));
                        }
                        else
                        {
                            _7353 = true;
                        }
                        bool _7359;
                        if (!_7353)
                        {
                            _7359 = _7326.z <= 0.0;
                        }
                        else
                        {
                            _7359 = true;
                        }
                        if (_7359)
                        {
                            _8197 = _775;
                            _8198 = _777;
                            _8199 = float3(0.0);
                            _8200 = false;
                            break;
                        }
                        uint _7372;
                        if (_296)
                        {
                            _7372 = _241;
                        }
                        else
                        {
                            _7372 = geometry._m0[(Settings.previousMapping + (_241 * 4u)) >> 2u];
                        }
                        bool _7379;
                        if (_241 != 4294967294u)
                        {
                            _7379 = _7372 >= Settings.oldTriangles;
                        }
                        else
                        {
                            _7379 = false;
                        }
                        if (_7379)
                        {
                            _8197 = _775;
                            _8198 = _777;
                            _8199 = float3(0.0);
                            _8200 = false;
                            break;
                        }
                        float2 _7383 = _7326.xy - float2(0.5);
                        float2 _189 = _7383;
                        bool _7396;
                        if (!any(_7383 < float2(0.0)))
                        {
                            _7396 = any(_7383 > float2(float(Settings.previousWidth - 1u), float(Settings.previousHeight - 1u)));
                        }
                        else
                        {
                            _7396 = true;
                        }
                        if (_7396)
                        {
                            _8197 = _775;
                            _8198 = _777;
                            _8199 = float3(0.0);
                            _8200 = false;
                            break;
                        }
                        int2 _7400 = int2(floor(_7383));
                        float2 _7402 = _7383 - float2(_7400);
                        uint _7403 = Settings.previousWidth * Settings.previousHeight;
                        float3 _7410;
                        _7410 = float3(0.0);
                        bool _7406;
                        bool _7409;
                        float3 _7411;
                        float _7413;
                        bool _7415;
                        bool _7419;
                        bool _8169;
                        bool _8170;
                        float3 _8171;
                        bool _8172;
                        float _8173;
                        bool _8174;
                        bool _7405 = false;
                        bool _7408 = _775;
                        float _7412 = 0.0;
                        bool _7414 = _777;
                        uint _7416 = 0u;
                        bool _7418 = _772;
                        for (;;)
                        {
                            if (_7416 < 2u)
                            {
                                _7411 = _7410;
                                bool _7425;
                                float3 _7427;
                                float _7429;
                                bool _7431;
                                bool _7424 = _7408;
                                float _7428 = _7412;
                                bool _7430 = _7414;
                                uint _7432 = 0u;
                                for (;;)
                                {
                                    if (_7432 < 2u)
                                    {
                                        float _7444;
                                        if (_7432 != 0u)
                                        {
                                            _7444 = _7402.x;
                                        }
                                        else
                                        {
                                            _7444 = 1.0 - _7402.x;
                                        }
                                        float _7452;
                                        if (_7416 != 0u)
                                        {
                                            _7452 = _7402.y;
                                        }
                                        else
                                        {
                                            _7452 = 1.0 - _7402.y;
                                        }
                                        float _7453 = _7444 * _7452;
                                        if (_7453 <= 0.0)
                                        {
                                            _7425 = _7424;
                                            _7427 = _7411;
                                            _7429 = _7428;
                                            _7431 = _7430;
                                            uint _7433 = _7432 + 1u;
                                            _7424 = _7425;
                                            _7411 = _7427;
                                            _7428 = _7429;
                                            _7430 = _7431;
                                            _7432 = _7433;
                                            continue;
                                        }
                                        int2 _7466 = min((_7400 + int2(int(_7432), int(_7416))), int2(int(Settings.previousWidth - 1u), int(Settings.previousHeight - 1u)));
                                        int2 _190 = _7466;
                                        uint _7474 = (uint(_190.y) * Settings.previousWidth) + uint(_190.x);
                                        float _7481 = as_type<float>(history._m0[((_7403 * 8u) + (_7474 * 4u)) >> 2u]);
                                        uint _7482 = _7403 * _811;
                                        uint _7486 = _7482 + (((_7474 * Settings.lobes) + _803) * 52u);
                                        uint _7487 = _7486 >> 2u;
                                        uint4 _7499 = uint4(history._m0[_7487], history._m0[_7487 + 1u], history._m0[_7487 + 2u], history._m0[_7487 + 3u]);
                                        uint _7501 = (_7486 + 16u) >> 2u;
                                        uint _7504 = _7501 + 1u;
                                        uint _7508 = (_7486 + 24u) >> 2u;
                                        float3 _7518 = as_type<float3>(uint3(history._m0[_7508], history._m0[_7508 + 1u], history._m0[_7508 + 2u]));
                                        uint _7520 = (_7486 + 36u) >> 2u;
                                        uint _7524 = (_7486 + 40u) >> 2u;
                                        float3 _7534 = as_type<float3>(uint3(history._m0[_7524], history._m0[_7524 + 1u], history._m0[_7524 + 2u]));
                                        bool _7742;
                                        bool _7743;
                                        if (history._m0[(4u * (_7403 + _7474)) >> 2u] == _7372)
                                        {
                                            uint4 _188 = _7499;
                                            bool _7740;
                                            do
                                            {
                                                uint _7547 = history._m0[_7501] & 7u;
                                                uint _7548 = history._m0[_7501] >> 8u;
                                                bool _7553;
                                                if (_7547 <= 4u)
                                                {
                                                    _7553 = _7548 < 1u;
                                                }
                                                else
                                                {
                                                    _7553 = true;
                                                }
                                                bool _7558;
                                                if (!_7553)
                                                {
                                                    _7558 = _7548 > 3u;
                                                }
                                                else
                                                {
                                                    _7558 = true;
                                                }
                                                bool _7565;
                                                if (!_7558)
                                                {
                                                    _7565 = history._m0[_7501] != ((_7548 << 8u) | _7547);
                                                }
                                                else
                                                {
                                                    _7565 = true;
                                                }
                                                bool _7574;
                                                if (!_7565)
                                                {
                                                    bool _7573;
                                                    if (_7548 != 3u)
                                                    {
                                                        _7573 = _7547 == 4u;
                                                    }
                                                    else
                                                    {
                                                        _7573 = false;
                                                    }
                                                    _7574 = _7573;
                                                }
                                                else
                                                {
                                                    _7574 = true;
                                                }
                                                bool _7583;
                                                if (!_7574)
                                                {
                                                    bool _7582;
                                                    if (_7548 == 3u)
                                                    {
                                                        _7582 = _7547 != 4u;
                                                    }
                                                    else
                                                    {
                                                        _7582 = false;
                                                    }
                                                    _7583 = _7582;
                                                }
                                                else
                                                {
                                                    _7583 = true;
                                                }
                                                bool _7589;
                                                if (!_7583)
                                                {
                                                    _7589 = (history._m0[_7520] >> 24u) != 255u;
                                                }
                                                else
                                                {
                                                    _7589 = true;
                                                }
                                                bool _7599;
                                                if (!_7589)
                                                {
                                                    bool3 _7593 = isnan(_7518);
                                                    bool3 _7594 = isinf(_7518);
                                                    _7599 = !all(not(bool3(_7593.x || _7594.x, _7593.y || _7594.y, _7593.z || _7594.z)));
                                                }
                                                else
                                                {
                                                    _7599 = true;
                                                }
                                                bool _7609;
                                                if (!_7599)
                                                {
                                                    bool3 _7603 = isnan(_7534);
                                                    bool3 _7604 = isinf(_7534);
                                                    _7609 = !all(not(bool3(_7603.x || _7604.x, _7603.y || _7604.y, _7603.z || _7604.z)));
                                                }
                                                else
                                                {
                                                    _7609 = true;
                                                }
                                                bool _7615;
                                                if (!_7609)
                                                {
                                                    _7615 = any(_7534 < float3(0.0));
                                                }
                                                else
                                                {
                                                    _7615 = true;
                                                }
                                                bool _7621;
                                                if (!_7615)
                                                {
                                                    _7621 = any(_7534 > float3(1.0));
                                                }
                                                else
                                                {
                                                    _7621 = true;
                                                }
                                                if (_7621)
                                                {
                                                    _7740 = false;
                                                    break;
                                                }
                                                bool _7708;
                                                bool _7709;
                                                uint _7625 = 0u;
                                                for (;;)
                                                {
                                                    if (_7625 < 4u)
                                                    {
                                                        bool _7705;
                                                        if (_7625 < _7547)
                                                        {
                                                            bool _7704;
                                                            if (_188[_7625] == 4294967294u)
                                                            {
                                                                float _7651 = dot(Settings.previousNormal.xyz, Settings.previousNormal.xyz);
                                                                bool _7657;
                                                                if (Settings.scenePaths == 1u)
                                                                {
                                                                    _7657 = Settings.previousPoint.w == 1.0;
                                                                }
                                                                else
                                                                {
                                                                    _7657 = false;
                                                                }
                                                                bool _7662;
                                                                if (_7657)
                                                                {
                                                                    _7662 = Settings.previousNormal.w == 0.0;
                                                                }
                                                                else
                                                                {
                                                                    _7662 = false;
                                                                }
                                                                bool _7670;
                                                                if (_7662)
                                                                {
                                                                    bool4 _7665 = isnan(Settings.previousPoint);
                                                                    bool4 _7666 = isinf(Settings.previousPoint);
                                                                    _7670 = all(not(bool4(_7665.x || _7666.x, _7665.y || _7666.y, _7665.z || _7666.z, _7665.w || _7666.w)));
                                                                }
                                                                else
                                                                {
                                                                    _7670 = false;
                                                                }
                                                                bool _7678;
                                                                if (_7670)
                                                                {
                                                                    bool4 _7673 = isnan(Settings.previousNormal);
                                                                    bool4 _7674 = isinf(Settings.previousNormal);
                                                                    _7678 = all(not(bool4(_7673.x || _7674.x, _7673.y || _7674.y, _7673.z || _7674.z, _7673.w || _7674.w)));
                                                                }
                                                                else
                                                                {
                                                                    _7678 = false;
                                                                }
                                                                bool _7685;
                                                                if (_7678)
                                                                {
                                                                    _7685 = all(abs(Settings.previousPoint.xyz) <= float3(999999995904.0));
                                                                }
                                                                else
                                                                {
                                                                    _7685 = false;
                                                                }
                                                                bool _7691;
                                                                if (_7685)
                                                                {
                                                                    _7691 = all(abs(Settings.previousNormal.xyz) <= float3(999999995904.0));
                                                                }
                                                                else
                                                                {
                                                                    _7691 = false;
                                                                }
                                                                bool _7698;
                                                                if (_7691)
                                                                {
                                                                    _7698 = !(isnan(_7651) || isinf(_7651));
                                                                }
                                                                else
                                                                {
                                                                    _7698 = false;
                                                                }
                                                                bool _7702;
                                                                if (_7698)
                                                                {
                                                                    _7702 = _7651 > 9.9999996826552253889678874634872e-21;
                                                                }
                                                                else
                                                                {
                                                                    _7702 = false;
                                                                }
                                                                _7704 = !_7702;
                                                            }
                                                            else
                                                            {
                                                                _7704 = _188[_7625] >= Settings.oldTriangles;
                                                            }
                                                            _7705 = _7704;
                                                        }
                                                        else
                                                        {
                                                            _7705 = _188[_7625] != 4294967295u;
                                                        }
                                                        if (_7705)
                                                        {
                                                            _7708 = false;
                                                            _7709 = true;
                                                            break;
                                                        }
                                                        _7625++;
                                                        continue;
                                                    }
                                                    else
                                                    {
                                                        _7708 = _7430;
                                                        _7709 = false;
                                                        break;
                                                    }
                                                }
                                                if (_7709)
                                                {
                                                    _7740 = _7708;
                                                    break;
                                                }
                                                if (_7548 == 1u)
                                                {
                                                    bool _7719;
                                                    if (history._m0[_7504] < Settings.oldTriangles)
                                                    {
                                                        _7719 = _7518.z == 0.0;
                                                    }
                                                    else
                                                    {
                                                        _7719 = false;
                                                    }
                                                    bool _7725;
                                                    if (_7719)
                                                    {
                                                        _7725 = all(_7518.xy >= float2(0.0));
                                                    }
                                                    else
                                                    {
                                                        _7725 = false;
                                                    }
                                                    bool _7731;
                                                    if (_7725)
                                                    {
                                                        _7731 = dot(_7518.xy, float2(1.0)) <= 1.0;
                                                    }
                                                    else
                                                    {
                                                        _7731 = false;
                                                    }
                                                    _7740 = _7731;
                                                    break;
                                                }
                                                bool _7739;
                                                if (history._m0[_7504] == 4294967295u)
                                                {
                                                    _7739 = abs(dot(_7518, _7518) - 1.0) < 9.9999997473787516355514526367188e-05;
                                                }
                                                else
                                                {
                                                    _7739 = false;
                                                }
                                                _7740 = _7739;
                                                break;
                                            } while(false);
                                            _7742 = _7740;
                                            _7743 = !_7740;
                                        }
                                        else
                                        {
                                            _7742 = _7430;
                                            _7743 = true;
                                        }
                                        bool _7758;
                                        if (!_7743)
                                        {
                                            bool _7751;
                                            if (history._m0[_7501] == _833)
                                            {
                                                _7751 = history._m0[_7504] == _1066;
                                            }
                                            else
                                            {
                                                _7751 = false;
                                            }
                                            bool _7756;
                                            if (_7751)
                                            {
                                                _7756 = all(_7499 == _184);
                                            }
                                            else
                                            {
                                                _7756 = false;
                                            }
                                            _7758 = !_7756;
                                        }
                                        else
                                        {
                                            _7758 = true;
                                        }
                                        bool _7765;
                                        if (!_7758)
                                        {
                                            _7765 = isnan(_7481) || isinf(_7481);
                                        }
                                        else
                                        {
                                            _7765 = true;
                                        }
                                        bool _7770;
                                        if (!_7765)
                                        {
                                            _7770 = _7481 <= 0.0;
                                        }
                                        else
                                        {
                                            _7770 = true;
                                        }
                                        if (_7770)
                                        {
                                            _7409 = _7424;
                                            _7415 = _7742;
                                            _7419 = false;
                                            _7413 = _7428;
                                            _7406 = true;
                                            break;
                                        }
                                        float _7774 = _7428 + (_7453 / _7481);
                                        float3 _7776;
                                        bool _7781;
                                        _7776 = float3(1.9999999949504854157567024230957e-06);
                                        _7781 = _7424;
                                        float3 _7777;
                                        bool _7782;
                                        for (int _7779 = 0; _7779 < 2; _7776 = _7777, _7779++, _7781 = _7782)
                                        {
                                            float3 _7789;
                                            _7782 = _7781;
                                            _7789 = float3(0.0);
                                            bool _7787;
                                            float3 _7790;
                                            for (int _7791 = -1; _7791 <= 1; _7782 = _7787, _7789 = _7790, _7791 += 2)
                                            {
                                                int2 _191 = _7466;
                                                uint _7796 = uint(_7779);
                                                _191[_7796] += _7791;
                                                bool _7812;
                                                if (!any(_191 < int2(0)))
                                                {
                                                    _7812 = any(_191 >= int2(int(Settings.previousWidth), int(Settings.previousHeight)));
                                                }
                                                else
                                                {
                                                    _7812 = true;
                                                }
                                                if (_7812)
                                                {
                                                    _7787 = _7782;
                                                    _7790 = _7789;
                                                    continue;
                                                }
                                                uint _7822 = (uint(_191.y) * Settings.previousWidth) + uint(_191.x);
                                                uint _7826 = _7482 + (((_7822 * Settings.lobes) + _803) * 52u);
                                                uint _7827 = _7826 >> 2u;
                                                uint4 _7839 = uint4(history._m0[_7827], history._m0[_7827 + 1u], history._m0[_7827 + 2u], history._m0[_7827 + 3u]);
                                                uint _7841 = (_7826 + 16u) >> 2u;
                                                uint _7844 = _7841 + 1u;
                                                uint _7848 = (_7826 + 24u) >> 2u;
                                                float3 _7858 = as_type<float3>(uint3(history._m0[_7848], history._m0[_7848 + 1u], history._m0[_7848 + 2u]));
                                                uint _7864 = (_7826 + 40u) >> 2u;
                                                float3 _7874 = as_type<float3>(uint3(history._m0[_7864], history._m0[_7864 + 1u], history._m0[_7864 + 2u]));
                                                bool _8081;
                                                bool _8082;
                                                if (history._m0[(4u * (_7403 + _7822)) >> 2u] == _7372)
                                                {
                                                    uint4 _187 = _7839;
                                                    bool _8080;
                                                    do
                                                    {
                                                        uint _7887 = history._m0[_7841] & 7u;
                                                        uint _7888 = history._m0[_7841] >> 8u;
                                                        bool _7893;
                                                        if (_7887 <= 4u)
                                                        {
                                                            _7893 = _7888 < 1u;
                                                        }
                                                        else
                                                        {
                                                            _7893 = true;
                                                        }
                                                        bool _7898;
                                                        if (!_7893)
                                                        {
                                                            _7898 = _7888 > 3u;
                                                        }
                                                        else
                                                        {
                                                            _7898 = true;
                                                        }
                                                        bool _7905;
                                                        if (!_7898)
                                                        {
                                                            _7905 = history._m0[_7841] != ((_7888 << 8u) | _7887);
                                                        }
                                                        else
                                                        {
                                                            _7905 = true;
                                                        }
                                                        bool _7914;
                                                        if (!_7905)
                                                        {
                                                            bool _7913;
                                                            if (_7888 != 3u)
                                                            {
                                                                _7913 = _7887 == 4u;
                                                            }
                                                            else
                                                            {
                                                                _7913 = false;
                                                            }
                                                            _7914 = _7913;
                                                        }
                                                        else
                                                        {
                                                            _7914 = true;
                                                        }
                                                        bool _7923;
                                                        if (!_7914)
                                                        {
                                                            bool _7922;
                                                            if (_7888 == 3u)
                                                            {
                                                                _7922 = _7887 != 4u;
                                                            }
                                                            else
                                                            {
                                                                _7922 = false;
                                                            }
                                                            _7923 = _7922;
                                                        }
                                                        else
                                                        {
                                                            _7923 = true;
                                                        }
                                                        bool _7929;
                                                        if (!_7923)
                                                        {
                                                            _7929 = (history._m0[(_7826 + 36u) >> 2u] >> 24u) != 255u;
                                                        }
                                                        else
                                                        {
                                                            _7929 = true;
                                                        }
                                                        bool _7939;
                                                        if (!_7929)
                                                        {
                                                            bool3 _7933 = isnan(_7858);
                                                            bool3 _7934 = isinf(_7858);
                                                            _7939 = !all(not(bool3(_7933.x || _7934.x, _7933.y || _7934.y, _7933.z || _7934.z)));
                                                        }
                                                        else
                                                        {
                                                            _7939 = true;
                                                        }
                                                        bool _7949;
                                                        if (!_7939)
                                                        {
                                                            bool3 _7943 = isnan(_7874);
                                                            bool3 _7944 = isinf(_7874);
                                                            _7949 = !all(not(bool3(_7943.x || _7944.x, _7943.y || _7944.y, _7943.z || _7944.z)));
                                                        }
                                                        else
                                                        {
                                                            _7949 = true;
                                                        }
                                                        bool _7955;
                                                        if (!_7949)
                                                        {
                                                            _7955 = any(_7874 < float3(0.0));
                                                        }
                                                        else
                                                        {
                                                            _7955 = true;
                                                        }
                                                        bool _7961;
                                                        if (!_7955)
                                                        {
                                                            _7961 = any(_7874 > float3(1.0));
                                                        }
                                                        else
                                                        {
                                                            _7961 = true;
                                                        }
                                                        if (_7961)
                                                        {
                                                            _8080 = false;
                                                            break;
                                                        }
                                                        bool _8048;
                                                        bool _8049;
                                                        uint _7965 = 0u;
                                                        for (;;)
                                                        {
                                                            if (_7965 < 4u)
                                                            {
                                                                bool _8045;
                                                                if (_7965 < _7887)
                                                                {
                                                                    bool _8044;
                                                                    if (_187[_7965] == 4294967294u)
                                                                    {
                                                                        float _7991 = dot(Settings.previousNormal.xyz, Settings.previousNormal.xyz);
                                                                        bool _7997;
                                                                        if (Settings.scenePaths == 1u)
                                                                        {
                                                                            _7997 = Settings.previousPoint.w == 1.0;
                                                                        }
                                                                        else
                                                                        {
                                                                            _7997 = false;
                                                                        }
                                                                        bool _8002;
                                                                        if (_7997)
                                                                        {
                                                                            _8002 = Settings.previousNormal.w == 0.0;
                                                                        }
                                                                        else
                                                                        {
                                                                            _8002 = false;
                                                                        }
                                                                        bool _8010;
                                                                        if (_8002)
                                                                        {
                                                                            bool4 _8005 = isnan(Settings.previousPoint);
                                                                            bool4 _8006 = isinf(Settings.previousPoint);
                                                                            _8010 = all(not(bool4(_8005.x || _8006.x, _8005.y || _8006.y, _8005.z || _8006.z, _8005.w || _8006.w)));
                                                                        }
                                                                        else
                                                                        {
                                                                            _8010 = false;
                                                                        }
                                                                        bool _8018;
                                                                        if (_8010)
                                                                        {
                                                                            bool4 _8013 = isnan(Settings.previousNormal);
                                                                            bool4 _8014 = isinf(Settings.previousNormal);
                                                                            _8018 = all(not(bool4(_8013.x || _8014.x, _8013.y || _8014.y, _8013.z || _8014.z, _8013.w || _8014.w)));
                                                                        }
                                                                        else
                                                                        {
                                                                            _8018 = false;
                                                                        }
                                                                        bool _8025;
                                                                        if (_8018)
                                                                        {
                                                                            _8025 = all(abs(Settings.previousPoint.xyz) <= float3(999999995904.0));
                                                                        }
                                                                        else
                                                                        {
                                                                            _8025 = false;
                                                                        }
                                                                        bool _8031;
                                                                        if (_8025)
                                                                        {
                                                                            _8031 = all(abs(Settings.previousNormal.xyz) <= float3(999999995904.0));
                                                                        }
                                                                        else
                                                                        {
                                                                            _8031 = false;
                                                                        }
                                                                        bool _8038;
                                                                        if (_8031)
                                                                        {
                                                                            _8038 = !(isnan(_7991) || isinf(_7991));
                                                                        }
                                                                        else
                                                                        {
                                                                            _8038 = false;
                                                                        }
                                                                        bool _8042;
                                                                        if (_8038)
                                                                        {
                                                                            _8042 = _7991 > 9.9999996826552253889678874634872e-21;
                                                                        }
                                                                        else
                                                                        {
                                                                            _8042 = false;
                                                                        }
                                                                        _8044 = !_8042;
                                                                    }
                                                                    else
                                                                    {
                                                                        _8044 = _187[_7965] >= Settings.oldTriangles;
                                                                    }
                                                                    _8045 = _8044;
                                                                }
                                                                else
                                                                {
                                                                    _8045 = _187[_7965] != 4294967295u;
                                                                }
                                                                if (_8045)
                                                                {
                                                                    _8048 = false;
                                                                    _8049 = true;
                                                                    break;
                                                                }
                                                                _7965++;
                                                                continue;
                                                            }
                                                            else
                                                            {
                                                                _8048 = _7782;
                                                                _8049 = false;
                                                                break;
                                                            }
                                                        }
                                                        if (_8049)
                                                        {
                                                            _8080 = _8048;
                                                            break;
                                                        }
                                                        if (_7888 == 1u)
                                                        {
                                                            bool _8059;
                                                            if (history._m0[_7844] < Settings.oldTriangles)
                                                            {
                                                                _8059 = _7858.z == 0.0;
                                                            }
                                                            else
                                                            {
                                                                _8059 = false;
                                                            }
                                                            bool _8065;
                                                            if (_8059)
                                                            {
                                                                _8065 = all(_7858.xy >= float2(0.0));
                                                            }
                                                            else
                                                            {
                                                                _8065 = false;
                                                            }
                                                            bool _8071;
                                                            if (_8065)
                                                            {
                                                                _8071 = dot(_7858.xy, float2(1.0)) <= 1.0;
                                                            }
                                                            else
                                                            {
                                                                _8071 = false;
                                                            }
                                                            _8080 = _8071;
                                                            break;
                                                        }
                                                        bool _8079;
                                                        if (history._m0[_7844] == 4294967295u)
                                                        {
                                                            _8079 = abs(dot(_7858, _7858) - 1.0) < 9.9999997473787516355514526367188e-05;
                                                        }
                                                        else
                                                        {
                                                            _8079 = false;
                                                        }
                                                        _8080 = _8079;
                                                        break;
                                                    } while(false);
                                                    _8081 = _8080;
                                                    _8082 = _8080;
                                                }
                                                else
                                                {
                                                    _8081 = _7782;
                                                    _8082 = false;
                                                }
                                                bool _8095;
                                                if (_8082)
                                                {
                                                    bool _8089;
                                                    if (history._m0[_7841] == _833)
                                                    {
                                                        _8089 = history._m0[_7844] == _1066;
                                                    }
                                                    else
                                                    {
                                                        _8089 = false;
                                                    }
                                                    bool _8094;
                                                    if (_8089)
                                                    {
                                                        _8094 = all(_7839 == _184);
                                                    }
                                                    else
                                                    {
                                                        _8094 = false;
                                                    }
                                                    _8095 = _8094;
                                                }
                                                else
                                                {
                                                    _8095 = false;
                                                }
                                                float3 _8102;
                                                if (_8095)
                                                {
                                                    _8102 = precise::max(_7789, abs(_7858 - _7518) * 1.5);
                                                }
                                                else
                                                {
                                                    _8102 = _7789;
                                                }
                                                _7787 = _8081;
                                                _7790 = _8102;
                                            }
                                            uint _8103 = uint(_7779);
                                            _7777 = _7776 + (_7789 * abs(_189[_8103] - float(_190[_8103])));
                                        }
                                        if (any(abs(_7518 - _1067) > precise::min(_7776, float3(_1289 ? 0.04999999701976776123046875 : 0.0199999995529651641845703125))))
                                        {
                                            _7409 = _7781;
                                            _7415 = _7742;
                                            _7419 = false;
                                            _7413 = _7774;
                                            _7406 = true;
                                            break;
                                        }
                                        float3 _8130 = float3(float(history._m0[_7520] & 255u), float((history._m0[_7520] >> 8u) & 255u), float((history._m0[_7520] >> 16u) & 255u)) * float3(0.0039215688593685626983642578125);
                                        float3 _8165;
                                        if (_878)
                                        {
                                            float _8134 = _8130.x;
                                            float _8143;
                                            if (_8134 <= 0.040449999272823333740234375)
                                            {
                                                _8143 = _8134 * 0.077399380505084991455078125;
                                            }
                                            else
                                            {
                                                _8143 = powr((_8134 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                            }
                                            float _8144 = _8130.y;
                                            float _8153;
                                            if (_8144 <= 0.040449999272823333740234375)
                                            {
                                                _8153 = _8144 * 0.077399380505084991455078125;
                                            }
                                            else
                                            {
                                                _8153 = powr((_8144 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                            }
                                            float _8154 = _8130.z;
                                            float _8163;
                                            if (_8154 <= 0.040449999272823333740234375)
                                            {
                                                _8163 = _8154 * 0.077399380505084991455078125;
                                            }
                                            else
                                            {
                                                _8163 = powr((_8154 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                            }
                                            _8165 = float3(_8143, _8153, _8163);
                                        }
                                        else
                                        {
                                            _8165 = _8130;
                                        }
                                        _7425 = _7781;
                                        _7427 = _7411 + (_8165 * _7453);
                                        _7429 = _7774;
                                        _7431 = _7742;
                                        uint _7433 = _7432 + 1u;
                                        _7424 = _7425;
                                        _7411 = _7427;
                                        _7428 = _7429;
                                        _7430 = _7431;
                                        _7432 = _7433;
                                        continue;
                                    }
                                    else
                                    {
                                        _7409 = _7424;
                                        _7415 = _7430;
                                        _7419 = _7418;
                                        _7413 = _7428;
                                        _7406 = _7405;
                                        break;
                                    }
                                }
                                if (_7406)
                                {
                                    _8169 = _7409;
                                    _8170 = _7415;
                                    _8171 = _7411;
                                    _8172 = _7419;
                                    _8173 = _7413;
                                    _8174 = _7406;
                                    break;
                                }
                                _7405 = _7406;
                                _7408 = _7409;
                                _7410 = _7411;
                                _7412 = _7413;
                                _7414 = _7415;
                                _7416++;
                                _7418 = _7419;
                                continue;
                            }
                            else
                            {
                                _8169 = _7408;
                                _8170 = _7414;
                                _8171 = _7410;
                                _8172 = _7418;
                                _8173 = _7412;
                                _8174 = _7405;
                                break;
                            }
                        }
                        if (_8174)
                        {
                            _8197 = _8169;
                            _8198 = _8170;
                            _8199 = _8171;
                            _8200 = _8172;
                            break;
                        }
                        bool _8183;
                        if (!(isnan(_8173) || isinf(_8173)))
                        {
                            _8183 = _8173 <= 0.0;
                        }
                        else
                        {
                            _8183 = true;
                        }
                        bool _8194;
                        if (!_8183)
                        {
                            _8194 = abs((1.0 / _8173) - _7326.z) > precise::max(0.00999999977648258209228515625, _7326.z * 0.004999999888241291046142578125);
                        }
                        else
                        {
                            _8194 = true;
                        }
                        if (_8194)
                        {
                            _8197 = _8169;
                            _8198 = _8170;
                            _8199 = _8171;
                            _8200 = false;
                            break;
                        }
                        _8197 = _8169;
                        _8198 = _8170;
                        _8199 = _8171;
                        _8200 = true;
                        break;
                    } while(false);
                    bool _8863;
                    uint _8864;
                    float3 _8865;
                    if (_8200)
                    {
                        float3 _8204;
                        float3 _8207;
                        bool _8211;
                        _8204 = _913;
                        _8207 = _913;
                        _8211 = _805;
                        float3 _8205;
                        float3 _8208;
                        bool _8212;
                        for (int _8209 = -1; _8209 <= 1; _8204 = _8205, _8207 = _8208, _8209++, _8211 = _8212)
                        {
                            _8205 = _8204;
                            _8208 = _8207;
                            _8212 = _8211;
                            float3 _8217;
                            float3 _8219;
                            bool _8220;
                            for (int _8221 = -1; _8221 <= 1; _8205 = _8217, _8208 = _8219, _8212 = _8220, _8221++)
                            {
                                int2 _8235 = clamp(int2(gl_GlobalInvocationID.xy) + int2(_8221, _8209), int2(0), int2(int(Settings.width - 1u), int(Settings.height - 1u)));
                                uint _8241 = (uint(_8235.y) * Settings.width) + uint(_8235.x);
                                uint _8245 = _812 + (((_8241 * Settings.lobes) + _803) * 52u);
                                uint _8246 = _8245 >> 2u;
                                uint4 _8258 = uint4(current._m0[_8246], current._m0[_8246 + 1u], current._m0[_8246 + 2u], current._m0[_8246 + 3u]);
                                uint _8260 = (_8245 + 16u) >> 2u;
                                uint _8263 = _8260 + 1u;
                                uint _8267 = (_8245 + 24u) >> 2u;
                                float3 _8277 = as_type<float3>(uint3(current._m0[_8267], current._m0[_8267 + 1u], current._m0[_8267 + 2u]));
                                uint _8279 = (_8245 + 36u) >> 2u;
                                uint _8283 = (_8245 + 40u) >> 2u;
                                float3 _8293 = as_type<float3>(uint3(current._m0[_8283], current._m0[_8283 + 1u], current._m0[_8283 + 2u]));
                                uint _8296 = (4u * (_228 + _8241)) >> 2u;
                                float4 _8319;
                                if (_267)
                                {
                                    uint _8305 = ((_228 * 28u) + (_8241 * 16u)) >> 2u;
                                    _8319 = as_type<float4>(uint4(current._m0[_8305], current._m0[_8305 + 1u], current._m0[_8305 + 2u], current._m0[_8305 + 3u]));
                                }
                                else
                                {
                                    _8319 = float4(0.0, 0.0, 0.0, 1.0);
                                }
                                bool _8522;
                                if (current._m0[_8296] == _241)
                                {
                                    uint4 _182 = _8258;
                                    bool _8520;
                                    do
                                    {
                                        uint _8327 = current._m0[_8260] & 7u;
                                        uint _8328 = current._m0[_8260] >> 8u;
                                        bool _8333;
                                        if (_8327 <= 4u)
                                        {
                                            _8333 = _8328 < 1u;
                                        }
                                        else
                                        {
                                            _8333 = true;
                                        }
                                        bool _8338;
                                        if (!_8333)
                                        {
                                            _8338 = _8328 > 3u;
                                        }
                                        else
                                        {
                                            _8338 = true;
                                        }
                                        bool _8345;
                                        if (!_8338)
                                        {
                                            _8345 = current._m0[_8260] != ((_8328 << 8u) | _8327);
                                        }
                                        else
                                        {
                                            _8345 = true;
                                        }
                                        bool _8354;
                                        if (!_8345)
                                        {
                                            bool _8353;
                                            if (_8328 != 3u)
                                            {
                                                _8353 = _8327 == 4u;
                                            }
                                            else
                                            {
                                                _8353 = false;
                                            }
                                            _8354 = _8353;
                                        }
                                        else
                                        {
                                            _8354 = true;
                                        }
                                        bool _8363;
                                        if (!_8354)
                                        {
                                            bool _8362;
                                            if (_8328 == 3u)
                                            {
                                                _8362 = _8327 != 4u;
                                            }
                                            else
                                            {
                                                _8362 = false;
                                            }
                                            _8363 = _8362;
                                        }
                                        else
                                        {
                                            _8363 = true;
                                        }
                                        bool _8369;
                                        if (!_8363)
                                        {
                                            _8369 = (current._m0[_8279] >> 24u) != 255u;
                                        }
                                        else
                                        {
                                            _8369 = true;
                                        }
                                        bool _8379;
                                        if (!_8369)
                                        {
                                            bool3 _8373 = isnan(_8277);
                                            bool3 _8374 = isinf(_8277);
                                            _8379 = !all(not(bool3(_8373.x || _8374.x, _8373.y || _8374.y, _8373.z || _8374.z)));
                                        }
                                        else
                                        {
                                            _8379 = true;
                                        }
                                        bool _8389;
                                        if (!_8379)
                                        {
                                            bool3 _8383 = isnan(_8293);
                                            bool3 _8384 = isinf(_8293);
                                            _8389 = !all(not(bool3(_8383.x || _8384.x, _8383.y || _8384.y, _8383.z || _8384.z)));
                                        }
                                        else
                                        {
                                            _8389 = true;
                                        }
                                        bool _8395;
                                        if (!_8389)
                                        {
                                            _8395 = any(_8293 < float3(0.0));
                                        }
                                        else
                                        {
                                            _8395 = true;
                                        }
                                        bool _8401;
                                        if (!_8395)
                                        {
                                            _8401 = any(_8293 > float3(1.0));
                                        }
                                        else
                                        {
                                            _8401 = true;
                                        }
                                        if (_8401)
                                        {
                                            _8520 = false;
                                            break;
                                        }
                                        bool _8488;
                                        bool _8489;
                                        uint _8405 = 0u;
                                        for (;;)
                                        {
                                            if (_8405 < 4u)
                                            {
                                                bool _8485;
                                                if (_8405 < _8327)
                                                {
                                                    bool _8484;
                                                    if (_182[_8405] == 4294967294u)
                                                    {
                                                        float _8431 = dot(Settings.currentNormal.xyz, Settings.currentNormal.xyz);
                                                        bool _8437;
                                                        if (Settings.scenePaths == 1u)
                                                        {
                                                            _8437 = Settings.currentPoint.w == 1.0;
                                                        }
                                                        else
                                                        {
                                                            _8437 = false;
                                                        }
                                                        bool _8442;
                                                        if (_8437)
                                                        {
                                                            _8442 = Settings.currentNormal.w == 0.0;
                                                        }
                                                        else
                                                        {
                                                            _8442 = false;
                                                        }
                                                        bool _8450;
                                                        if (_8442)
                                                        {
                                                            bool4 _8445 = isnan(Settings.currentPoint);
                                                            bool4 _8446 = isinf(Settings.currentPoint);
                                                            _8450 = all(not(bool4(_8445.x || _8446.x, _8445.y || _8446.y, _8445.z || _8446.z, _8445.w || _8446.w)));
                                                        }
                                                        else
                                                        {
                                                            _8450 = false;
                                                        }
                                                        bool _8458;
                                                        if (_8450)
                                                        {
                                                            bool4 _8453 = isnan(Settings.currentNormal);
                                                            bool4 _8454 = isinf(Settings.currentNormal);
                                                            _8458 = all(not(bool4(_8453.x || _8454.x, _8453.y || _8454.y, _8453.z || _8454.z, _8453.w || _8454.w)));
                                                        }
                                                        else
                                                        {
                                                            _8458 = false;
                                                        }
                                                        bool _8465;
                                                        if (_8458)
                                                        {
                                                            _8465 = all(abs(Settings.currentPoint.xyz) <= float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _8465 = false;
                                                        }
                                                        bool _8471;
                                                        if (_8465)
                                                        {
                                                            _8471 = all(abs(Settings.currentNormal.xyz) <= float3(999999995904.0));
                                                        }
                                                        else
                                                        {
                                                            _8471 = false;
                                                        }
                                                        bool _8478;
                                                        if (_8471)
                                                        {
                                                            _8478 = !(isnan(_8431) || isinf(_8431));
                                                        }
                                                        else
                                                        {
                                                            _8478 = false;
                                                        }
                                                        bool _8482;
                                                        if (_8478)
                                                        {
                                                            _8482 = _8431 > 9.9999996826552253889678874634872e-21;
                                                        }
                                                        else
                                                        {
                                                            _8482 = false;
                                                        }
                                                        _8484 = !_8482;
                                                    }
                                                    else
                                                    {
                                                        _8484 = _182[_8405] >= Settings.triangles;
                                                    }
                                                    _8485 = _8484;
                                                }
                                                else
                                                {
                                                    _8485 = _182[_8405] != 4294967295u;
                                                }
                                                if (_8485)
                                                {
                                                    _8488 = false;
                                                    _8489 = true;
                                                    break;
                                                }
                                                _8405++;
                                                continue;
                                            }
                                            else
                                            {
                                                _8488 = _8212;
                                                _8489 = false;
                                                break;
                                            }
                                        }
                                        if (_8489)
                                        {
                                            _8520 = _8488;
                                            break;
                                        }
                                        if (_8328 == 1u)
                                        {
                                            bool _8499;
                                            if (current._m0[_8263] < Settings.triangles)
                                            {
                                                _8499 = _8277.z == 0.0;
                                            }
                                            else
                                            {
                                                _8499 = false;
                                            }
                                            bool _8505;
                                            if (_8499)
                                            {
                                                _8505 = all(_8277.xy >= float2(0.0));
                                            }
                                            else
                                            {
                                                _8505 = false;
                                            }
                                            bool _8511;
                                            if (_8505)
                                            {
                                                _8511 = dot(_8277.xy, float2(1.0)) <= 1.0;
                                            }
                                            else
                                            {
                                                _8511 = false;
                                            }
                                            _8520 = _8511;
                                            break;
                                        }
                                        bool _8519;
                                        if (current._m0[_8263] == 4294967295u)
                                        {
                                            _8519 = abs(dot(_8277, _8277) - 1.0) < 9.9999997473787516355514526367188e-05;
                                        }
                                        else
                                        {
                                            _8519 = false;
                                        }
                                        _8520 = _8519;
                                        break;
                                    } while(false);
                                    _8220 = _8520;
                                    _8522 = !_8520;
                                }
                                else
                                {
                                    _8220 = _8212;
                                    _8522 = true;
                                }
                                bool _8537;
                                if (!_8522)
                                {
                                    bool _8530;
                                    if (current._m0[_8260] == _833)
                                    {
                                        _8530 = current._m0[_8263] == _836;
                                    }
                                    else
                                    {
                                        _8530 = false;
                                    }
                                    bool _8535;
                                    if (_8530)
                                    {
                                        _8535 = all(_8258 == _829);
                                    }
                                    else
                                    {
                                        _8535 = false;
                                    }
                                    _8537 = !_8535;
                                }
                                else
                                {
                                    _8537 = true;
                                }
                                bool _8758;
                                if (!_8537)
                                {
                                    uint2 _8541 = uint2(_8235);
                                    uint _8542 = _8241 * 4u;
                                    uint _8543 = _8542 >> 2u;
                                    float _8550 = as_type<float>(current._m0[(_242 + _8542) >> 2u]);
                                    uint _8553 = (_248 + (_8241 * 16u)) >> 2u;
                                    float4 _8566 = as_type<float4>(uint4(current._m0[_8553], current._m0[_8553 + 1u], current._m0[_8553 + 2u], current._m0[_8553 + 3u]));
                                    float4 _8574 = ownership.read(uint2(int3(int(_8541.x), int(_8541.y), 0).xy), 0);
                                    bool _8635;
                                    if (current._m0[_8296] == 4294967294u)
                                    {
                                        float _8583 = dot(Settings.currentNormal.xyz, Settings.currentNormal.xyz);
                                        bool _8589;
                                        if (Settings.scenePaths == 1u)
                                        {
                                            _8589 = Settings.currentPoint.w == 1.0;
                                        }
                                        else
                                        {
                                            _8589 = false;
                                        }
                                        bool _8594;
                                        if (_8589)
                                        {
                                            _8594 = Settings.currentNormal.w == 0.0;
                                        }
                                        else
                                        {
                                            _8594 = false;
                                        }
                                        bool _8602;
                                        if (_8594)
                                        {
                                            bool4 _8597 = isnan(Settings.currentPoint);
                                            bool4 _8598 = isinf(Settings.currentPoint);
                                            _8602 = all(not(bool4(_8597.x || _8598.x, _8597.y || _8598.y, _8597.z || _8598.z, _8597.w || _8598.w)));
                                        }
                                        else
                                        {
                                            _8602 = false;
                                        }
                                        bool _8610;
                                        if (_8602)
                                        {
                                            bool4 _8605 = isnan(Settings.currentNormal);
                                            bool4 _8606 = isinf(Settings.currentNormal);
                                            _8610 = all(not(bool4(_8605.x || _8606.x, _8605.y || _8606.y, _8605.z || _8606.z, _8605.w || _8606.w)));
                                        }
                                        else
                                        {
                                            _8610 = false;
                                        }
                                        bool _8617;
                                        if (_8610)
                                        {
                                            _8617 = all(abs(Settings.currentPoint.xyz) <= float3(999999995904.0));
                                        }
                                        else
                                        {
                                            _8617 = false;
                                        }
                                        bool _8623;
                                        if (_8617)
                                        {
                                            _8623 = all(abs(Settings.currentNormal.xyz) <= float3(999999995904.0));
                                        }
                                        else
                                        {
                                            _8623 = false;
                                        }
                                        bool _8630;
                                        if (_8623)
                                        {
                                            _8630 = !(isnan(_8583) || isinf(_8583));
                                        }
                                        else
                                        {
                                            _8630 = false;
                                        }
                                        bool _8634;
                                        if (_8630)
                                        {
                                            _8634 = _8583 > 9.9999996826552253889678874634872e-21;
                                        }
                                        else
                                        {
                                            _8634 = false;
                                        }
                                        _8635 = _8634;
                                    }
                                    else
                                    {
                                        _8635 = false;
                                    }
                                    bool _8660;
                                    if (_267)
                                    {
                                        bool _8647;
                                        if (_8319.w == 1.0)
                                        {
                                            bool4 _8642 = isnan(_8319);
                                            bool4 _8643 = isinf(_8319);
                                            _8647 = all(not(bool4(_8642.x || _8643.x, _8642.y || _8643.y, _8642.z || _8643.z, _8642.w || _8643.w)));
                                        }
                                        else
                                        {
                                            _8647 = false;
                                        }
                                        bool _8653;
                                        if (_8647)
                                        {
                                            _8653 = all(_8319.xyz >= float3(0.0));
                                        }
                                        else
                                        {
                                            _8653 = false;
                                        }
                                        bool _8659;
                                        if (_8653)
                                        {
                                            _8659 = all(_8319.xyz <= float3(999999995904.0));
                                        }
                                        else
                                        {
                                            _8659 = false;
                                        }
                                        _8660 = _8659;
                                    }
                                    else
                                    {
                                        _8660 = true;
                                    }
                                    bool _8674;
                                    if (_8635)
                                    {
                                        _8674 = (current._m0[_8543] >> 24u) == 254u;
                                    }
                                    else
                                    {
                                        bool _8671;
                                        if (current._m0[_8296] < Settings.triangles)
                                        {
                                            _8671 = (current._m0[_8543] >> 24u) == 255u;
                                        }
                                        else
                                        {
                                            _8671 = false;
                                        }
                                        _8674 = _8671;
                                    }
                                    bool _8712;
                                    if (_8574.w > 0.5)
                                    {
                                        bool _8711;
                                        do
                                        {
                                            uint _8684 = uint(rint(_8574.z * 255.0));
                                            float _8685 = _8574.x;
                                            bool _8694;
                                            if ((isunordered(_8685, 0.0) || _8685 > 0.0))
                                            {
                                                bool _8693;
                                                if (_8684 != 1u)
                                                {
                                                    _8693 = _8684 != 2u;
                                                }
                                                else
                                                {
                                                    _8693 = false;
                                                }
                                                _8694 = _8693;
                                            }
                                            else
                                            {
                                                _8694 = true;
                                            }
                                            if (_8694)
                                            {
                                                _8711 = false;
                                                break;
                                            }
                                            uint _8697 = current._m0[_8543] >> 24u;
                                            if (_8697 == 253u)
                                            {
                                                _8711 = true;
                                                break;
                                            }
                                            if (_8685 == 0.0039215688593685626983642578125)
                                            {
                                                _8711 = _8697 == 254u;
                                                break;
                                            }
                                            bool _8710;
                                            if (_8697 == 255u)
                                            {
                                                _8710 = _8574.y > 0.0;
                                            }
                                            else
                                            {
                                                _8710 = false;
                                            }
                                            _8711 = _8710;
                                            break;
                                        } while(false);
                                        _8712 = _8711;
                                    }
                                    else
                                    {
                                        _8712 = false;
                                    }
                                    bool _8720;
                                    if (_8712 ? _8674 : false)
                                    {
                                        _8720 = !(isnan(_8550) || isinf(_8550));
                                    }
                                    else
                                    {
                                        _8720 = false;
                                    }
                                    bool _8724;
                                    if (_8720)
                                    {
                                        _8724 = _8550 > 0.0;
                                    }
                                    else
                                    {
                                        _8724 = false;
                                    }
                                    bool _8729;
                                    if (_8724)
                                    {
                                        _8729 = _8566.w == 1.0;
                                    }
                                    else
                                    {
                                        _8729 = false;
                                    }
                                    bool _8738;
                                    if (_8729 ? _8660 : false)
                                    {
                                        bool4 _8733 = isnan(_8566);
                                        bool4 _8734 = isinf(_8566);
                                        _8738 = all(not(bool4(_8733.x || _8734.x, _8733.y || _8734.y, _8733.z || _8734.z, _8733.w || _8734.w)));
                                    }
                                    else
                                    {
                                        _8738 = false;
                                    }
                                    bool _8744;
                                    if (_8738)
                                    {
                                        _8744 = all(_8566.xyz >= float3(0.0));
                                    }
                                    else
                                    {
                                        _8744 = false;
                                    }
                                    bool _8750;
                                    if (_8744)
                                    {
                                        _8750 = all(_8566.xyz <= float3(1.0));
                                    }
                                    else
                                    {
                                        _8750 = false;
                                    }
                                    bool _8756;
                                    if (_8750)
                                    {
                                        _8756 = any(_8566.xyz > float3(0.0));
                                    }
                                    else
                                    {
                                        _8756 = false;
                                    }
                                    _8758 = !_8756;
                                }
                                else
                                {
                                    _8758 = true;
                                }
                                if (_8758)
                                {
                                    _8217 = _8205;
                                    _8219 = _8208;
                                    continue;
                                }
                                float3 _8770 = float3(float(current._m0[_8279] & 255u), float((current._m0[_8279] >> 8u) & 255u), float((current._m0[_8279] >> 16u) & 255u)) * float3(0.0039215688593685626983642578125);
                                float3 _8805;
                                if (_878)
                                {
                                    float _8774 = _8770.x;
                                    float _8783;
                                    if (_8774 <= 0.040449999272823333740234375)
                                    {
                                        _8783 = _8774 * 0.077399380505084991455078125;
                                    }
                                    else
                                    {
                                        _8783 = powr((_8774 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                    }
                                    float _8784 = _8770.y;
                                    float _8793;
                                    if (_8784 <= 0.040449999272823333740234375)
                                    {
                                        _8793 = _8784 * 0.077399380505084991455078125;
                                    }
                                    else
                                    {
                                        _8793 = powr((_8784 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                    }
                                    float _8794 = _8770.z;
                                    float _8803;
                                    if (_8794 <= 0.040449999272823333740234375)
                                    {
                                        _8803 = _8794 * 0.077399380505084991455078125;
                                    }
                                    else
                                    {
                                        _8803 = powr((_8794 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                    }
                                    _8805 = float3(_8783, _8793, _8803);
                                }
                                else
                                {
                                    _8805 = _8770;
                                }
                                _8217 = precise::max(_8205, _8805);
                                _8219 = precise::min(_8208, _8805);
                            }
                        }
                        float3 _8812 = mix(_913, fast::clamp(_8199, _8207, _8204), float3(Settings.weight));
                        float3 _8850;
                        if (_878)
                        {
                            float3 _8818 = fast::clamp(_8812, float3(0.0), float3(1.0));
                            float _8819 = _8818.x;
                            float _8828;
                            if (_8819 <= 0.003130800090730190277099609375)
                            {
                                _8828 = _8819 * 12.9200000762939453125;
                            }
                            else
                            {
                                _8828 = (1.05499994754791259765625 * powr(_8819, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                            }
                            float _8829 = _8818.y;
                            float _8838;
                            if (_8829 <= 0.003130800090730190277099609375)
                            {
                                _8838 = _8829 * 12.9200000762939453125;
                            }
                            else
                            {
                                _8838 = (1.05499994754791259765625 * powr(_8829, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                            }
                            float _8839 = _8818.z;
                            float _8848;
                            if (_8839 <= 0.003130800090730190277099609375)
                            {
                                _8848 = _8839 * 12.9200000762939453125;
                            }
                            else
                            {
                                _8848 = (1.05499994754791259765625 * powr(_8839, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                            }
                            _8850 = float3(_8828, _8838, _8848);
                        }
                        else
                        {
                            _8850 = fast::clamp(_8812, float3(0.0), float3(1.0));
                        }
                        uint3 _8854 = uint3(floor((_8850 * 255.0) + float3(0.5)));
                        _8863 = _8211;
                        _8864 = ((_8854.x | (_8854.y << 8u)) | (_8854.z << 16u)) | (current._m0[_850] & 4278190080u);
                        _8865 = _8812;
                    }
                    else
                    {
                        _8863 = _805;
                        _8864 = current._m0[_850];
                        _8865 = _913;
                    }
                    _8867 = _8200;
                    _8868 = _8197;
                    _8869 = _8198;
                    _8870 = _7326;
                    _8871 = _7318;
                    _8872 = _7319;
                    _8873 = _7320;
                    _8874 = _7321;
                    _8875 = _7322;
                    _8876 = _7323;
                    _8877 = _7324;
                    _8878 = _7325;
                    _8879 = _8200 ? true : _801;
                    _8880 = _8863;
                    _8881 = _8864;
                    _8882 = _8865;
                }
                else
                {
                    _8867 = _772;
                    _8868 = _775;
                    _8869 = _777;
                    _8870 = _779;
                    _8871 = _781;
                    _8872 = _783;
                    _8873 = _785;
                    _8874 = _787;
                    _8875 = _789;
                    _8876 = _791;
                    _8877 = _793;
                    _8878 = _795;
                    _8879 = _801;
                    _8880 = _805;
                    _8881 = current._m0[_850];
                    _8882 = _913;
                }
                _773 = _8867;
                _776 = _8868;
                _778 = _8869;
                _780 = _8870;
                _782 = _8871;
                _784 = _8872;
                _786 = _8873;
                _788 = _8874;
                _790 = _8875;
                _792 = _8876;
                _794 = _8877;
                _796 = _8878;
                _798 = _1068;
                _802 = _8879;
                _806 = _8880;
                _8883 = _8881;
                _8884 = _8882;
            }
            else
            {
                _773 = _772;
                _776 = _775;
                _778 = _777;
                _780 = _779;
                _782 = _781;
                _784 = _783;
                _786 = _785;
                _788 = _787;
                _790 = _789;
                _792 = _791;
                _794 = _793;
                _796 = _795;
                _798 = _797;
                _802 = _801;
                _806 = _805;
                _8883 = current._m0[_850];
                _8884 = _913;
            }
            float3 _8885 = _8884 * _864;
            float3 _8922;
            if (_878)
            {
                float3 _8890 = fast::clamp(_8885, float3(0.0), float3(1.0));
                float _8891 = _8890.x;
                float _8900;
                if (_8891 <= 0.003130800090730190277099609375)
                {
                    _8900 = _8891 * 12.9200000762939453125;
                }
                else
                {
                    _8900 = (1.05499994754791259765625 * powr(_8891, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                float _8901 = _8890.y;
                float _8910;
                if (_8901 <= 0.003130800090730190277099609375)
                {
                    _8910 = _8901 * 12.9200000762939453125;
                }
                else
                {
                    _8910 = (1.05499994754791259765625 * powr(_8901, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                float _8911 = _8890.z;
                float _8920;
                if (_8911 <= 0.003130800090730190277099609375)
                {
                    _8920 = _8911 * 12.9200000762939453125;
                }
                else
                {
                    _8920 = (1.05499994754791259765625 * powr(_8911, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                _8922 = float3(_8900, _8910, _8920);
            }
            else
            {
                _8922 = fast::clamp(_8885, float3(0.0), float3(1.0));
            }
            uint3 _8926 = uint3(floor((_8922 * 255.0) + float3(0.5)));
            uint _8933 = (_8926.x | (_8926.y << 8u)) | (_8926.z << 16u);
            uint _8934 = _8933 | 4278190080u;
            float3 _8944 = float3(float(_8933 & 255u), float((_8934 >> 8u) & 255u), float((_8934 >> 16u) & 255u)) * float3(0.0039215688593685626983642578125);
            float3 _8979;
            if (_878)
            {
                float _8948 = _8944.x;
                float _8957;
                if (_8948 <= 0.040449999272823333740234375)
                {
                    _8957 = _8948 * 0.077399380505084991455078125;
                }
                else
                {
                    _8957 = powr((_8948 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                float _8958 = _8944.y;
                float _8967;
                if (_8958 <= 0.040449999272823333740234375)
                {
                    _8967 = _8958 * 0.077399380505084991455078125;
                }
                else
                {
                    _8967 = powr((_8958 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                float _8968 = _8944.z;
                float _8977;
                if (_8968 <= 0.040449999272823333740234375)
                {
                    _8977 = _8968 * 0.077399380505084991455078125;
                }
                else
                {
                    _8977 = powr((_8968 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                _8979 = float3(_8957, _8967, _8977);
            }
            else
            {
                _8979 = _8944;
            }
            _800 = _799 + _8979;
            bool _8980 = !_482;
            uint4 _8982 = select(_829, uint4(4294967295u), bool4(_8980));
            bool3 _8985 = bool3(_8980);
            resolved._m0[_817] = _8982.x;
            resolved._m0[_820] = _8982.y;
            resolved._m0[_823] = _8982.z;
            resolved._m0[_826] = _8982.w;
            resolved._m0[_831] = _8980 ? 0u : _833;
            resolved._m0[_834] = _8980 ? 4294967295u : _836;
            uint3 _8999 = as_type<uint3>(select(_848, float3(0.0), _8985));
            resolved._m0[_838] = _8999.x;
            resolved._m0[_841] = _8999.y;
            resolved._m0[_844] = _8999.z;
            resolved._m0[_850] = _8980 ? 0u : _8883;
            uint3 _9007 = as_type<uint3>(select(_864, float3(0.0), _8985));
            resolved._m0[_854] = _9007.x;
            resolved._m0[_857] = _9007.y;
            resolved._m0[_860] = _9007.z;
        }
        if (_801)
        {
            float3 _9022 = _287.xyz + ((_799 / float3(float(Settings.lobes))) * _264.xyz);
            float3 _9064;
            if ((Settings.flags & 2u) != 0u)
            {
                float3 _9032 = fast::clamp(_9022, float3(0.0), float3(1.0));
                float _9033 = _9032.x;
                float _9042;
                if (_9033 <= 0.003130800090730190277099609375)
                {
                    _9042 = _9033 * 12.9200000762939453125;
                }
                else
                {
                    _9042 = (1.05499994754791259765625 * powr(_9033, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                float _9043 = _9032.y;
                float _9052;
                if (_9043 <= 0.003130800090730190277099609375)
                {
                    _9052 = _9043 * 12.9200000762939453125;
                }
                else
                {
                    _9052 = (1.05499994754791259765625 * powr(_9043, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                float _9053 = _9032.z;
                float _9062;
                if (_9053 <= 0.003130800090730190277099609375)
                {
                    _9062 = _9053 * 12.9200000762939453125;
                }
                else
                {
                    _9062 = (1.05499994754791259765625 * powr(_9053, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                _9064 = float3(_9042, _9052, _9062);
            }
            else
            {
                _9064 = fast::clamp(_9022, float3(0.0), float3(1.0));
            }
            uint3 _9068 = uint3(floor((_9064 * 255.0) + float3(0.5)));
            resolved._m0[_234] = ((_9068.x | (_9068.y << 8u)) | (_9068.z << 16u)) | (_236 & 4278190080u);
        }
        break;
    } while(false);
}

