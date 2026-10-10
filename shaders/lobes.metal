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
};

constant float2 _145 = {};
constant float4 _146 = {};
constant bool _147 = {};

constant spvUnsafeArray<float2, 8> _148 = spvUnsafeArray<float2, 8>({ float2(0.5, 0.0), float2(-0.5, 0.0), float2(0.0, 0.5), float2(0.0, -0.5), float2(0.611999988555908203125), float2(-0.611999988555908203125, 0.611999988555908203125), float2(0.611999988555908203125, -0.611999988555908203125), float2(-0.611999988555908203125) });

kernel void reflection_lobes_main(device type_ByteAddressBuffer& current [[buffer(0)]], device type_ByteAddressBuffer& history [[buffer(1)]], device type_ByteAddressBuffer& geometry [[buffer(2)]], device type_RWByteAddressBuffer& resolved [[buffer(3)]], constant type_Settings& Settings [[buffer(4)]], texture2d<float> ownership [[texture(0)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        bool _182;
        if (gl_GlobalInvocationID.x < Settings.width)
        {
            _182 = gl_GlobalInvocationID.y >= Settings.height;
        }
        else
        {
            _182 = true;
        }
        if (_182)
        {
            break;
        }
        uint _187 = Settings.width * Settings.height;
        uint _191 = (gl_GlobalInvocationID.y * Settings.width) + gl_GlobalInvocationID.x;
        uint _192 = _191 * 4u;
        uint _193 = _192 >> 2u;
        uint _195 = current._m0[_193];
        uint _198 = (4u * (_187 + _191)) >> 2u;
        uint _200 = current._m0[_198];
        uint _201 = _187 * 8u;
        uint _203 = (_201 + _192) >> 2u;
        uint _205 = current._m0[_203];
        float _206 = as_type<float>(_205);
        uint _207 = _187 * 12u;
        uint _210 = (_207 + (_191 * 16u)) >> 2u;
        uint _212 = current._m0[_210];
        uint _213 = _210 + 1u;
        uint _215 = current._m0[_213];
        uint _216 = _210 + 2u;
        uint _218 = current._m0[_216];
        uint _219 = _210 + 3u;
        uint _221 = current._m0[_219];
        float4 _223 = as_type<float4>(uint4(_212, _215, _218, _221));
        float4 _231 = ownership.read(uint2(int3(int(gl_GlobalInvocationID.x), int(gl_GlobalInvocationID.y), 0).xy), 0);
        bool _269;
        if (_231.w > 0.5)
        {
            bool _268;
            do
            {
                uint _241 = uint(rint(_231.z * 255.0));
                float _242 = _231.x;
                bool _251;
                if ((isunordered(_242, 0.0) || _242 > 0.0))
                {
                    bool _250;
                    if (_241 != 1u)
                    {
                        _250 = _241 != 2u;
                    }
                    else
                    {
                        _250 = false;
                    }
                    _251 = _250;
                }
                else
                {
                    _251 = true;
                }
                if (_251)
                {
                    _268 = false;
                    break;
                }
                uint _254 = _195 >> 24u;
                if (_254 == 253u)
                {
                    _268 = true;
                    break;
                }
                if (_242 == 0.0039215688593685626983642578125)
                {
                    _268 = _254 == 254u;
                    break;
                }
                bool _267;
                if (_254 == 255u)
                {
                    _267 = _231.y > 0.0;
                }
                else
                {
                    _267 = false;
                }
                _268 = _267;
                break;
            } while(false);
            _269 = _268;
        }
        else
        {
            _269 = false;
        }
        bool _274;
        if (_269)
        {
            _274 = (_195 >> 24u) == 255u;
        }
        else
        {
            _274 = false;
        }
        bool _280;
        if (_274)
        {
            _280 = _200 < Settings.triangles;
        }
        else
        {
            _280 = false;
        }
        bool _287;
        if (_280)
        {
            _287 = !(isnan(_206) || isinf(_206));
        }
        else
        {
            _287 = false;
        }
        bool _291;
        if (_287)
        {
            _291 = _206 > 0.0;
        }
        else
        {
            _291 = false;
        }
        bool _296;
        if (_291)
        {
            _296 = _223.w == 1.0;
        }
        else
        {
            _296 = false;
        }
        bool _304;
        if (_296)
        {
            bool4 _299 = isnan(_223);
            bool4 _300 = isinf(_223);
            _304 = all(not(bool4(_299.x || _300.x, _299.y || _300.y, _299.z || _300.z, _299.w || _300.w)));
        }
        else
        {
            _304 = false;
        }
        bool _310;
        if (_304)
        {
            _310 = all(_223.xyz >= float3(0.0));
        }
        else
        {
            _310 = false;
        }
        bool _316;
        if (_310)
        {
            _316 = all(_223.xyz <= float3(1.0));
        }
        else
        {
            _316 = false;
        }
        bool _322;
        if (_316)
        {
            _322 = any(_223.xyz > float3(0.0));
        }
        else
        {
            _322 = false;
        }
        resolved._m0[_193] = _195;
        resolved._m0[_198] = _322 ? _200 : 4294967295u;
        resolved._m0[_203] = _205;
        resolved._m0[_210] = _212;
        resolved._m0[_213] = _215;
        resolved._m0[_216] = _218;
        resolved._m0[_219] = _221;
        float4 _335;
        float2 _337;
        float3 _339;
        bool _341;
        _335 = _146;
        _337 = _145;
        _339 = float3(0.0);
        _341 = false;
        float3 _340;
        bool _333;
        float4 _336;
        float2 _338;
        bool _342;
        bool _332;
        for (uint _343 = 0u; _343 < Settings.lobes; _332 = _333, _335 = _336, _337 = _338, _339 = _340, _341 = _342, _343++)
        {
            uint _350 = _187 * 28u;
            uint _355 = (_350 + (((_191 * Settings.lobes) + _343) * 12u)) >> 2u;
            uint _358 = _355 + 1u;
            uint _361 = _355 + 2u;
            uint3 _364 = uint3(current._m0[_355], current._m0[_358], current._m0[_361]);
            float3 _374 = float3(float(current._m0[_361] & 255u), float((current._m0[_361] >> 8u) & 255u), float((current._m0[_361] >> 16u) & 255u)) * float3(0.0039215688593685626983642578125);
            bool _378 = (Settings.flags & 2u) != 0u;
            float3 _413;
            if (_378)
            {
                float _382 = _374.x;
                float _391;
                if (_382 <= 0.040449999272823333740234375)
                {
                    _391 = _382 * 0.077399380505084991455078125;
                }
                else
                {
                    _391 = powr((_382 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                float _392 = _374.y;
                float _401;
                if (_392 <= 0.040449999272823333740234375)
                {
                    _401 = _392 * 0.077399380505084991455078125;
                }
                else
                {
                    _401 = powr((_392 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                float _402 = _374.z;
                float _411;
                if (_402 <= 0.040449999272823333740234375)
                {
                    _411 = _402 * 0.077399380505084991455078125;
                }
                else
                {
                    _411 = powr((_402 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                }
                _413 = float3(_391, _401, _411);
            }
            else
            {
                _413 = _374;
            }
            bool _419;
            if (_322)
            {
                _419 = current._m0[_355] < Settings.triangles;
            }
            else
            {
                _419 = false;
            }
            bool _435;
            if (_419)
            {
                bool _427;
                if (current._m0[_355] != 4294967295u)
                {
                    _427 = (current._m0[_361] >> 24u) == 255u;
                }
                else
                {
                    _427 = false;
                }
                bool _434;
                if (_427)
                {
                    _434 = ((current._m0[_358] & 65535u) + (current._m0[_358] >> 16u)) <= 65535u;
                }
                else
                {
                    _434 = false;
                }
                _435 = _434;
            }
            else
            {
                _435 = false;
            }
            _364.x = _435 ? current._m0[_355] : 4294967295u;
            bool _442;
            if (_435)
            {
                _442 = (Settings.flags & 1u) != 0u;
            }
            else
            {
                _442 = false;
            }
            bool _448;
            if (_442)
            {
                _448 = Settings.weight > 0.0;
            }
            else
            {
                _448 = false;
            }
            uint3 _3296;
            float3 _3297;
            if (_448)
            {
                uint _454 = Settings.previousVertices + (_200 * 48u);
                uint _456 = Settings.previousVertices + (current._m0[_355] * 48u);
                uint _457 = _454 >> 2u;
                float4 _470 = as_type<float4>(uint4(geometry._m0[_457], geometry._m0[_457 + 1u], geometry._m0[_457 + 2u], geometry._m0[_457 + 3u]));
                uint _472 = (_454 + 16u) >> 2u;
                float4 _485 = as_type<float4>(uint4(geometry._m0[_472], geometry._m0[_472 + 1u], geometry._m0[_472 + 2u], geometry._m0[_472 + 3u]));
                uint _487 = (_454 + 32u) >> 2u;
                float4 _500 = as_type<float4>(uint4(geometry._m0[_487], geometry._m0[_487 + 1u], geometry._m0[_487 + 2u], geometry._m0[_487 + 3u]));
                float _505 = float(Settings.previousWidth);
                float _508 = float(Settings.previousHeight);
                float4 _513 = float4(_505, _508, Settings.clip.xy);
                float _516 = float(_343);
                float4 _517 = float4(Settings.roughness, _516, 0.0, 0.0);
                float _518 = _470.w;
                bool _524;
                if (_518 == 1.0)
                {
                    _524 = _485.w == 1.0;
                }
                else
                {
                    _524 = false;
                }
                bool _529;
                if (_524)
                {
                    _529 = _500.w == 1.0;
                }
                else
                {
                    _529 = false;
                }
                float4 _2592;
                float2 _2593;
                float4 _2594;
                if (_529)
                {
                    uint _532 = _456 >> 2u;
                    float4 _545 = as_type<float4>(uint4(geometry._m0[_532], geometry._m0[_532 + 1u], geometry._m0[_532 + 2u], geometry._m0[_532 + 3u]));
                    uint _547 = (_456 + 16u) >> 2u;
                    float4 _560 = as_type<float4>(uint4(geometry._m0[_547], geometry._m0[_547 + 1u], geometry._m0[_547 + 2u], geometry._m0[_547 + 3u]));
                    uint _562 = (_456 + 32u) >> 2u;
                    float4 _575 = as_type<float4>(uint4(geometry._m0[_562], geometry._m0[_562 + 1u], geometry._m0[_562 + 2u], geometry._m0[_562 + 3u]));
                    float2 _581 = float2(float(current._m0[_358] & 65535u), float(current._m0[_358] >> 16u)) * float2(1.525902189314365386962890625e-05);
                    float2 _2590;
                    float4 _2591;
                    do
                    {
                        bool _586;
                        bool _811;
                        do
                        {
                            _586 = _518 == 2.0;
                            bool _591;
                            if (_586)
                            {
                                _591 = _485.w == 2.0;
                            }
                            else
                            {
                                _591 = false;
                            }
                            bool _596;
                            if (_591)
                            {
                                _596 = _500.w == 2.0;
                            }
                            else
                            {
                                _596 = false;
                            }
                            bool _612;
                            if (!_596)
                            {
                                bool _605;
                                if ((isunordered(_518, 1.0) || _518 == 1.0))
                                {
                                    _605 = _485.w != 1.0;
                                }
                                else
                                {
                                    _605 = true;
                                }
                                bool _611;
                                if (!_605)
                                {
                                    _611 = _500.w != 1.0;
                                }
                                else
                                {
                                    _611 = true;
                                }
                                _612 = _611;
                            }
                            else
                            {
                                _612 = false;
                            }
                            bool _622;
                            if (!_612)
                            {
                                bool4 _616 = isnan(_470);
                                bool4 _617 = isinf(_470);
                                _622 = !all(not(bool4(_616.x || _617.x, _616.y || _617.y, _616.z || _617.z, _616.w || _617.w)));
                            }
                            else
                            {
                                _622 = true;
                            }
                            bool _632;
                            if (!_622)
                            {
                                bool4 _626 = isnan(_485);
                                bool4 _627 = isinf(_485);
                                _632 = !all(not(bool4(_626.x || _627.x, _626.y || _627.y, _626.z || _627.z, _626.w || _627.w)));
                            }
                            else
                            {
                                _632 = true;
                            }
                            bool _642;
                            if (!_632)
                            {
                                bool4 _636 = isnan(_500);
                                bool4 _637 = isinf(_500);
                                _642 = !all(not(bool4(_636.x || _637.x, _636.y || _637.y, _636.z || _637.z, _636.w || _637.w)));
                            }
                            else
                            {
                                _642 = true;
                            }
                            bool _652;
                            if (!_642)
                            {
                                bool4 _646 = isnan(Settings.projection);
                                bool4 _647 = isinf(Settings.projection);
                                _652 = !all(not(bool4(_646.x || _647.x, _646.y || _647.y, _646.z || _647.z, _646.w || _647.w)));
                            }
                            else
                            {
                                _652 = true;
                            }
                            bool _662;
                            if (!_652)
                            {
                                bool4 _656 = isnan(_513);
                                bool4 _657 = isinf(_513);
                                _662 = !all(not(bool4(_656.x || _657.x, _656.y || _657.y, _656.z || _657.z, _656.w || _657.w)));
                            }
                            else
                            {
                                _662 = true;
                            }
                            bool _672;
                            if (!_662)
                            {
                                bool4 _666 = isnan(_517);
                                bool4 _667 = isinf(_517);
                                _672 = !all(not(bool4(_666.x || _667.x, _666.y || _667.y, _666.z || _667.z, _666.w || _667.w)));
                            }
                            else
                            {
                                _672 = true;
                            }
                            bool _680;
                            if (!_672)
                            {
                                _680 = any(abs(_470.xyz) > float3(999999995904.0));
                            }
                            else
                            {
                                _680 = true;
                            }
                            bool _688;
                            if (!_680)
                            {
                                _688 = any(abs(_485.xyz) > float3(999999995904.0));
                            }
                            else
                            {
                                _688 = true;
                            }
                            bool _696;
                            if (!_688)
                            {
                                _696 = any(abs(_500.xyz) > float3(999999995904.0));
                            }
                            else
                            {
                                _696 = true;
                            }
                            bool _703;
                            if (!_696)
                            {
                                _703 = any(Settings.projection.xy <= float2(0.0));
                            }
                            else
                            {
                                _703 = true;
                            }
                            bool _710;
                            if (!_703)
                            {
                                _710 = any(abs(Settings.projection) > float4(999999995904.0));
                            }
                            else
                            {
                                _710 = true;
                            }
                            bool _717;
                            if (!_710)
                            {
                                _717 = any(_513.xy < float2(1.0));
                            }
                            else
                            {
                                _717 = true;
                            }
                            bool _724;
                            if (!_717)
                            {
                                _724 = any(_513.xy > float2(16384.0));
                            }
                            else
                            {
                                _724 = true;
                            }
                            bool _729;
                            if (!_724)
                            {
                                _729 = Settings.clip.x <= 0.0;
                            }
                            else
                            {
                                _729 = true;
                            }
                            bool _734;
                            if (!_729)
                            {
                                _734 = Settings.clip.y <= Settings.clip.x;
                            }
                            else
                            {
                                _734 = true;
                            }
                            bool _739;
                            if (!_734)
                            {
                                _739 = Settings.clip.y > 999999995904.0;
                            }
                            else
                            {
                                _739 = true;
                            }
                            bool _744;
                            if (!_739)
                            {
                                _744 = Settings.roughness < 0.0;
                            }
                            else
                            {
                                _744 = true;
                            }
                            bool _749;
                            if (!_744)
                            {
                                _749 = Settings.roughness > 1.0;
                            }
                            else
                            {
                                _749 = true;
                            }
                            bool _754;
                            if (!_749)
                            {
                                _754 = _516 < 0.0;
                            }
                            else
                            {
                                _754 = true;
                            }
                            bool _759;
                            if (!_754)
                            {
                                _759 = _516 > 7.0;
                            }
                            else
                            {
                                _759 = true;
                            }
                            bool _765;
                            if (!_759)
                            {
                                _765 = floor(_516) != _516;
                            }
                            else
                            {
                                _765 = true;
                            }
                            bool _772;
                            if (!_765)
                            {
                                _772 = any(_517.zw != float2(0.0));
                            }
                            else
                            {
                                _772 = true;
                            }
                            if (_772)
                            {
                                _811 = false;
                                break;
                            }
                            bool _780;
                            if (_596)
                            {
                                _780 = any(_500.xyz != float3(0.0));
                            }
                            else
                            {
                                _780 = false;
                            }
                            if (_780)
                            {
                                _811 = false;
                                break;
                            }
                            float3 _793;
                            if (_596)
                            {
                                _793 = _485.xyz;
                            }
                            else
                            {
                                float3 _788 = _470.xyz;
                                _793 = cross(_485.xyz - _788, _500.xyz - _788);
                            }
                            float _794 = dot(_793, _793);
                            bool3 _795 = isnan(_793);
                            bool3 _796 = isinf(_793);
                            bool _806;
                            if (all(not(bool3(_795.x || _796.x, _795.y || _796.y, _795.z || _796.z))))
                            {
                                _806 = !(isnan(_794) || isinf(_794));
                            }
                            else
                            {
                                _806 = false;
                            }
                            bool _810;
                            if (_806)
                            {
                                _810 = _794 > 9.9999996826552253889678874634872e-21;
                            }
                            else
                            {
                                _810 = false;
                            }
                            _811 = _810;
                            break;
                        } while(false);
                        bool _816;
                        if (_811)
                        {
                            _816 = _545.w != 1.0;
                        }
                        else
                        {
                            _816 = true;
                        }
                        bool _822;
                        if (!_816)
                        {
                            _822 = _560.w != 1.0;
                        }
                        else
                        {
                            _822 = true;
                        }
                        bool _828;
                        if (!_822)
                        {
                            _828 = _575.w != 1.0;
                        }
                        else
                        {
                            _828 = true;
                        }
                        bool _838;
                        if (!_828)
                        {
                            bool4 _832 = isnan(_545);
                            bool4 _833 = isinf(_545);
                            _838 = !all(not(bool4(_832.x || _833.x, _832.y || _833.y, _832.z || _833.z, _832.w || _833.w)));
                        }
                        else
                        {
                            _838 = true;
                        }
                        bool _848;
                        if (!_838)
                        {
                            bool4 _842 = isnan(_560);
                            bool4 _843 = isinf(_560);
                            _848 = !all(not(bool4(_842.x || _843.x, _842.y || _843.y, _842.z || _843.z, _842.w || _843.w)));
                        }
                        else
                        {
                            _848 = true;
                        }
                        bool _858;
                        if (!_848)
                        {
                            bool4 _852 = isnan(_575);
                            bool4 _853 = isinf(_575);
                            _858 = !all(not(bool4(_852.x || _853.x, _852.y || _853.y, _852.z || _853.z, _852.w || _853.w)));
                        }
                        else
                        {
                            _858 = true;
                        }
                        bool _868;
                        if (!_858)
                        {
                            bool2 _862 = isnan(_581);
                            bool2 _863 = isinf(_581);
                            _868 = !all(not(bool2(_862.x || _863.x, _862.y || _863.y)));
                        }
                        else
                        {
                            _868 = true;
                        }
                        bool _874;
                        if (!_868)
                        {
                            _874 = any(_581 < float2(0.0));
                        }
                        else
                        {
                            _874 = true;
                        }
                        bool _882;
                        if (!_874)
                        {
                            _882 = (_581.x + _581.y) > 1.000010013580322265625;
                        }
                        else
                        {
                            _882 = true;
                        }
                        if (_882)
                        {
                            _2590 = _337;
                            _2591 = float4(0.0);
                            break;
                        }
                        float3 _885 = _560.xyz;
                        float3 _886 = _545.xyz;
                        float3 _888 = _575.xyz;
                        float3 _890 = cross(_885 - _886, _888 - _886);
                        float _891 = dot(_890, _890);
                        bool3 _892 = isnan(_890);
                        bool3 _893 = isinf(_890);
                        bool _902;
                        if (all(not(bool3(_892.x || _893.x, _892.y || _893.y, _892.z || _893.z))))
                        {
                            _902 = isnan(_891) || isinf(_891);
                        }
                        else
                        {
                            _902 = true;
                        }
                        bool _907;
                        if (!_902)
                        {
                            _907 = _891 <= 9.9999996826552253889678874634872e-21;
                        }
                        else
                        {
                            _907 = true;
                        }
                        if (_907)
                        {
                            _2590 = _337;
                            _2591 = float4(0.0);
                            break;
                        }
                        float _910 = _581.x;
                        float _912 = _581.y;
                        float3 _918 = ((_886 * ((1.0 - _910) - _912)) + (_885 * _910)) + (_888 * _912);
                        bool _923;
                        if (_586)
                        {
                            _923 = _485.w == 2.0;
                        }
                        else
                        {
                            _923 = false;
                        }
                        bool _928;
                        if (_923)
                        {
                            _928 = _500.w == 2.0;
                        }
                        else
                        {
                            _928 = false;
                        }
                        float3 _939;
                        if (_928)
                        {
                            _939 = _485.xyz;
                        }
                        else
                        {
                            float3 _934 = _470.xyz;
                            _939 = cross(_485.xyz - _934, _500.xyz - _934);
                        }
                        float3 _940 = fast::normalize(_939);
                        float3 _942 = _470.xyz;
                        float3 _946 = _918 - ((_940 * 2.0) * dot(_918 - _942, _940));
                        float2 _947 = _513.xy;
                        bool3 _949 = isnan(_946);
                        bool3 _950 = isinf(_946);
                        bool _958;
                        if (all(not(bool3(_949.x || _950.x, _949.y || _950.y, _949.z || _950.z))))
                        {
                            _958 = _946.z > 0.0;
                        }
                        else
                        {
                            _958 = false;
                        }
                        float2 _971;
                        if (_958)
                        {
                            _971 = fast::clamp(((Settings.projection.xy * _946.xy) / float2(_946.z)) + Settings.projection.zw, float2(0.0), _947 - float2(0.001000000047497451305389404296875));
                        }
                        else
                        {
                            _971 = _947 * 0.5;
                        }
                        float _974 = precise::max(8.0, precise::min(_505, _508) * 0.125);
                        float2 _976;
                        float2 _979;
                        _976 = _971;
                        _979 = _337;
                        float2 _977;
                        float2 _980;
                        float2 _2125;
                        float2 _2126;
                        float4 _2127;
                        bool _2128;
                        uint _981 = 0u;
                        for (;;)
                        {
                            if (_981 < 16u)
                            {
                                bool _994;
                                float _1134;
                                float2 _1180;
                                bool _1181;
                                do
                                {
                                    float3 _1133;
                                    float _1135;
                                    float3 _1136;
                                    bool _1137;
                                    do
                                    {
                                        bool2 _990 = isnan(_976);
                                        bool2 _991 = isinf(_976);
                                        _994 = all(not(bool2(_990.x || _991.x, _990.y || _991.y)));
                                        bool _999;
                                        if (_994)
                                        {
                                            _999 = any(_976 < float2(0.0));
                                        }
                                        else
                                        {
                                            _999 = true;
                                        }
                                        bool _1005;
                                        if (!_999)
                                        {
                                            _1005 = any(_976 >= _947);
                                        }
                                        else
                                        {
                                            _1005 = true;
                                        }
                                        if (_1005)
                                        {
                                            _1133 = float3(0.0);
                                            _1134 = 0.0;
                                            _1135 = 0.0;
                                            _1136 = float3(0.0);
                                            _1137 = false;
                                            break;
                                        }
                                        bool _1012;
                                        if (_586)
                                        {
                                            _1012 = _485.w == 2.0;
                                        }
                                        else
                                        {
                                            _1012 = false;
                                        }
                                        bool _1017;
                                        if (_1012)
                                        {
                                            _1017 = _500.w == 2.0;
                                        }
                                        else
                                        {
                                            _1017 = false;
                                        }
                                        float3 _1027;
                                        if (_1017)
                                        {
                                            _1027 = _485.xyz;
                                        }
                                        else
                                        {
                                            _1027 = cross(_485.xyz - _942, _500.xyz - _942);
                                        }
                                        float3 _1028 = fast::normalize(_1027);
                                        float3 _1036 = fast::normalize(float3((_976 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                        float3 _1042;
                                        if (dot(_1028, _1036) > 0.0)
                                        {
                                            _1042 = -_1028;
                                        }
                                        else
                                        {
                                            _1042 = _1028;
                                        }
                                        float _1043 = dot(_1036, _1042);
                                        bool _1052;
                                        if (!(isnan(_1043) || isinf(_1043)))
                                        {
                                            _1052 = abs(_1043) <= 9.9999999600419720025001879548654e-13;
                                        }
                                        else
                                        {
                                            _1052 = true;
                                        }
                                        if (_1052)
                                        {
                                            _1133 = float3(0.0);
                                            _1134 = 0.0;
                                            _1135 = 0.0;
                                            _1136 = float3(0.0);
                                            _1137 = false;
                                            break;
                                        }
                                        float _1056 = dot(_942, _1042) / _1043;
                                        float _1058 = _1056 * _1036.z;
                                        bool _1066;
                                        if (!(isnan(_1056) || isinf(_1056)))
                                        {
                                            _1066 = _1056 <= 0.0;
                                        }
                                        else
                                        {
                                            _1066 = true;
                                        }
                                        bool _1071;
                                        if (!_1066)
                                        {
                                            _1071 = _1058 < Settings.clip.x;
                                        }
                                        else
                                        {
                                            _1071 = true;
                                        }
                                        bool _1076;
                                        if (!_1071)
                                        {
                                            _1076 = _1058 > Settings.clip.y;
                                        }
                                        else
                                        {
                                            _1076 = true;
                                        }
                                        if (_1076)
                                        {
                                            _1133 = float3(0.0);
                                            _1134 = _1058;
                                            _1135 = 0.0;
                                            _1136 = float3(0.0);
                                            _1137 = false;
                                            break;
                                        }
                                        bool _1083;
                                        if (_586)
                                        {
                                            _1083 = _485.w == 2.0;
                                        }
                                        else
                                        {
                                            _1083 = false;
                                        }
                                        bool _1088;
                                        if (_1083)
                                        {
                                            _1088 = _500.w == 2.0;
                                        }
                                        else
                                        {
                                            _1088 = false;
                                        }
                                        float _1091 = precise::max(_1088 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _1056 * 9.9999997473787516355514526367188e-06);
                                        float3 _1094 = (_1036 * _1056) + (_1042 * _1091);
                                        float3 _1095 = reflect(_1036, _1042);
                                        uint _1096 = uint(_516);
                                        float3 _1103 = fast::normalize(cross(_1095, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_1095.y) < 0.949999988079071044921875))));
                                        float3 _1115 = fast::normalize(_1095 + (((_1103 * _148[_1096].x) + (cross(_1095, _1103) * _148[_1096].y)) * (Settings.roughness * Settings.roughness)));
                                        float3 _1119 = select(_1115, _1095, bool3(dot(_1115, _1042) <= 0.0));
                                        bool3 _1120 = isnan(_1094);
                                        bool3 _1121 = isinf(_1094);
                                        bool _1132;
                                        if (all(not(bool3(_1120.x || _1121.x, _1120.y || _1121.y, _1120.z || _1121.z))))
                                        {
                                            bool3 _1127 = isnan(_1119);
                                            bool3 _1128 = isinf(_1119);
                                            _1132 = all(not(bool3(_1127.x || _1128.x, _1127.y || _1128.y, _1127.z || _1128.z)));
                                        }
                                        else
                                        {
                                            _1132 = false;
                                        }
                                        _1133 = _1119;
                                        _1134 = _1058;
                                        _1135 = _1091;
                                        _1136 = _1094;
                                        _1137 = _1132;
                                        break;
                                    } while(false);
                                    if (!_1137)
                                    {
                                        _1180 = float2(0.0);
                                        _1181 = false;
                                        break;
                                    }
                                    float3 _1141 = _918 - _1136;
                                    float _1142 = dot(_1141, _1141);
                                    bool _1151;
                                    if (!(isnan(_1142) || isinf(_1142)))
                                    {
                                        _1151 = _1142 <= (_1135 * _1135);
                                    }
                                    else
                                    {
                                        _1151 = true;
                                    }
                                    if (_1151)
                                    {
                                        _1180 = float2(0.0);
                                        _1181 = false;
                                        break;
                                    }
                                    float3 _1155 = _1141 * rsqrt(_1142);
                                    if (dot(_1155, _1133) <= 0.0)
                                    {
                                        _1180 = float2(0.0);
                                        _1181 = false;
                                        break;
                                    }
                                    float3 _1166 = fast::normalize(cross(_1133, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_1133.y) < 0.949999988079071044921875))));
                                    float2 _1174 = float2(dot(_1155, _1166), dot(_1155, cross(_1133, _1166))) * precise::max(Settings.projection.x, Settings.projection.y);
                                    bool2 _1175 = isnan(_1174);
                                    bool2 _1176 = isinf(_1174);
                                    _1180 = _1174;
                                    _1181 = all(not(bool2(_1175.x || _1176.x, _1175.y || _1176.y)));
                                    break;
                                } while(false);
                                if (!_1181)
                                {
                                    _2125 = _976;
                                    _2126 = _979;
                                    _2127 = float4(0.0);
                                    _2128 = true;
                                    break;
                                }
                                float _1189 = precise::max(abs(_1180.x), abs(_1180.y));
                                if (_1189 <= 0.00200000009499490261077880859375)
                                {
                                    float4 _1445;
                                    do
                                    {
                                        bool _1266;
                                        do
                                        {
                                            bool _1201;
                                            if (_586)
                                            {
                                                _1201 = _485.w == 2.0;
                                            }
                                            else
                                            {
                                                _1201 = false;
                                            }
                                            bool _1206;
                                            if (_1201)
                                            {
                                                _1206 = _500.w == 2.0;
                                            }
                                            else
                                            {
                                                _1206 = false;
                                            }
                                            if (_1206)
                                            {
                                                _1266 = true;
                                                break;
                                            }
                                            float3 _1218 = _485.xyz - _942;
                                            float3 _1220 = _500.xyz - _942;
                                            float3 _1221 = (float3((_976 - Settings.projection.zw) / Settings.projection.xy, 1.0) * _1134) - _942;
                                            float _1222 = dot(_1218, _1218);
                                            float _1223 = dot(_1218, _1220);
                                            float _1224 = dot(_1220, _1220);
                                            float _1225 = dot(_1221, _1218);
                                            float _1226 = dot(_1221, _1220);
                                            float _1229 = (_1222 * _1224) - (_1223 * _1223);
                                            bool _1237;
                                            if (!(isnan(_1229) || isinf(_1229)))
                                            {
                                                _1237 = _1229 <= 9.9999996826552253889678874634872e-21;
                                            }
                                            else
                                            {
                                                _1237 = true;
                                            }
                                            if (_1237)
                                            {
                                                _1266 = false;
                                                break;
                                            }
                                            float2 _1248 = float2((_1224 * _1225) - (_1223 * _1226), (_1222 * _1226) - (_1223 * _1225)) / float2(_1229);
                                            bool2 _1249 = isnan(_1248);
                                            bool2 _1250 = isinf(_1248);
                                            bool _1258;
                                            if (all(not(bool2(_1249.x || _1250.x, _1249.y || _1250.y))))
                                            {
                                                _1258 = all(_1248 >= float2(-9.9999997473787516355514526367188e-06));
                                            }
                                            else
                                            {
                                                _1258 = false;
                                            }
                                            bool _1265;
                                            if (_1258)
                                            {
                                                _1265 = (_1248.x + _1248.y) <= 1.000010013580322265625;
                                            }
                                            else
                                            {
                                                _1265 = false;
                                            }
                                            _1266 = _1265;
                                            break;
                                        } while(false);
                                        if (!_1266)
                                        {
                                            _1445 = float4(0.0);
                                            break;
                                        }
                                        float3 _1410;
                                        bool _1411;
                                        do
                                        {
                                            bool _1276;
                                            if (_994)
                                            {
                                                _1276 = any(_976 < float2(0.0));
                                            }
                                            else
                                            {
                                                _1276 = true;
                                            }
                                            bool _1282;
                                            if (!_1276)
                                            {
                                                _1282 = any(_976 >= _947);
                                            }
                                            else
                                            {
                                                _1282 = true;
                                            }
                                            if (_1282)
                                            {
                                                _1410 = float3(0.0);
                                                _1411 = false;
                                                break;
                                            }
                                            bool _1289;
                                            if (_586)
                                            {
                                                _1289 = _485.w == 2.0;
                                            }
                                            else
                                            {
                                                _1289 = false;
                                            }
                                            bool _1294;
                                            if (_1289)
                                            {
                                                _1294 = _500.w == 2.0;
                                            }
                                            else
                                            {
                                                _1294 = false;
                                            }
                                            float3 _1304;
                                            if (_1294)
                                            {
                                                _1304 = _485.xyz;
                                            }
                                            else
                                            {
                                                _1304 = cross(_485.xyz - _942, _500.xyz - _942);
                                            }
                                            float3 _1305 = fast::normalize(_1304);
                                            float3 _1313 = fast::normalize(float3((_976 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                            float3 _1319;
                                            if (dot(_1305, _1313) > 0.0)
                                            {
                                                _1319 = -_1305;
                                            }
                                            else
                                            {
                                                _1319 = _1305;
                                            }
                                            float _1320 = dot(_1313, _1319);
                                            bool _1329;
                                            if (!(isnan(_1320) || isinf(_1320)))
                                            {
                                                _1329 = abs(_1320) <= 9.9999999600419720025001879548654e-13;
                                            }
                                            else
                                            {
                                                _1329 = true;
                                            }
                                            if (_1329)
                                            {
                                                _1410 = float3(0.0);
                                                _1411 = false;
                                                break;
                                            }
                                            float _1333 = dot(_942, _1319) / _1320;
                                            float _1335 = _1333 * _1313.z;
                                            bool _1343;
                                            if (!(isnan(_1333) || isinf(_1333)))
                                            {
                                                _1343 = _1333 <= 0.0;
                                            }
                                            else
                                            {
                                                _1343 = true;
                                            }
                                            bool _1348;
                                            if (!_1343)
                                            {
                                                _1348 = _1335 < Settings.clip.x;
                                            }
                                            else
                                            {
                                                _1348 = true;
                                            }
                                            bool _1353;
                                            if (!_1348)
                                            {
                                                _1353 = _1335 > Settings.clip.y;
                                            }
                                            else
                                            {
                                                _1353 = true;
                                            }
                                            if (_1353)
                                            {
                                                _1410 = float3(0.0);
                                                _1411 = false;
                                                break;
                                            }
                                            bool _1360;
                                            if (_586)
                                            {
                                                _1360 = _485.w == 2.0;
                                            }
                                            else
                                            {
                                                _1360 = false;
                                            }
                                            bool _1365;
                                            if (_1360)
                                            {
                                                _1365 = _500.w == 2.0;
                                            }
                                            else
                                            {
                                                _1365 = false;
                                            }
                                            float3 _1371 = (_1313 * _1333) + (_1319 * precise::max(_1365 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _1333 * 9.9999997473787516355514526367188e-06));
                                            float3 _1372 = reflect(_1313, _1319);
                                            uint _1373 = uint(_516);
                                            float3 _1380 = fast::normalize(cross(_1372, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_1372.y) < 0.949999988079071044921875))));
                                            float3 _1392 = fast::normalize(_1372 + (((_1380 * _148[_1373].x) + (cross(_1372, _1380) * _148[_1373].y)) * (Settings.roughness * Settings.roughness)));
                                            float3 _1396 = select(_1392, _1372, bool3(dot(_1392, _1319) <= 0.0));
                                            bool3 _1397 = isnan(_1371);
                                            bool3 _1398 = isinf(_1371);
                                            bool _1409;
                                            if (all(not(bool3(_1397.x || _1398.x, _1397.y || _1398.y, _1397.z || _1398.z))))
                                            {
                                                bool3 _1404 = isnan(_1396);
                                                bool3 _1405 = isinf(_1396);
                                                _1409 = all(not(bool3(_1404.x || _1405.x, _1404.y || _1405.y, _1404.z || _1405.z)));
                                            }
                                            else
                                            {
                                                _1409 = false;
                                            }
                                            _1410 = _1371;
                                            _1411 = _1409;
                                            break;
                                        } while(false);
                                        if (!_1411)
                                        {
                                            _1445 = float4(0.0);
                                            break;
                                        }
                                        float3 _1417 = precise::max(abs(_1410), abs(_918));
                                        float _1425 = length(_918 - _1410);
                                        bool _1439;
                                        if (!(isnan(_1425) || isinf(_1425)))
                                        {
                                            _1439 = ((precise::max(1.0, precise::max(_1417.x, precise::max(_1417.y, _1417.z))) * 9.9999999747524270787835121154785e-07) * precise::max(Settings.projection.x, Settings.projection.y)) > (_1425 * 0.01200000010430812835693359375);
                                        }
                                        else
                                        {
                                            _1439 = true;
                                        }
                                        if (_1439)
                                        {
                                            _1445 = float4(0.0);
                                            break;
                                        }
                                        _1445 = float4(_976, _1134, 1.0);
                                        break;
                                    } while(false);
                                    _2125 = _976;
                                    _2126 = _979;
                                    _2127 = _1445;
                                    _2128 = true;
                                    break;
                                }
                                float _1449 = ((_976.x + 0.25) < _505) ? 0.25 : (-0.25);
                                float _1453 = ((_976.y + 0.25) < _508) ? 0.25 : (-0.25);
                                float2 _1455 = _976 + float2(_1449, 0.0);
                                float2 _1649;
                                bool _1650;
                                do
                                {
                                    float3 _1603;
                                    float _1604;
                                    float3 _1605;
                                    bool _1606;
                                    do
                                    {
                                        bool2 _1460 = isnan(_1455);
                                        bool2 _1461 = isinf(_1455);
                                        bool _1469;
                                        if (all(not(bool2(_1460.x || _1461.x, _1460.y || _1461.y))))
                                        {
                                            _1469 = any(_1455 < float2(0.0));
                                        }
                                        else
                                        {
                                            _1469 = true;
                                        }
                                        bool _1475;
                                        if (!_1469)
                                        {
                                            _1475 = any(_1455 >= _947);
                                        }
                                        else
                                        {
                                            _1475 = true;
                                        }
                                        if (_1475)
                                        {
                                            _1603 = float3(0.0);
                                            _1604 = 0.0;
                                            _1605 = float3(0.0);
                                            _1606 = false;
                                            break;
                                        }
                                        bool _1482;
                                        if (_586)
                                        {
                                            _1482 = _485.w == 2.0;
                                        }
                                        else
                                        {
                                            _1482 = false;
                                        }
                                        bool _1487;
                                        if (_1482)
                                        {
                                            _1487 = _500.w == 2.0;
                                        }
                                        else
                                        {
                                            _1487 = false;
                                        }
                                        float3 _1497;
                                        if (_1487)
                                        {
                                            _1497 = _485.xyz;
                                        }
                                        else
                                        {
                                            _1497 = cross(_485.xyz - _942, _500.xyz - _942);
                                        }
                                        float3 _1498 = fast::normalize(_1497);
                                        float3 _1506 = fast::normalize(float3((_1455 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                        float3 _1512;
                                        if (dot(_1498, _1506) > 0.0)
                                        {
                                            _1512 = -_1498;
                                        }
                                        else
                                        {
                                            _1512 = _1498;
                                        }
                                        float _1513 = dot(_1506, _1512);
                                        bool _1522;
                                        if (!(isnan(_1513) || isinf(_1513)))
                                        {
                                            _1522 = abs(_1513) <= 9.9999999600419720025001879548654e-13;
                                        }
                                        else
                                        {
                                            _1522 = true;
                                        }
                                        if (_1522)
                                        {
                                            _1603 = float3(0.0);
                                            _1604 = 0.0;
                                            _1605 = float3(0.0);
                                            _1606 = false;
                                            break;
                                        }
                                        float _1526 = dot(_942, _1512) / _1513;
                                        float _1528 = _1526 * _1506.z;
                                        bool _1536;
                                        if (!(isnan(_1526) || isinf(_1526)))
                                        {
                                            _1536 = _1526 <= 0.0;
                                        }
                                        else
                                        {
                                            _1536 = true;
                                        }
                                        bool _1541;
                                        if (!_1536)
                                        {
                                            _1541 = _1528 < Settings.clip.x;
                                        }
                                        else
                                        {
                                            _1541 = true;
                                        }
                                        bool _1546;
                                        if (!_1541)
                                        {
                                            _1546 = _1528 > Settings.clip.y;
                                        }
                                        else
                                        {
                                            _1546 = true;
                                        }
                                        if (_1546)
                                        {
                                            _1603 = float3(0.0);
                                            _1604 = 0.0;
                                            _1605 = float3(0.0);
                                            _1606 = false;
                                            break;
                                        }
                                        bool _1553;
                                        if (_586)
                                        {
                                            _1553 = _485.w == 2.0;
                                        }
                                        else
                                        {
                                            _1553 = false;
                                        }
                                        bool _1558;
                                        if (_1553)
                                        {
                                            _1558 = _500.w == 2.0;
                                        }
                                        else
                                        {
                                            _1558 = false;
                                        }
                                        float _1561 = precise::max(_1558 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _1526 * 9.9999997473787516355514526367188e-06);
                                        float3 _1564 = (_1506 * _1526) + (_1512 * _1561);
                                        float3 _1565 = reflect(_1506, _1512);
                                        uint _1566 = uint(_516);
                                        float3 _1573 = fast::normalize(cross(_1565, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_1565.y) < 0.949999988079071044921875))));
                                        float3 _1585 = fast::normalize(_1565 + (((_1573 * _148[_1566].x) + (cross(_1565, _1573) * _148[_1566].y)) * (Settings.roughness * Settings.roughness)));
                                        float3 _1589 = select(_1585, _1565, bool3(dot(_1585, _1512) <= 0.0));
                                        bool3 _1590 = isnan(_1564);
                                        bool3 _1591 = isinf(_1564);
                                        bool _1602;
                                        if (all(not(bool3(_1590.x || _1591.x, _1590.y || _1591.y, _1590.z || _1591.z))))
                                        {
                                            bool3 _1597 = isnan(_1589);
                                            bool3 _1598 = isinf(_1589);
                                            _1602 = all(not(bool3(_1597.x || _1598.x, _1597.y || _1598.y, _1597.z || _1598.z)));
                                        }
                                        else
                                        {
                                            _1602 = false;
                                        }
                                        _1603 = _1589;
                                        _1604 = _1561;
                                        _1605 = _1564;
                                        _1606 = _1602;
                                        break;
                                    } while(false);
                                    if (!_1606)
                                    {
                                        _1649 = float2(0.0);
                                        _1650 = false;
                                        break;
                                    }
                                    float3 _1610 = _918 - _1605;
                                    float _1611 = dot(_1610, _1610);
                                    bool _1620;
                                    if (!(isnan(_1611) || isinf(_1611)))
                                    {
                                        _1620 = _1611 <= (_1604 * _1604);
                                    }
                                    else
                                    {
                                        _1620 = true;
                                    }
                                    if (_1620)
                                    {
                                        _1649 = float2(0.0);
                                        _1650 = false;
                                        break;
                                    }
                                    float3 _1624 = _1610 * rsqrt(_1611);
                                    if (dot(_1624, _1603) <= 0.0)
                                    {
                                        _1649 = float2(0.0);
                                        _1650 = false;
                                        break;
                                    }
                                    float3 _1635 = fast::normalize(cross(_1603, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_1603.y) < 0.949999988079071044921875))));
                                    float2 _1643 = float2(dot(_1624, _1635), dot(_1624, cross(_1603, _1635))) * precise::max(Settings.projection.x, Settings.projection.y);
                                    bool2 _1644 = isnan(_1643);
                                    bool2 _1645 = isinf(_1643);
                                    _1649 = _1643;
                                    _1650 = all(not(bool2(_1644.x || _1645.x, _1644.y || _1645.y)));
                                    break;
                                } while(false);
                                bool _1851;
                                if (_1650)
                                {
                                    float2 _1654 = _976 + float2(0.0, _1453);
                                    float2 _1848;
                                    bool _1849;
                                    do
                                    {
                                        float3 _1802;
                                        float _1803;
                                        float3 _1804;
                                        bool _1805;
                                        do
                                        {
                                            bool2 _1659 = isnan(_1654);
                                            bool2 _1660 = isinf(_1654);
                                            bool _1668;
                                            if (all(not(bool2(_1659.x || _1660.x, _1659.y || _1660.y))))
                                            {
                                                _1668 = any(_1654 < float2(0.0));
                                            }
                                            else
                                            {
                                                _1668 = true;
                                            }
                                            bool _1674;
                                            if (!_1668)
                                            {
                                                _1674 = any(_1654 >= _947);
                                            }
                                            else
                                            {
                                                _1674 = true;
                                            }
                                            if (_1674)
                                            {
                                                _1802 = float3(0.0);
                                                _1803 = 0.0;
                                                _1804 = float3(0.0);
                                                _1805 = false;
                                                break;
                                            }
                                            bool _1681;
                                            if (_586)
                                            {
                                                _1681 = _485.w == 2.0;
                                            }
                                            else
                                            {
                                                _1681 = false;
                                            }
                                            bool _1686;
                                            if (_1681)
                                            {
                                                _1686 = _500.w == 2.0;
                                            }
                                            else
                                            {
                                                _1686 = false;
                                            }
                                            float3 _1696;
                                            if (_1686)
                                            {
                                                _1696 = _485.xyz;
                                            }
                                            else
                                            {
                                                _1696 = cross(_485.xyz - _942, _500.xyz - _942);
                                            }
                                            float3 _1697 = fast::normalize(_1696);
                                            float3 _1705 = fast::normalize(float3((_1654 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                            float3 _1711;
                                            if (dot(_1697, _1705) > 0.0)
                                            {
                                                _1711 = -_1697;
                                            }
                                            else
                                            {
                                                _1711 = _1697;
                                            }
                                            float _1712 = dot(_1705, _1711);
                                            bool _1721;
                                            if (!(isnan(_1712) || isinf(_1712)))
                                            {
                                                _1721 = abs(_1712) <= 9.9999999600419720025001879548654e-13;
                                            }
                                            else
                                            {
                                                _1721 = true;
                                            }
                                            if (_1721)
                                            {
                                                _1802 = float3(0.0);
                                                _1803 = 0.0;
                                                _1804 = float3(0.0);
                                                _1805 = false;
                                                break;
                                            }
                                            float _1725 = dot(_942, _1711) / _1712;
                                            float _1727 = _1725 * _1705.z;
                                            bool _1735;
                                            if (!(isnan(_1725) || isinf(_1725)))
                                            {
                                                _1735 = _1725 <= 0.0;
                                            }
                                            else
                                            {
                                                _1735 = true;
                                            }
                                            bool _1740;
                                            if (!_1735)
                                            {
                                                _1740 = _1727 < Settings.clip.x;
                                            }
                                            else
                                            {
                                                _1740 = true;
                                            }
                                            bool _1745;
                                            if (!_1740)
                                            {
                                                _1745 = _1727 > Settings.clip.y;
                                            }
                                            else
                                            {
                                                _1745 = true;
                                            }
                                            if (_1745)
                                            {
                                                _1802 = float3(0.0);
                                                _1803 = 0.0;
                                                _1804 = float3(0.0);
                                                _1805 = false;
                                                break;
                                            }
                                            bool _1752;
                                            if (_586)
                                            {
                                                _1752 = _485.w == 2.0;
                                            }
                                            else
                                            {
                                                _1752 = false;
                                            }
                                            bool _1757;
                                            if (_1752)
                                            {
                                                _1757 = _500.w == 2.0;
                                            }
                                            else
                                            {
                                                _1757 = false;
                                            }
                                            float _1760 = precise::max(_1757 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _1725 * 9.9999997473787516355514526367188e-06);
                                            float3 _1763 = (_1705 * _1725) + (_1711 * _1760);
                                            float3 _1764 = reflect(_1705, _1711);
                                            uint _1765 = uint(_516);
                                            float3 _1772 = fast::normalize(cross(_1764, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_1764.y) < 0.949999988079071044921875))));
                                            float3 _1784 = fast::normalize(_1764 + (((_1772 * _148[_1765].x) + (cross(_1764, _1772) * _148[_1765].y)) * (Settings.roughness * Settings.roughness)));
                                            float3 _1788 = select(_1784, _1764, bool3(dot(_1784, _1711) <= 0.0));
                                            bool3 _1789 = isnan(_1763);
                                            bool3 _1790 = isinf(_1763);
                                            bool _1801;
                                            if (all(not(bool3(_1789.x || _1790.x, _1789.y || _1790.y, _1789.z || _1790.z))))
                                            {
                                                bool3 _1796 = isnan(_1788);
                                                bool3 _1797 = isinf(_1788);
                                                _1801 = all(not(bool3(_1796.x || _1797.x, _1796.y || _1797.y, _1796.z || _1797.z)));
                                            }
                                            else
                                            {
                                                _1801 = false;
                                            }
                                            _1802 = _1788;
                                            _1803 = _1760;
                                            _1804 = _1763;
                                            _1805 = _1801;
                                            break;
                                        } while(false);
                                        if (!_1805)
                                        {
                                            _1848 = float2(0.0);
                                            _1849 = false;
                                            break;
                                        }
                                        float3 _1809 = _918 - _1804;
                                        float _1810 = dot(_1809, _1809);
                                        bool _1819;
                                        if (!(isnan(_1810) || isinf(_1810)))
                                        {
                                            _1819 = _1810 <= (_1803 * _1803);
                                        }
                                        else
                                        {
                                            _1819 = true;
                                        }
                                        if (_1819)
                                        {
                                            _1848 = float2(0.0);
                                            _1849 = false;
                                            break;
                                        }
                                        float3 _1823 = _1809 * rsqrt(_1810);
                                        if (dot(_1823, _1802) <= 0.0)
                                        {
                                            _1848 = float2(0.0);
                                            _1849 = false;
                                            break;
                                        }
                                        float3 _1834 = fast::normalize(cross(_1802, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_1802.y) < 0.949999988079071044921875))));
                                        float2 _1842 = float2(dot(_1823, _1834), dot(_1823, cross(_1802, _1834))) * precise::max(Settings.projection.x, Settings.projection.y);
                                        bool2 _1843 = isnan(_1842);
                                        bool2 _1844 = isinf(_1842);
                                        _1848 = _1842;
                                        _1849 = all(not(bool2(_1843.x || _1844.x, _1843.y || _1844.y)));
                                        break;
                                    } while(false);
                                    _980 = _1848;
                                    _1851 = !_1849;
                                }
                                else
                                {
                                    _980 = _979;
                                    _1851 = true;
                                }
                                if (_1851)
                                {
                                    _2125 = _976;
                                    _2126 = _980;
                                    _2127 = float4(0.0);
                                    _2128 = true;
                                    break;
                                }
                                float2 _1856 = (_1649 - _1180) / float2(_1449);
                                float2 _1859 = (_980 - _1180) / float2(_1453);
                                float _1860 = _1856.x;
                                float _1861 = _1859.y;
                                float _1863 = _1859.x;
                                float _1864 = _1856.y;
                                float _1866 = (_1860 * _1861) - (_1863 * _1864);
                                bool _1875;
                                if (!(isnan(_1866) || isinf(_1866)))
                                {
                                    _1875 = abs(_1866) < 9.9999999392252902907785028219223e-09;
                                }
                                else
                                {
                                    _1875 = true;
                                }
                                if (_1875)
                                {
                                    _2125 = _976;
                                    _2126 = _980;
                                    _2127 = float4(0.0);
                                    _2128 = true;
                                    break;
                                }
                                float2 _1887 = float2((_1861 * _1180.x) - (_1863 * _1180.y), ((-_1864) * _1180.x) + (_1860 * _1180.y)) / float2(_1866);
                                bool2 _1888 = isnan(_1887);
                                bool2 _1889 = isinf(_1887);
                                if (!all(not(bool2(_1888.x || _1889.x, _1888.y || _1889.y))))
                                {
                                    _2125 = _976;
                                    _2126 = _980;
                                    _2127 = float4(0.0);
                                    _2128 = true;
                                    break;
                                }
                                float2 _1906;
                                _1906 = _1887 * precise::min(1.0, _974 / precise::max(precise::max(abs(_1887.x), abs(_1887.y)), 9.9999999600419720025001879548654e-13));
                                float2 _1907;
                                bool _2121;
                                uint _1909 = 0u;
                                for (;;)
                                {
                                    if (_1909 < 6u)
                                    {
                                        float2 _1914 = _976 - _1906;
                                        float2 _2108;
                                        bool _2109;
                                        do
                                        {
                                            float3 _2062;
                                            float _2063;
                                            float3 _2064;
                                            bool _2065;
                                            do
                                            {
                                                bool2 _1919 = isnan(_1914);
                                                bool2 _1920 = isinf(_1914);
                                                bool _1928;
                                                if (all(not(bool2(_1919.x || _1920.x, _1919.y || _1920.y))))
                                                {
                                                    _1928 = any(_1914 < float2(0.0));
                                                }
                                                else
                                                {
                                                    _1928 = true;
                                                }
                                                bool _1934;
                                                if (!_1928)
                                                {
                                                    _1934 = any(_1914 >= _947);
                                                }
                                                else
                                                {
                                                    _1934 = true;
                                                }
                                                if (_1934)
                                                {
                                                    _2062 = float3(0.0);
                                                    _2063 = 0.0;
                                                    _2064 = float3(0.0);
                                                    _2065 = false;
                                                    break;
                                                }
                                                bool _1941;
                                                if (_586)
                                                {
                                                    _1941 = _485.w == 2.0;
                                                }
                                                else
                                                {
                                                    _1941 = false;
                                                }
                                                bool _1946;
                                                if (_1941)
                                                {
                                                    _1946 = _500.w == 2.0;
                                                }
                                                else
                                                {
                                                    _1946 = false;
                                                }
                                                float3 _1956;
                                                if (_1946)
                                                {
                                                    _1956 = _485.xyz;
                                                }
                                                else
                                                {
                                                    _1956 = cross(_485.xyz - _942, _500.xyz - _942);
                                                }
                                                float3 _1957 = fast::normalize(_1956);
                                                float3 _1965 = fast::normalize(float3((_1914 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                                float3 _1971;
                                                if (dot(_1957, _1965) > 0.0)
                                                {
                                                    _1971 = -_1957;
                                                }
                                                else
                                                {
                                                    _1971 = _1957;
                                                }
                                                float _1972 = dot(_1965, _1971);
                                                bool _1981;
                                                if (!(isnan(_1972) || isinf(_1972)))
                                                {
                                                    _1981 = abs(_1972) <= 9.9999999600419720025001879548654e-13;
                                                }
                                                else
                                                {
                                                    _1981 = true;
                                                }
                                                if (_1981)
                                                {
                                                    _2062 = float3(0.0);
                                                    _2063 = 0.0;
                                                    _2064 = float3(0.0);
                                                    _2065 = false;
                                                    break;
                                                }
                                                float _1985 = dot(_942, _1971) / _1972;
                                                float _1987 = _1985 * _1965.z;
                                                bool _1995;
                                                if (!(isnan(_1985) || isinf(_1985)))
                                                {
                                                    _1995 = _1985 <= 0.0;
                                                }
                                                else
                                                {
                                                    _1995 = true;
                                                }
                                                bool _2000;
                                                if (!_1995)
                                                {
                                                    _2000 = _1987 < Settings.clip.x;
                                                }
                                                else
                                                {
                                                    _2000 = true;
                                                }
                                                bool _2005;
                                                if (!_2000)
                                                {
                                                    _2005 = _1987 > Settings.clip.y;
                                                }
                                                else
                                                {
                                                    _2005 = true;
                                                }
                                                if (_2005)
                                                {
                                                    _2062 = float3(0.0);
                                                    _2063 = 0.0;
                                                    _2064 = float3(0.0);
                                                    _2065 = false;
                                                    break;
                                                }
                                                bool _2012;
                                                if (_586)
                                                {
                                                    _2012 = _485.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2012 = false;
                                                }
                                                bool _2017;
                                                if (_2012)
                                                {
                                                    _2017 = _500.w == 2.0;
                                                }
                                                else
                                                {
                                                    _2017 = false;
                                                }
                                                float _2020 = precise::max(_2017 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _1985 * 9.9999997473787516355514526367188e-06);
                                                float3 _2023 = (_1965 * _1985) + (_1971 * _2020);
                                                float3 _2024 = reflect(_1965, _1971);
                                                uint _2025 = uint(_516);
                                                float3 _2032 = fast::normalize(cross(_2024, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_2024.y) < 0.949999988079071044921875))));
                                                float3 _2044 = fast::normalize(_2024 + (((_2032 * _148[_2025].x) + (cross(_2024, _2032) * _148[_2025].y)) * (Settings.roughness * Settings.roughness)));
                                                float3 _2048 = select(_2044, _2024, bool3(dot(_2044, _1971) <= 0.0));
                                                bool3 _2049 = isnan(_2023);
                                                bool3 _2050 = isinf(_2023);
                                                bool _2061;
                                                if (all(not(bool3(_2049.x || _2050.x, _2049.y || _2050.y, _2049.z || _2050.z))))
                                                {
                                                    bool3 _2056 = isnan(_2048);
                                                    bool3 _2057 = isinf(_2048);
                                                    _2061 = all(not(bool3(_2056.x || _2057.x, _2056.y || _2057.y, _2056.z || _2057.z)));
                                                }
                                                else
                                                {
                                                    _2061 = false;
                                                }
                                                _2062 = _2048;
                                                _2063 = _2020;
                                                _2064 = _2023;
                                                _2065 = _2061;
                                                break;
                                            } while(false);
                                            if (!_2065)
                                            {
                                                _2108 = float2(0.0);
                                                _2109 = false;
                                                break;
                                            }
                                            float3 _2069 = _918 - _2064;
                                            float _2070 = dot(_2069, _2069);
                                            bool _2079;
                                            if (!(isnan(_2070) || isinf(_2070)))
                                            {
                                                _2079 = _2070 <= (_2063 * _2063);
                                            }
                                            else
                                            {
                                                _2079 = true;
                                            }
                                            if (_2079)
                                            {
                                                _2108 = float2(0.0);
                                                _2109 = false;
                                                break;
                                            }
                                            float3 _2083 = _2069 * rsqrt(_2070);
                                            if (dot(_2083, _2062) <= 0.0)
                                            {
                                                _2108 = float2(0.0);
                                                _2109 = false;
                                                break;
                                            }
                                            float3 _2094 = fast::normalize(cross(_2062, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_2062.y) < 0.949999988079071044921875))));
                                            float2 _2102 = float2(dot(_2083, _2094), dot(_2083, cross(_2062, _2094))) * precise::max(Settings.projection.x, Settings.projection.y);
                                            bool2 _2103 = isnan(_2102);
                                            bool2 _2104 = isinf(_2102);
                                            _2108 = _2102;
                                            _2109 = all(not(bool2(_2103.x || _2104.x, _2103.y || _2104.y)));
                                            break;
                                        } while(false);
                                        bool _2118;
                                        if (_2109)
                                        {
                                            _2118 = precise::max(abs(_2108.x), abs(_2108.y)) < _1189;
                                        }
                                        else
                                        {
                                            _2118 = false;
                                        }
                                        if (_2118)
                                        {
                                            _977 = _1914;
                                            _2121 = true;
                                            break;
                                        }
                                        _1907 = _1906 * 0.5;
                                        _1906 = _1907;
                                        _1909++;
                                        continue;
                                    }
                                    else
                                    {
                                        _977 = _976;
                                        _2121 = false;
                                        break;
                                    }
                                }
                                if (!_2121)
                                {
                                    _2125 = _977;
                                    _2126 = _980;
                                    _2127 = float4(0.0);
                                    _2128 = true;
                                    break;
                                }
                                _976 = _977;
                                _979 = _980;
                                _981++;
                                continue;
                            }
                            else
                            {
                                _2125 = _976;
                                _2126 = _979;
                                _2127 = _335;
                                _2128 = false;
                                break;
                            }
                        }
                        if (_2128)
                        {
                            _2590 = _2126;
                            _2591 = _2127;
                            break;
                        }
                        bool _2138;
                        float _2278;
                        float2 _2324;
                        bool _2325;
                        do
                        {
                            float3 _2277;
                            float _2279;
                            float3 _2280;
                            bool _2281;
                            do
                            {
                                bool2 _2134 = isnan(_2125);
                                bool2 _2135 = isinf(_2125);
                                _2138 = all(not(bool2(_2134.x || _2135.x, _2134.y || _2135.y)));
                                bool _2143;
                                if (_2138)
                                {
                                    _2143 = any(_2125 < float2(0.0));
                                }
                                else
                                {
                                    _2143 = true;
                                }
                                bool _2149;
                                if (!_2143)
                                {
                                    _2149 = any(_2125 >= _947);
                                }
                                else
                                {
                                    _2149 = true;
                                }
                                if (_2149)
                                {
                                    _2277 = float3(0.0);
                                    _2278 = 0.0;
                                    _2279 = 0.0;
                                    _2280 = float3(0.0);
                                    _2281 = false;
                                    break;
                                }
                                bool _2156;
                                if (_586)
                                {
                                    _2156 = _485.w == 2.0;
                                }
                                else
                                {
                                    _2156 = false;
                                }
                                bool _2161;
                                if (_2156)
                                {
                                    _2161 = _500.w == 2.0;
                                }
                                else
                                {
                                    _2161 = false;
                                }
                                float3 _2171;
                                if (_2161)
                                {
                                    _2171 = _485.xyz;
                                }
                                else
                                {
                                    _2171 = cross(_485.xyz - _942, _500.xyz - _942);
                                }
                                float3 _2172 = fast::normalize(_2171);
                                float3 _2180 = fast::normalize(float3((_2125 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                float3 _2186;
                                if (dot(_2172, _2180) > 0.0)
                                {
                                    _2186 = -_2172;
                                }
                                else
                                {
                                    _2186 = _2172;
                                }
                                float _2187 = dot(_2180, _2186);
                                bool _2196;
                                if (!(isnan(_2187) || isinf(_2187)))
                                {
                                    _2196 = abs(_2187) <= 9.9999999600419720025001879548654e-13;
                                }
                                else
                                {
                                    _2196 = true;
                                }
                                if (_2196)
                                {
                                    _2277 = float3(0.0);
                                    _2278 = 0.0;
                                    _2279 = 0.0;
                                    _2280 = float3(0.0);
                                    _2281 = false;
                                    break;
                                }
                                float _2200 = dot(_942, _2186) / _2187;
                                float _2202 = _2200 * _2180.z;
                                bool _2210;
                                if (!(isnan(_2200) || isinf(_2200)))
                                {
                                    _2210 = _2200 <= 0.0;
                                }
                                else
                                {
                                    _2210 = true;
                                }
                                bool _2215;
                                if (!_2210)
                                {
                                    _2215 = _2202 < Settings.clip.x;
                                }
                                else
                                {
                                    _2215 = true;
                                }
                                bool _2220;
                                if (!_2215)
                                {
                                    _2220 = _2202 > Settings.clip.y;
                                }
                                else
                                {
                                    _2220 = true;
                                }
                                if (_2220)
                                {
                                    _2277 = float3(0.0);
                                    _2278 = _2202;
                                    _2279 = 0.0;
                                    _2280 = float3(0.0);
                                    _2281 = false;
                                    break;
                                }
                                bool _2227;
                                if (_586)
                                {
                                    _2227 = _485.w == 2.0;
                                }
                                else
                                {
                                    _2227 = false;
                                }
                                bool _2232;
                                if (_2227)
                                {
                                    _2232 = _500.w == 2.0;
                                }
                                else
                                {
                                    _2232 = false;
                                }
                                float _2235 = precise::max(_2232 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _2200 * 9.9999997473787516355514526367188e-06);
                                float3 _2238 = (_2180 * _2200) + (_2186 * _2235);
                                float3 _2239 = reflect(_2180, _2186);
                                uint _2240 = uint(_516);
                                float3 _2247 = fast::normalize(cross(_2239, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_2239.y) < 0.949999988079071044921875))));
                                float3 _2259 = fast::normalize(_2239 + (((_2247 * _148[_2240].x) + (cross(_2239, _2247) * _148[_2240].y)) * (Settings.roughness * Settings.roughness)));
                                float3 _2263 = select(_2259, _2239, bool3(dot(_2259, _2186) <= 0.0));
                                bool3 _2264 = isnan(_2238);
                                bool3 _2265 = isinf(_2238);
                                bool _2276;
                                if (all(not(bool3(_2264.x || _2265.x, _2264.y || _2265.y, _2264.z || _2265.z))))
                                {
                                    bool3 _2271 = isnan(_2263);
                                    bool3 _2272 = isinf(_2263);
                                    _2276 = all(not(bool3(_2271.x || _2272.x, _2271.y || _2272.y, _2271.z || _2272.z)));
                                }
                                else
                                {
                                    _2276 = false;
                                }
                                _2277 = _2263;
                                _2278 = _2202;
                                _2279 = _2235;
                                _2280 = _2238;
                                _2281 = _2276;
                                break;
                            } while(false);
                            if (!_2281)
                            {
                                _2324 = float2(0.0);
                                _2325 = false;
                                break;
                            }
                            float3 _2285 = _918 - _2280;
                            float _2286 = dot(_2285, _2285);
                            bool _2295;
                            if (!(isnan(_2286) || isinf(_2286)))
                            {
                                _2295 = _2286 <= (_2279 * _2279);
                            }
                            else
                            {
                                _2295 = true;
                            }
                            if (_2295)
                            {
                                _2324 = float2(0.0);
                                _2325 = false;
                                break;
                            }
                            float3 _2299 = _2285 * rsqrt(_2286);
                            if (dot(_2299, _2277) <= 0.0)
                            {
                                _2324 = float2(0.0);
                                _2325 = false;
                                break;
                            }
                            float3 _2310 = fast::normalize(cross(_2277, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_2277.y) < 0.949999988079071044921875))));
                            float2 _2318 = float2(dot(_2299, _2310), dot(_2299, cross(_2277, _2310))) * precise::max(Settings.projection.x, Settings.projection.y);
                            bool2 _2319 = isnan(_2318);
                            bool2 _2320 = isinf(_2318);
                            _2324 = _2318;
                            _2325 = all(not(bool2(_2319.x || _2320.x, _2319.y || _2320.y)));
                            break;
                        } while(false);
                        bool _2334;
                        if (_2325)
                        {
                            _2334 = precise::max(abs(_2324.x), abs(_2324.y)) <= 0.00200000009499490261077880859375;
                        }
                        else
                        {
                            _2334 = false;
                        }
                        if (_2334)
                        {
                            float4 _2589;
                            do
                            {
                                bool _2410;
                                do
                                {
                                    bool _2345;
                                    if (_586)
                                    {
                                        _2345 = _485.w == 2.0;
                                    }
                                    else
                                    {
                                        _2345 = false;
                                    }
                                    bool _2350;
                                    if (_2345)
                                    {
                                        _2350 = _500.w == 2.0;
                                    }
                                    else
                                    {
                                        _2350 = false;
                                    }
                                    if (_2350)
                                    {
                                        _2410 = true;
                                        break;
                                    }
                                    float3 _2362 = _485.xyz - _942;
                                    float3 _2364 = _500.xyz - _942;
                                    float3 _2365 = (float3((_2125 - Settings.projection.zw) / Settings.projection.xy, 1.0) * _2278) - _942;
                                    float _2366 = dot(_2362, _2362);
                                    float _2367 = dot(_2362, _2364);
                                    float _2368 = dot(_2364, _2364);
                                    float _2369 = dot(_2365, _2362);
                                    float _2370 = dot(_2365, _2364);
                                    float _2373 = (_2366 * _2368) - (_2367 * _2367);
                                    bool _2381;
                                    if (!(isnan(_2373) || isinf(_2373)))
                                    {
                                        _2381 = _2373 <= 9.9999996826552253889678874634872e-21;
                                    }
                                    else
                                    {
                                        _2381 = true;
                                    }
                                    if (_2381)
                                    {
                                        _2410 = false;
                                        break;
                                    }
                                    float2 _2392 = float2((_2368 * _2369) - (_2367 * _2370), (_2366 * _2370) - (_2367 * _2369)) / float2(_2373);
                                    bool2 _2393 = isnan(_2392);
                                    bool2 _2394 = isinf(_2392);
                                    bool _2402;
                                    if (all(not(bool2(_2393.x || _2394.x, _2393.y || _2394.y))))
                                    {
                                        _2402 = all(_2392 >= float2(-9.9999997473787516355514526367188e-06));
                                    }
                                    else
                                    {
                                        _2402 = false;
                                    }
                                    bool _2409;
                                    if (_2402)
                                    {
                                        _2409 = (_2392.x + _2392.y) <= 1.000010013580322265625;
                                    }
                                    else
                                    {
                                        _2409 = false;
                                    }
                                    _2410 = _2409;
                                    break;
                                } while(false);
                                if (!_2410)
                                {
                                    _2589 = float4(0.0);
                                    break;
                                }
                                float3 _2554;
                                bool _2555;
                                do
                                {
                                    bool _2420;
                                    if (_2138)
                                    {
                                        _2420 = any(_2125 < float2(0.0));
                                    }
                                    else
                                    {
                                        _2420 = true;
                                    }
                                    bool _2426;
                                    if (!_2420)
                                    {
                                        _2426 = any(_2125 >= _947);
                                    }
                                    else
                                    {
                                        _2426 = true;
                                    }
                                    if (_2426)
                                    {
                                        _2554 = float3(0.0);
                                        _2555 = false;
                                        break;
                                    }
                                    bool _2433;
                                    if (_586)
                                    {
                                        _2433 = _485.w == 2.0;
                                    }
                                    else
                                    {
                                        _2433 = false;
                                    }
                                    bool _2438;
                                    if (_2433)
                                    {
                                        _2438 = _500.w == 2.0;
                                    }
                                    else
                                    {
                                        _2438 = false;
                                    }
                                    float3 _2448;
                                    if (_2438)
                                    {
                                        _2448 = _485.xyz;
                                    }
                                    else
                                    {
                                        _2448 = cross(_485.xyz - _942, _500.xyz - _942);
                                    }
                                    float3 _2449 = fast::normalize(_2448);
                                    float3 _2457 = fast::normalize(float3((_2125 - Settings.projection.zw) / Settings.projection.xy, 1.0));
                                    float3 _2463;
                                    if (dot(_2449, _2457) > 0.0)
                                    {
                                        _2463 = -_2449;
                                    }
                                    else
                                    {
                                        _2463 = _2449;
                                    }
                                    float _2464 = dot(_2457, _2463);
                                    bool _2473;
                                    if (!(isnan(_2464) || isinf(_2464)))
                                    {
                                        _2473 = abs(_2464) <= 9.9999999600419720025001879548654e-13;
                                    }
                                    else
                                    {
                                        _2473 = true;
                                    }
                                    if (_2473)
                                    {
                                        _2554 = float3(0.0);
                                        _2555 = false;
                                        break;
                                    }
                                    float _2477 = dot(_942, _2463) / _2464;
                                    float _2479 = _2477 * _2457.z;
                                    bool _2487;
                                    if (!(isnan(_2477) || isinf(_2477)))
                                    {
                                        _2487 = _2477 <= 0.0;
                                    }
                                    else
                                    {
                                        _2487 = true;
                                    }
                                    bool _2492;
                                    if (!_2487)
                                    {
                                        _2492 = _2479 < Settings.clip.x;
                                    }
                                    else
                                    {
                                        _2492 = true;
                                    }
                                    bool _2497;
                                    if (!_2492)
                                    {
                                        _2497 = _2479 > Settings.clip.y;
                                    }
                                    else
                                    {
                                        _2497 = true;
                                    }
                                    if (_2497)
                                    {
                                        _2554 = float3(0.0);
                                        _2555 = false;
                                        break;
                                    }
                                    bool _2504;
                                    if (_586)
                                    {
                                        _2504 = _485.w == 2.0;
                                    }
                                    else
                                    {
                                        _2504 = false;
                                    }
                                    bool _2509;
                                    if (_2504)
                                    {
                                        _2509 = _500.w == 2.0;
                                    }
                                    else
                                    {
                                        _2509 = false;
                                    }
                                    float3 _2515 = (_2457 * _2477) + (_2463 * precise::max(_2509 ? 0.04999999701976776123046875 : 0.00999999977648258209228515625, _2477 * 9.9999997473787516355514526367188e-06));
                                    float3 _2516 = reflect(_2457, _2463);
                                    uint _2517 = uint(_516);
                                    float3 _2524 = fast::normalize(cross(_2516, select(float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), bool3(abs(_2516.y) < 0.949999988079071044921875))));
                                    float3 _2536 = fast::normalize(_2516 + (((_2524 * _148[_2517].x) + (cross(_2516, _2524) * _148[_2517].y)) * (Settings.roughness * Settings.roughness)));
                                    float3 _2540 = select(_2536, _2516, bool3(dot(_2536, _2463) <= 0.0));
                                    bool3 _2541 = isnan(_2515);
                                    bool3 _2542 = isinf(_2515);
                                    bool _2553;
                                    if (all(not(bool3(_2541.x || _2542.x, _2541.y || _2542.y, _2541.z || _2542.z))))
                                    {
                                        bool3 _2548 = isnan(_2540);
                                        bool3 _2549 = isinf(_2540);
                                        _2553 = all(not(bool3(_2548.x || _2549.x, _2548.y || _2549.y, _2548.z || _2549.z)));
                                    }
                                    else
                                    {
                                        _2553 = false;
                                    }
                                    _2554 = _2515;
                                    _2555 = _2553;
                                    break;
                                } while(false);
                                if (!_2555)
                                {
                                    _2589 = float4(0.0);
                                    break;
                                }
                                float3 _2561 = precise::max(abs(_2554), abs(_918));
                                float _2569 = length(_918 - _2554);
                                bool _2583;
                                if (!(isnan(_2569) || isinf(_2569)))
                                {
                                    _2583 = ((precise::max(1.0, precise::max(_2561.x, precise::max(_2561.y, _2561.z))) * 9.9999999747524270787835121154785e-07) * precise::max(Settings.projection.x, Settings.projection.y)) > (_2569 * 0.01200000010430812835693359375);
                                }
                                else
                                {
                                    _2583 = true;
                                }
                                if (_2583)
                                {
                                    _2589 = float4(0.0);
                                    break;
                                }
                                _2589 = float4(_2125, _2278, 1.0);
                                break;
                            } while(false);
                            _2590 = _2126;
                            _2591 = _2589;
                            break;
                        }
                        _2590 = _2126;
                        _2591 = float4(0.0);
                        break;
                    } while(false);
                    _2592 = _2591;
                    _2593 = _2590;
                    _2594 = _2591;
                }
                else
                {
                    _2592 = _335;
                    _2593 = _337;
                    _2594 = float4(0.0);
                }
                float3 _2979;
                bool _2980;
                do
                {
                    bool _2604;
                    if ((Settings.flags & 1u) != 0u)
                    {
                        _2604 = Settings.weight == 0.0;
                    }
                    else
                    {
                        _2604 = true;
                    }
                    bool _2610;
                    if (!_2604)
                    {
                        _2610 = _2594.w != 1.0;
                    }
                    else
                    {
                        _2610 = true;
                    }
                    bool _2620;
                    if (!_2610)
                    {
                        bool4 _2614 = isnan(_2594);
                        bool4 _2615 = isinf(_2594);
                        _2620 = !all(not(bool4(_2614.x || _2615.x, _2614.y || _2615.y, _2614.z || _2615.z, _2614.w || _2615.w)));
                    }
                    else
                    {
                        _2620 = true;
                    }
                    bool _2626;
                    if (!_2620)
                    {
                        _2626 = _2594.z <= 0.0;
                    }
                    else
                    {
                        _2626 = true;
                    }
                    if (_2626)
                    {
                        _2979 = float3(0.0);
                        _2980 = false;
                        break;
                    }
                    uint _2633 = (Settings.previousMapping + (_200 * 4u)) >> 2u;
                    uint _2638 = (Settings.previousMapping + (current._m0[_355] * 4u)) >> 2u;
                    bool _2645;
                    if (geometry._m0[_2633] != 4294967295u)
                    {
                        _2645 = geometry._m0[_2638] == 4294967295u;
                    }
                    else
                    {
                        _2645 = true;
                    }
                    if (_2645)
                    {
                        _2979 = float3(0.0);
                        _2980 = false;
                        break;
                    }
                    float2 _2649 = _2594.xy - float2(0.5);
                    float2 _157 = _2649;
                    bool _2662;
                    if (!any(_2649 < float2(0.0)))
                    {
                        _2662 = any(_2649 > float2(float(Settings.previousWidth - 1u), float(Settings.previousHeight - 1u)));
                    }
                    else
                    {
                        _2662 = true;
                    }
                    if (_2662)
                    {
                        _2979 = float3(0.0);
                        _2980 = false;
                        break;
                    }
                    int2 _2666 = int2(floor(_2649));
                    float2 _2668 = _2649 - float2(_2666);
                    float2 _2674 = float2(float(current._m0[_358] & 65535u), float(current._m0[_358] >> 16u)) * float2(1.525902189314365386962890625e-05);
                    uint _2675 = Settings.previousWidth * Settings.previousHeight;
                    float3 _2680;
                    _2680 = float3(0.0);
                    bool _2678;
                    float3 _2681;
                    bool _2685;
                    float3 _2975;
                    bool _2976;
                    bool _2977;
                    bool _2677 = false;
                    uint _2682 = 0u;
                    bool _2684 = _332;
                    for (;;)
                    {
                        if (_2682 < 2u)
                        {
                            _2681 = _2680;
                            float3 _2690;
                            uint _2692 = 0u;
                            for (;;)
                            {
                                if (_2692 < 2u)
                                {
                                    float _2704;
                                    if (_2692 != 0u)
                                    {
                                        _2704 = _2668.x;
                                    }
                                    else
                                    {
                                        _2704 = 1.0 - _2668.x;
                                    }
                                    float _2712;
                                    if (_2682 != 0u)
                                    {
                                        _2712 = _2668.y;
                                    }
                                    else
                                    {
                                        _2712 = 1.0 - _2668.y;
                                    }
                                    float _2713 = _2704 * _2712;
                                    if (_2713 <= 0.0)
                                    {
                                        _2690 = _2681;
                                        uint _2693 = _2692 + 1u;
                                        _2681 = _2690;
                                        _2692 = _2693;
                                        continue;
                                    }
                                    int2 _2726 = min((_2666 + int2(int(_2692), int(_2682))), int2(int(Settings.previousWidth - 1u), int(Settings.previousHeight - 1u)));
                                    int2 _158 = _2726;
                                    uint _2734 = (uint(_158.y) * Settings.previousWidth) + uint(_158.x);
                                    float _2746 = as_type<float>(history._m0[((_2675 * 8u) + (_2734 * 4u)) >> 2u]);
                                    uint _2747 = _2675 * 28u;
                                    uint _2752 = (_2747 + (((_2734 * Settings.lobes) + _343) * 12u)) >> 2u;
                                    uint _2755 = _2752 + 1u;
                                    uint _2758 = _2752 + 2u;
                                    bool _2765;
                                    if (history._m0[(4u * (_2675 + _2734)) >> 2u] == geometry._m0[_2633])
                                    {
                                        _2765 = history._m0[_2752] != geometry._m0[_2638];
                                    }
                                    else
                                    {
                                        _2765 = true;
                                    }
                                    bool _2783;
                                    if (!_2765)
                                    {
                                        bool _2774;
                                        if (history._m0[_2752] != 4294967295u)
                                        {
                                            _2774 = (history._m0[_2758] >> 24u) == 255u;
                                        }
                                        else
                                        {
                                            _2774 = false;
                                        }
                                        bool _2781;
                                        if (_2774)
                                        {
                                            _2781 = ((history._m0[_2755] & 65535u) + (history._m0[_2755] >> 16u)) <= 65535u;
                                        }
                                        else
                                        {
                                            _2781 = false;
                                        }
                                        _2783 = !_2781;
                                    }
                                    else
                                    {
                                        _2783 = true;
                                    }
                                    bool _2790;
                                    if (!_2783)
                                    {
                                        _2790 = isnan(_2746) || isinf(_2746);
                                    }
                                    else
                                    {
                                        _2790 = true;
                                    }
                                    bool _2795;
                                    if (!_2790)
                                    {
                                        _2795 = _2746 <= 0.0;
                                    }
                                    else
                                    {
                                        _2795 = true;
                                    }
                                    bool _2805;
                                    if (!_2795)
                                    {
                                        _2805 = abs(_2746 - _2594.z) > precise::max(0.00999999977648258209228515625, _2594.z * 0.004999999888241291046142578125);
                                    }
                                    else
                                    {
                                        _2805 = true;
                                    }
                                    if (_2805)
                                    {
                                        _2685 = false;
                                        _2678 = true;
                                        break;
                                    }
                                    float2 _2813 = float2(float(history._m0[_2755] & 65535u), float(history._m0[_2755] >> 16u)) * float2(1.525902189314365386962890625e-05);
                                    float2 _2815;
                                    _2815 = float2(0.00203051813878118991851806640625);
                                    float2 _2816;
                                    for (int _2818 = 0; _2818 < 2; _2815 = _2816, _2818++)
                                    {
                                        float2 _2824;
                                        _2824 = float2(0.0);
                                        float2 _2825;
                                        for (int _2827 = -1; _2827 <= 1; _2824 = _2825, _2827 += 2)
                                        {
                                            int2 _159 = _2726;
                                            uint _2832 = uint(_2818);
                                            _159[_2832] += _2827;
                                            bool _2848;
                                            if (!any(_159 < int2(0)))
                                            {
                                                _2848 = any(_159 >= int2(int(Settings.previousWidth), int(Settings.previousHeight)));
                                            }
                                            else
                                            {
                                                _2848 = true;
                                            }
                                            if (_2848)
                                            {
                                                _2825 = _2824;
                                                continue;
                                            }
                                            uint _2858 = (uint(_159.y) * Settings.previousWidth) + uint(_159.x);
                                            uint _2863 = (_2747 + (((_2858 * Settings.lobes) + _343) * 12u)) >> 2u;
                                            uint _2866 = _2863 + 1u;
                                            bool _2881;
                                            if (history._m0[(4u * (_2675 + _2858)) >> 2u] == geometry._m0[_2633])
                                            {
                                                _2881 = history._m0[_2863] == geometry._m0[_2638];
                                            }
                                            else
                                            {
                                                _2881 = false;
                                            }
                                            bool _2897;
                                            if (_2881)
                                            {
                                                bool _2889;
                                                if (history._m0[_2863] != 4294967295u)
                                                {
                                                    _2889 = (history._m0[_2863 + 2u] >> 24u) == 255u;
                                                }
                                                else
                                                {
                                                    _2889 = false;
                                                }
                                                bool _2896;
                                                if (_2889)
                                                {
                                                    _2896 = ((history._m0[_2866] & 65535u) + (history._m0[_2866] >> 16u)) <= 65535u;
                                                }
                                                else
                                                {
                                                    _2896 = false;
                                                }
                                                _2897 = _2896;
                                            }
                                            else
                                            {
                                                _2897 = false;
                                            }
                                            float2 _2910;
                                            if (_2897)
                                            {
                                                _2910 = precise::max(_2824, abs((float2(float(history._m0[_2866] & 65535u), float(history._m0[_2866] >> 16u)) * float2(1.525902189314365386962890625e-05)) - _2813) * 1.5);
                                            }
                                            else
                                            {
                                                _2910 = _2824;
                                            }
                                            _2825 = _2910;
                                        }
                                        uint _2911 = uint(_2818);
                                        _2816 = _2815 + (_2824 * abs(_157[_2911] - float(_158[_2911])));
                                    }
                                    if (any(abs(_2813 - _2674) > precise::min(_2815, float2(0.0500000007450580596923828125))))
                                    {
                                        _2685 = false;
                                        _2678 = true;
                                        break;
                                    }
                                    float3 _2936 = float3(float(history._m0[_2758] & 255u), float((history._m0[_2758] >> 8u) & 255u), float((history._m0[_2758] >> 16u) & 255u)) * float3(0.0039215688593685626983642578125);
                                    float3 _2971;
                                    if (_378)
                                    {
                                        float _2940 = _2936.x;
                                        float _2949;
                                        if (_2940 <= 0.040449999272823333740234375)
                                        {
                                            _2949 = _2940 * 0.077399380505084991455078125;
                                        }
                                        else
                                        {
                                            _2949 = powr((_2940 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                        }
                                        float _2950 = _2936.y;
                                        float _2959;
                                        if (_2950 <= 0.040449999272823333740234375)
                                        {
                                            _2959 = _2950 * 0.077399380505084991455078125;
                                        }
                                        else
                                        {
                                            _2959 = powr((_2950 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                        }
                                        float _2960 = _2936.z;
                                        float _2969;
                                        if (_2960 <= 0.040449999272823333740234375)
                                        {
                                            _2969 = _2960 * 0.077399380505084991455078125;
                                        }
                                        else
                                        {
                                            _2969 = powr((_2960 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                        }
                                        _2971 = float3(_2949, _2959, _2969);
                                    }
                                    else
                                    {
                                        _2971 = _2936;
                                    }
                                    _2690 = _2681 + (_2971 * _2713);
                                    uint _2693 = _2692 + 1u;
                                    _2681 = _2690;
                                    _2692 = _2693;
                                    continue;
                                }
                                else
                                {
                                    _2685 = _2684;
                                    _2678 = _2677;
                                    break;
                                }
                            }
                            if (_2678)
                            {
                                _2975 = _2681;
                                _2976 = _2685;
                                _2977 = _2678;
                                break;
                            }
                            _2677 = _2678;
                            _2680 = _2681;
                            _2682++;
                            _2684 = _2685;
                            continue;
                        }
                        else
                        {
                            _2975 = _2680;
                            _2976 = _2684;
                            _2977 = _2677;
                            break;
                        }
                    }
                    if (_2977)
                    {
                        _2979 = _2975;
                        _2980 = _2976;
                        break;
                    }
                    _2979 = _2975;
                    _2980 = true;
                    break;
                } while(false);
                uint3 _3293;
                float3 _3294;
                if (_2980)
                {
                    float3 _2984;
                    float3 _2987;
                    _2984 = _413;
                    _2987 = _413;
                    float3 _2985;
                    float3 _2988;
                    for (int _2989 = -1; _2989 <= 1; _2984 = _2985, _2987 = _2988, _2989++)
                    {
                        _2985 = _2984;
                        _2988 = _2987;
                        float3 _2995;
                        float3 _2997;
                        for (int _2998 = -1; _2998 <= 1; _2985 = _2995, _2988 = _2997, _2998++)
                        {
                            int2 _3012 = clamp(int2(gl_GlobalInvocationID.xy) + int2(_2998, _2989), int2(0), int2(int(Settings.width - 1u), int(Settings.height - 1u)));
                            uint _3018 = (uint(_3012.y) * Settings.width) + uint(_3012.x);
                            uint _3023 = (_350 + (((_3018 * Settings.lobes) + _343) * 12u)) >> 2u;
                            uint _3026 = _3023 + 1u;
                            uint _3029 = _3023 + 2u;
                            uint _3034 = (4u * (_187 + _3018)) >> 2u;
                            bool _3041;
                            if (current._m0[_3034] == _200)
                            {
                                _3041 = current._m0[_3023] != current._m0[_355];
                            }
                            else
                            {
                                _3041 = true;
                            }
                            bool _3059;
                            if (!_3041)
                            {
                                bool _3050;
                                if (current._m0[_3023] != 4294967295u)
                                {
                                    _3050 = (current._m0[_3029] >> 24u) == 255u;
                                }
                                else
                                {
                                    _3050 = false;
                                }
                                bool _3057;
                                if (_3050)
                                {
                                    _3057 = ((current._m0[_3026] & 65535u) + (current._m0[_3026] >> 16u)) <= 65535u;
                                }
                                else
                                {
                                    _3057 = false;
                                }
                                _3059 = !_3057;
                            }
                            else
                            {
                                _3059 = true;
                            }
                            if (_3059)
                            {
                                _2995 = _2985;
                                _2997 = _2988;
                                continue;
                            }
                            uint _3062 = _3018 * 4u;
                            float _3067 = as_type<float>(current._m0[(_201 + _3062) >> 2u]);
                            uint _3070 = (_207 + (_3018 * 16u)) >> 2u;
                            float4 _3083 = as_type<float4>(uint4(current._m0[_3070], current._m0[_3070 + 1u], current._m0[_3070 + 2u], current._m0[_3070 + 3u]));
                            uint2 _3084 = uint2(_3012);
                            uint _3085 = _3062 >> 2u;
                            float4 _3095 = ownership.read(uint2(int3(int(_3084.x), int(_3084.y), 0).xy), 0);
                            bool _3133;
                            if (_3095.w > 0.5)
                            {
                                bool _3132;
                                do
                                {
                                    uint _3105 = uint(rint(_3095.z * 255.0));
                                    float _3106 = _3095.x;
                                    bool _3115;
                                    if ((isunordered(_3106, 0.0) || _3106 > 0.0))
                                    {
                                        bool _3114;
                                        if (_3105 != 1u)
                                        {
                                            _3114 = _3105 != 2u;
                                        }
                                        else
                                        {
                                            _3114 = false;
                                        }
                                        _3115 = _3114;
                                    }
                                    else
                                    {
                                        _3115 = true;
                                    }
                                    if (_3115)
                                    {
                                        _3132 = false;
                                        break;
                                    }
                                    uint _3118 = current._m0[_3085] >> 24u;
                                    if (_3118 == 253u)
                                    {
                                        _3132 = true;
                                        break;
                                    }
                                    if (_3106 == 0.0039215688593685626983642578125)
                                    {
                                        _3132 = _3118 == 254u;
                                        break;
                                    }
                                    bool _3131;
                                    if (_3118 == 255u)
                                    {
                                        _3131 = _3095.y > 0.0;
                                    }
                                    else
                                    {
                                        _3131 = false;
                                    }
                                    _3132 = _3131;
                                    break;
                                } while(false);
                                _3133 = _3132;
                            }
                            else
                            {
                                _3133 = false;
                            }
                            bool _3138;
                            if (_3133)
                            {
                                _3138 = (current._m0[_3085] >> 24u) == 255u;
                            }
                            else
                            {
                                _3138 = false;
                            }
                            bool _3144;
                            if (_3138)
                            {
                                _3144 = current._m0[_3034] < Settings.triangles;
                            }
                            else
                            {
                                _3144 = false;
                            }
                            bool _3151;
                            if (_3144)
                            {
                                _3151 = !(isnan(_3067) || isinf(_3067));
                            }
                            else
                            {
                                _3151 = false;
                            }
                            bool _3155;
                            if (_3151)
                            {
                                _3155 = _3067 > 0.0;
                            }
                            else
                            {
                                _3155 = false;
                            }
                            bool _3160;
                            if (_3155)
                            {
                                _3160 = _3083.w == 1.0;
                            }
                            else
                            {
                                _3160 = false;
                            }
                            bool _3168;
                            if (_3160)
                            {
                                bool4 _3163 = isnan(_3083);
                                bool4 _3164 = isinf(_3083);
                                _3168 = all(not(bool4(_3163.x || _3164.x, _3163.y || _3164.y, _3163.z || _3164.z, _3163.w || _3164.w)));
                            }
                            else
                            {
                                _3168 = false;
                            }
                            bool _3174;
                            if (_3168)
                            {
                                _3174 = all(_3083.xyz >= float3(0.0));
                            }
                            else
                            {
                                _3174 = false;
                            }
                            bool _3180;
                            if (_3174)
                            {
                                _3180 = all(_3083.xyz <= float3(1.0));
                            }
                            else
                            {
                                _3180 = false;
                            }
                            bool _3186;
                            if (_3180)
                            {
                                _3186 = any(_3083.xyz > float3(0.0));
                            }
                            else
                            {
                                _3186 = false;
                            }
                            if (!_3186)
                            {
                                _2995 = _2985;
                                _2997 = _2988;
                                continue;
                            }
                            float3 _3199 = float3(float(current._m0[_3029] & 255u), float((current._m0[_3029] >> 8u) & 255u), float((current._m0[_3029] >> 16u) & 255u)) * float3(0.0039215688593685626983642578125);
                            float3 _3234;
                            if (_378)
                            {
                                float _3203 = _3199.x;
                                float _3212;
                                if (_3203 <= 0.040449999272823333740234375)
                                {
                                    _3212 = _3203 * 0.077399380505084991455078125;
                                }
                                else
                                {
                                    _3212 = powr((_3203 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                }
                                float _3213 = _3199.y;
                                float _3222;
                                if (_3213 <= 0.040449999272823333740234375)
                                {
                                    _3222 = _3213 * 0.077399380505084991455078125;
                                }
                                else
                                {
                                    _3222 = powr((_3213 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                }
                                float _3223 = _3199.z;
                                float _3232;
                                if (_3223 <= 0.040449999272823333740234375)
                                {
                                    _3232 = _3223 * 0.077399380505084991455078125;
                                }
                                else
                                {
                                    _3232 = powr((_3223 + 0.054999999701976776123046875) * 0.947867333889007568359375, 2.400000095367431640625);
                                }
                                _3234 = float3(_3212, _3222, _3232);
                            }
                            else
                            {
                                _3234 = _3199;
                            }
                            _2995 = precise::max(_2985, _3234);
                            _2997 = precise::min(_2988, _3234);
                        }
                    }
                    float3 _3241 = mix(_413, fast::clamp(_2979, _2987, _2984), float3(Settings.weight));
                    float3 _3279;
                    if (_378)
                    {
                        float3 _3246 = fast::clamp(_3241, float3(0.0), float3(1.0));
                        float _3247 = _3246.x;
                        float _3256;
                        if (_3247 <= 0.003130800090730190277099609375)
                        {
                            _3256 = _3247 * 12.9200000762939453125;
                        }
                        else
                        {
                            _3256 = (1.05499994754791259765625 * powr(_3247, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                        }
                        float _3257 = _3246.y;
                        float _3266;
                        if (_3257 <= 0.003130800090730190277099609375)
                        {
                            _3266 = _3257 * 12.9200000762939453125;
                        }
                        else
                        {
                            _3266 = (1.05499994754791259765625 * powr(_3257, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                        }
                        float _3267 = _3246.z;
                        float _3276;
                        if (_3267 <= 0.003130800090730190277099609375)
                        {
                            _3276 = _3267 * 12.9200000762939453125;
                        }
                        else
                        {
                            _3276 = (1.05499994754791259765625 * powr(_3267, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                        }
                        _3279 = float3(_3256, _3266, _3276);
                    }
                    else
                    {
                        _3279 = fast::clamp(_3241, float3(0.0), float3(1.0));
                    }
                    uint3 _3283 = uint3(floor((_3279 * 255.0) + float3(0.5)));
                    uint3 _3292 = _364;
                    _3292.z = ((_3283.x | (_3283.y << 8u)) | (_3283.z << 16u)) | (current._m0[_361] & 4278190080u);
                    _3293 = _3292;
                    _3294 = _3241;
                }
                else
                {
                    _3293 = _364;
                    _3294 = _413;
                }
                _3296 = _3293;
                _333 = _2980;
                _336 = _2592;
                _338 = _2593;
                _342 = _2980 ? true : _341;
                _3297 = _3294;
            }
            else
            {
                _3296 = _364;
                _333 = _332;
                _336 = _335;
                _338 = _337;
                _342 = _341;
                _3297 = _413;
            }
            resolved._m0[_355] = _3296.x;
            resolved._m0[_358] = _3296.y;
            resolved._m0[_361] = _3296.z;
            _340 = _339 + _3297;
        }
        if (_341)
        {
            float3 _3310 = (_339 / float3(float(Settings.lobes))) * _223.xyz;
            float3 _3352;
            if ((Settings.flags & 2u) != 0u)
            {
                float3 _3319 = fast::clamp(_3310, float3(0.0), float3(1.0));
                float _3320 = _3319.x;
                float _3329;
                if (_3320 <= 0.003130800090730190277099609375)
                {
                    _3329 = _3320 * 12.9200000762939453125;
                }
                else
                {
                    _3329 = (1.05499994754791259765625 * powr(_3320, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                float _3330 = _3319.y;
                float _3339;
                if (_3330 <= 0.003130800090730190277099609375)
                {
                    _3339 = _3330 * 12.9200000762939453125;
                }
                else
                {
                    _3339 = (1.05499994754791259765625 * powr(_3330, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                float _3340 = _3319.z;
                float _3349;
                if (_3340 <= 0.003130800090730190277099609375)
                {
                    _3349 = _3340 * 12.9200000762939453125;
                }
                else
                {
                    _3349 = (1.05499994754791259765625 * powr(_3340, 0.4166666567325592041015625)) - 0.054999999701976776123046875;
                }
                _3352 = float3(_3329, _3339, _3349);
            }
            else
            {
                _3352 = fast::clamp(_3310, float3(0.0), float3(1.0));
            }
            uint3 _3356 = uint3(floor((_3352 * 255.0) + float3(0.5)));
            resolved._m0[_193] = ((_3356.x | (_3356.y << 8u)) | (_3356.z << 16u)) | (_195 & 4278190080u);
        }
        break;
    } while(false);
}

