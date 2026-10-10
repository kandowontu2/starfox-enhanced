#pragma clang diagnostic ignored "-Wmissing-prototypes"

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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

struct type_FrameSettings
{
    uint oldTriangles;
    uint previousVertices;
    uint previousMapping;
    uint geometryBytes;
    uint currentWidth;
    uint currentHeight;
    uint recordPrefix;
    uint pathStride;
    uint lobes;
    uint queryFirst;
    uint queryCount;
    uint pathFlags;
    float acceptedRoughness;
    uint frameUnused0;
    uint frameUnused1;
    uint frameUnused2;
    float4 projection;
    float4 extentClip;
    float4 oldPoint;
    float4 oldNormal;
    float4 oldLiquid[8];
};

constant float4 _311 = {};

kernel void feature_frames_main(device type_ByteAddressBuffer& queries [[buffer(0)]], device type_ByteAddressBuffer& geometry [[buffer(1)]], device type_ByteAddressBuffer& current [[buffer(2)]], device type_RWByteAddressBuffer& frames [[buffer(3)]], constant type_FrameSettings& FrameSettings [[buffer(4)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        if (gl_GlobalInvocationID.x >= FrameSettings.queryCount)
        {
            break;
        }
        uint _14 = gl_GlobalInvocationID.x * 512u;
        uint _15 = gl_GlobalInvocationID.x * 48u;
        uint _345 = _14 >> 2u;
        frames._m0[_345] = 0u;
        uint _16 = _345 + 1u;
        frames._m0[_16] = 0u;
        uint _17 = _345 + 2u;
        frames._m0[_17] = 0u;
        uint _18 = _345 + 3u;
        frames._m0[_18] = 0u;
        uint _350 = (_14 + 16u) >> 2u;
        frames._m0[_350] = 0u;
        uint _106 = _350 + 1u;
        frames._m0[_106] = 0u;
        uint _107 = _350 + 2u;
        frames._m0[_107] = 0u;
        uint _108 = _350 + 3u;
        frames._m0[_108] = 0u;
        uint _355 = (_14 + 32u) >> 2u;
        frames._m0[_355] = 0u;
        uint _110 = _355 + 1u;
        frames._m0[_110] = 0u;
        uint _111 = _355 + 2u;
        frames._m0[_111] = 0u;
        uint _112 = _355 + 3u;
        frames._m0[_112] = 0u;
        uint _360 = (_14 + 48u) >> 2u;
        frames._m0[_360] = 0u;
        uint _114 = _360 + 1u;
        frames._m0[_114] = 0u;
        uint _115 = _360 + 2u;
        frames._m0[_115] = 0u;
        uint _116 = _360 + 3u;
        frames._m0[_116] = 0u;
        uint _365 = (_14 + 64u) >> 2u;
        frames._m0[_365] = 0u;
        uint _118 = _365 + 1u;
        frames._m0[_118] = 0u;
        uint _119 = _365 + 2u;
        frames._m0[_119] = 0u;
        uint _120 = _365 + 3u;
        frames._m0[_120] = 0u;
        uint _370 = (_14 + 80u) >> 2u;
        frames._m0[_370] = 0u;
        uint _122 = _370 + 1u;
        frames._m0[_122] = 0u;
        uint _123 = _370 + 2u;
        frames._m0[_123] = 0u;
        uint _124 = _370 + 3u;
        frames._m0[_124] = 0u;
        uint _125 = _14 + 96u;
        uint _375 = _125 >> 2u;
        frames._m0[_375] = 0u;
        frames._m0[_375 + 1u] = 0u;
        frames._m0[_375 + 2u] = 0u;
        frames._m0[_375 + 3u] = 0u;
        uint _380 = (_14 + 112u) >> 2u;
        frames._m0[_380] = 0u;
        frames._m0[_380 + 1u] = 0u;
        frames._m0[_380 + 2u] = 0u;
        frames._m0[_380 + 3u] = 0u;
        uint _385 = (_14 + 128u) >> 2u;
        frames._m0[_385] = 0u;
        frames._m0[_385 + 1u] = 0u;
        frames._m0[_385 + 2u] = 0u;
        frames._m0[_385 + 3u] = 0u;
        uint _390 = (_14 + 144u) >> 2u;
        frames._m0[_390] = 0u;
        frames._m0[_390 + 1u] = 0u;
        frames._m0[_390 + 2u] = 0u;
        frames._m0[_390 + 3u] = 0u;
        uint _395 = (_14 + 160u) >> 2u;
        frames._m0[_395] = 0u;
        frames._m0[_395 + 1u] = 0u;
        frames._m0[_395 + 2u] = 0u;
        frames._m0[_395 + 3u] = 0u;
        uint _400 = (_14 + 176u) >> 2u;
        frames._m0[_400] = 0u;
        frames._m0[_400 + 1u] = 0u;
        frames._m0[_400 + 2u] = 0u;
        frames._m0[_400 + 3u] = 0u;
        uint _405 = (_14 + 192u) >> 2u;
        frames._m0[_405] = 0u;
        frames._m0[_405 + 1u] = 0u;
        frames._m0[_405 + 2u] = 0u;
        frames._m0[_405 + 3u] = 0u;
        uint _410 = (_14 + 208u) >> 2u;
        frames._m0[_410] = 0u;
        frames._m0[_410 + 1u] = 0u;
        frames._m0[_410 + 2u] = 0u;
        frames._m0[_410 + 3u] = 0u;
        uint _415 = (_14 + 224u) >> 2u;
        frames._m0[_415] = 0u;
        frames._m0[_415 + 1u] = 0u;
        frames._m0[_415 + 2u] = 0u;
        frames._m0[_415 + 3u] = 0u;
        uint _420 = (_14 + 240u) >> 2u;
        frames._m0[_420] = 0u;
        frames._m0[_420 + 1u] = 0u;
        frames._m0[_420 + 2u] = 0u;
        frames._m0[_420 + 3u] = 0u;
        uint _425 = (_14 + 256u) >> 2u;
        frames._m0[_425] = 0u;
        frames._m0[_425 + 1u] = 0u;
        frames._m0[_425 + 2u] = 0u;
        frames._m0[_425 + 3u] = 0u;
        uint _430 = (_14 + 272u) >> 2u;
        frames._m0[_430] = 0u;
        frames._m0[_430 + 1u] = 0u;
        frames._m0[_430 + 2u] = 0u;
        frames._m0[_430 + 3u] = 0u;
        uint _435 = (_14 + 288u) >> 2u;
        frames._m0[_435] = 0u;
        uint _174 = _435 + 1u;
        frames._m0[_174] = 0u;
        uint _175 = _435 + 2u;
        frames._m0[_175] = 0u;
        uint _176 = _435 + 3u;
        frames._m0[_176] = 0u;
        uint _440 = (_14 + 304u) >> 2u;
        frames._m0[_440] = 0u;
        uint _178 = _440 + 1u;
        frames._m0[_178] = 0u;
        uint _179 = _440 + 2u;
        frames._m0[_179] = 0u;
        uint _180 = _440 + 3u;
        frames._m0[_180] = 0u;
        uint _445 = (_14 + 320u) >> 2u;
        frames._m0[_445] = 0u;
        uint _182 = _445 + 1u;
        frames._m0[_182] = 0u;
        uint _183 = _445 + 2u;
        frames._m0[_183] = 0u;
        uint _184 = _445 + 3u;
        frames._m0[_184] = 0u;
        uint _450 = (_14 + 336u) >> 2u;
        frames._m0[_450] = 0u;
        uint _186 = _450 + 1u;
        frames._m0[_186] = 0u;
        uint _187 = _450 + 2u;
        frames._m0[_187] = 0u;
        uint _188 = _450 + 3u;
        frames._m0[_188] = 0u;
        uint _455 = (_14 + 352u) >> 2u;
        frames._m0[_455] = 0u;
        uint _190 = _455 + 1u;
        frames._m0[_190] = 0u;
        uint _191 = _455 + 2u;
        frames._m0[_191] = 0u;
        uint _192 = _455 + 3u;
        frames._m0[_192] = 0u;
        uint _460 = (_14 + 368u) >> 2u;
        frames._m0[_460] = 0u;
        uint _194 = _460 + 1u;
        frames._m0[_194] = 0u;
        uint _195 = _460 + 2u;
        frames._m0[_195] = 0u;
        uint _196 = _460 + 3u;
        frames._m0[_196] = 0u;
        uint _465 = (_14 + 384u) >> 2u;
        frames._m0[_465] = 0u;
        uint _198 = _465 + 1u;
        frames._m0[_198] = 0u;
        uint _199 = _465 + 2u;
        frames._m0[_199] = 0u;
        uint _200 = _465 + 3u;
        frames._m0[_200] = 0u;
        uint _470 = (_14 + 400u) >> 2u;
        frames._m0[_470] = 0u;
        uint _202 = _470 + 1u;
        frames._m0[_202] = 0u;
        uint _203 = _470 + 2u;
        frames._m0[_203] = 0u;
        uint _204 = _470 + 3u;
        frames._m0[_204] = 0u;
        uint _475 = (_14 + 416u) >> 2u;
        frames._m0[_475] = 0u;
        uint _206 = _475 + 1u;
        frames._m0[_206] = 0u;
        uint _207 = _475 + 2u;
        frames._m0[_207] = 0u;
        uint _208 = _475 + 3u;
        frames._m0[_208] = 0u;
        uint _480 = (_14 + 432u) >> 2u;
        frames._m0[_480] = 0u;
        uint _210 = _480 + 1u;
        frames._m0[_210] = 0u;
        uint _211 = _480 + 2u;
        frames._m0[_211] = 0u;
        uint _212 = _480 + 3u;
        frames._m0[_212] = 0u;
        uint _485 = (_14 + 448u) >> 2u;
        frames._m0[_485] = 0u;
        uint _214 = _485 + 1u;
        frames._m0[_214] = 0u;
        uint _215 = _485 + 2u;
        frames._m0[_215] = 0u;
        uint _216 = _485 + 3u;
        frames._m0[_216] = 0u;
        uint _490 = (_14 + 464u) >> 2u;
        frames._m0[_490] = 0u;
        uint _218 = _490 + 1u;
        frames._m0[_218] = 0u;
        uint _219 = _490 + 2u;
        frames._m0[_219] = 0u;
        uint _220 = _490 + 3u;
        frames._m0[_220] = 0u;
        uint _495 = (_14 + 480u) >> 2u;
        frames._m0[_495] = 0u;
        uint _222 = _495 + 1u;
        frames._m0[_222] = 0u;
        uint _223 = _495 + 2u;
        frames._m0[_223] = 0u;
        uint _224 = _495 + 3u;
        frames._m0[_224] = 0u;
        uint _500 = (_14 + 496u) >> 2u;
        frames._m0[_500] = 0u;
        uint _226 = _500 + 1u;
        frames._m0[_226] = 0u;
        uint _227 = _500 + 2u;
        frames._m0[_227] = 0u;
        uint _228 = _500 + 3u;
        frames._m0[_228] = 0u;
        uint _505 = _15 >> 2u;
        uint _507 = queries._m0[_505];
        uint _508 = (_15 + 20u) >> 2u;
        uint _510 = queries._m0[_508];
        uint _511 = (_15 + 24u) >> 2u;
        uint _513 = queries._m0[_511];
        uint _514 = (_15 + 44u) >> 2u;
        uint _516 = queries._m0[_514];
        uint _517 = _510 & 7u;
        uint _519 = (_510 >> 4u) & 15u;
        uint _520 = _510 >> 8u;
        bool _527;
        if (_507 != 4294967295u)
        {
            _527 = _516 >= FrameSettings.lobes;
        }
        else
        {
            _527 = true;
        }
        bool _546;
        if (!_527)
        {
            bool _536;
            if (_517 <= 4u)
            {
                _536 = (_519 >> _517) == 0u;
            }
            else
            {
                _536 = false;
            }
            bool _540;
            if (_536)
            {
                _540 = _520 >= 1u;
            }
            else
            {
                _540 = false;
            }
            bool _544;
            if (_540)
            {
                _544 = _520 <= 3u;
            }
            else
            {
                _544 = false;
            }
            _546 = !_544;
        }
        else
        {
            _546 = true;
        }
        bool _555;
        if (!_546)
        {
            _555 = _510 != (((_520 << 8u) | (_519 << 4u)) | _517);
        }
        else
        {
            _555 = true;
        }
        bool _566;
        if (!_555)
        {
            bool _565;
            if (_520 == 3u)
            {
                _565 = _517 != 4u;
            }
            else
            {
                _565 = _517 == 4u;
            }
            _566 = _565;
        }
        else
        {
            _566 = true;
        }
        if (_566)
        {
            break;
        }
        uint _579 = ((((FrameSettings.currentWidth * FrameSettings.currentHeight) * FrameSettings.recordPrefix) + ((FrameSettings.queryFirst + gl_GlobalInvocationID.x) * FrameSettings.pathStride)) + 24u) >> 2u;
        uint _581 = current._m0[_579];
        uint _583 = current._m0[_579 + 1u];
        uint _585 = current._m0[_579 + 2u];
        float3 _587 = as_type<float3>(uint3(_581, _583, _585));
        bool _625;
        do
        {
            bool3 _590 = isnan(_587);
            bool3 _591 = isinf(_587);
            if (!all(not(bool3(_590.x || _591.x, _590.y || _591.y, _590.z || _591.z))))
            {
                _625 = false;
                break;
            }
            if (_520 == 1u)
            {
                bool _608;
                if (_587.z == 0.0)
                {
                    _608 = all(_587.xy >= float2(0.0));
                }
                else
                {
                    _608 = false;
                }
                bool _614;
                if (_608)
                {
                    _614 = spvFAdd(_587.x, _587.y) <= 1.0;
                }
                else
                {
                    _614 = false;
                }
                _625 = _614;
                break;
            }
            bool _619;
            if (_520 != 2u)
            {
                _619 = _520 == 3u;
            }
            else
            {
                _619 = true;
            }
            bool _624;
            if (_619)
            {
                _624 = abs(spvFSub(dot(_587, _587), 1.0)) < 0.00010099999781232327222824096679688;
            }
            else
            {
                _624 = false;
            }
            _625 = _624;
            break;
        } while(false);
        if (!_625)
        {
            break;
        }
        bool _629 = _507 == 4294967293u;
        float4 _887;
        float4 _888;
        float4 _889;
        if (_629)
        {
            if ((FrameSettings.pathFlags & 4u) == 0u)
            {
                break;
            }
            _887 = _311;
            _888 = _311;
            _889 = _311;
        }
        else
        {
            float4 _865;
            float4 _866;
            float4 _867;
            bool _868;
            do
            {
                float4 _742;
                float4 _743;
                float4 _744;
                if (_507 == 4294967294u)
                {
                    bool _654;
                    if ((FrameSettings.pathFlags & 1u) != 0u)
                    {
                        _654 = FrameSettings.oldPoint.w != 1.0;
                    }
                    else
                    {
                        _654 = true;
                    }
                    bool _661;
                    if (!_654)
                    {
                        _661 = FrameSettings.oldNormal.w != 0.0;
                    }
                    else
                    {
                        _661 = true;
                    }
                    if (_661)
                    {
                        _865 = float4(0.0);
                        _866 = float4(0.0);
                        _867 = float4(0.0);
                        _868 = false;
                        break;
                    }
                    _742 = float4(FrameSettings.oldPoint.xyz, 2.0);
                    _743 = float4(FrameSettings.oldNormal.xyz, 2.0);
                    _744 = float4(0.0, 0.0, 0.0, 2.0);
                }
                else
                {
                    bool _686;
                    if (_507 < FrameSettings.oldTriangles)
                    {
                        _686 = FrameSettings.previousVertices > FrameSettings.previousMapping;
                    }
                    else
                    {
                        _686 = true;
                    }
                    bool _695;
                    if (!_686)
                    {
                        _695 = FrameSettings.previousMapping > FrameSettings.geometryBytes;
                    }
                    else
                    {
                        _695 = true;
                    }
                    bool _704;
                    if (!_695)
                    {
                        _704 = _507 >= ((FrameSettings.previousMapping - FrameSettings.previousVertices) / 48u);
                    }
                    else
                    {
                        _704 = true;
                    }
                    if (_704)
                    {
                        _865 = float4(0.0);
                        _866 = float4(0.0);
                        _867 = float4(0.0);
                        _868 = false;
                        break;
                    }
                    uint _43 = FrameSettings.previousVertices + (_507 * 48u);
                    uint _709 = _43 >> 2u;
                    uint _720 = (_43 + 16u) >> 2u;
                    uint _731 = (_43 + 32u) >> 2u;
                    _742 = as_type<float4>(uint4(geometry._m0[_709], geometry._m0[_709 + 1u], geometry._m0[_709 + 2u], geometry._m0[_709 + 3u]));
                    _743 = as_type<float4>(uint4(geometry._m0[_720], geometry._m0[_720 + 1u], geometry._m0[_720 + 2u], geometry._m0[_720 + 3u]));
                    _744 = as_type<float4>(uint4(geometry._m0[_731], geometry._m0[_731 + 1u], geometry._m0[_731 + 2u], geometry._m0[_731 + 3u]));
                }
                bool _864;
                do
                {
                    bool _753;
                    if (_742.w == 2.0)
                    {
                        _753 = _743.w == 2.0;
                    }
                    else
                    {
                        _753 = false;
                    }
                    bool _758;
                    if (_753)
                    {
                        _758 = _744.w == 2.0;
                    }
                    else
                    {
                        _758 = false;
                    }
                    bool _774;
                    if (!_758)
                    {
                        bool _767;
                        if ((isunordered(_742.w, 1.0) || _742.w == 1.0))
                        {
                            _767 = _743.w != 1.0;
                        }
                        else
                        {
                            _767 = true;
                        }
                        bool _773;
                        if (!_767)
                        {
                            _773 = _744.w != 1.0;
                        }
                        else
                        {
                            _773 = true;
                        }
                        _774 = _773;
                    }
                    else
                    {
                        _774 = false;
                    }
                    bool _784;
                    if (!_774)
                    {
                        bool4 _778 = isnan(_742);
                        bool4 _779 = isinf(_742);
                        _784 = !all(not(bool4(_778.x || _779.x, _778.y || _779.y, _778.z || _779.z, _778.w || _779.w)));
                    }
                    else
                    {
                        _784 = true;
                    }
                    bool _794;
                    if (!_784)
                    {
                        bool4 _788 = isnan(_743);
                        bool4 _789 = isinf(_743);
                        _794 = !all(not(bool4(_788.x || _789.x, _788.y || _789.y, _788.z || _789.z, _788.w || _789.w)));
                    }
                    else
                    {
                        _794 = true;
                    }
                    bool _804;
                    if (!_794)
                    {
                        bool4 _798 = isnan(_744);
                        bool4 _799 = isinf(_744);
                        _804 = !all(not(bool4(_798.x || _799.x, _798.y || _799.y, _798.z || _799.z, _798.w || _799.w)));
                    }
                    else
                    {
                        _804 = true;
                    }
                    bool _812;
                    if (!_804)
                    {
                        _812 = any(abs(_742.xyz) > float3(999999995904.0));
                    }
                    else
                    {
                        _812 = true;
                    }
                    bool _820;
                    if (!_812)
                    {
                        _820 = any(abs(_743.xyz) > float3(999999995904.0));
                    }
                    else
                    {
                        _820 = true;
                    }
                    bool _828;
                    if (!_820)
                    {
                        _828 = any(abs(_744.xyz) > float3(999999995904.0));
                    }
                    else
                    {
                        _828 = true;
                    }
                    if (_828)
                    {
                        _864 = false;
                        break;
                    }
                    bool _836;
                    if (_758)
                    {
                        _836 = any(_744.xyz != float3(0.0));
                    }
                    else
                    {
                        _836 = false;
                    }
                    if (_836)
                    {
                        _864 = false;
                        break;
                    }
                    float3 _847;
                    if (_758)
                    {
                        _847 = _743.xyz;
                    }
                    else
                    {
                        _847 = cross(spvFSub(_743.xyz, _742.xyz), spvFSub(_744.xyz, _742.xyz));
                    }
                    float _57 = dot(_847, _847);
                    bool3 _848 = isnan(_847);
                    bool3 _849 = isinf(_847);
                    bool _859;
                    if (all(not(bool3(_848.x || _849.x, _848.y || _849.y, _848.z || _849.z))))
                    {
                        _859 = !(isnan(_57) || isinf(_57));
                    }
                    else
                    {
                        _859 = false;
                    }
                    bool _863;
                    if (_859)
                    {
                        _863 = _57 > 9.9999996826552253889678874634872e-21;
                    }
                    else
                    {
                        _863 = false;
                    }
                    _864 = _863;
                    break;
                } while(false);
                _865 = _742;
                _866 = _743;
                _867 = _744;
                _868 = _864;
                break;
            } while(false);
            if (!_868)
            {
                break;
            }
            uint4 _872 = as_type<uint4>(_865);
            frames._m0[_345] = _872.x;
            frames._m0[_16] = _872.y;
            frames._m0[_17] = _872.z;
            frames._m0[_18] = _872.w;
            uint4 _877 = as_type<uint4>(_866);
            frames._m0[_350] = _877.x;
            frames._m0[_106] = _877.y;
            frames._m0[_107] = _877.z;
            frames._m0[_108] = _877.w;
            uint4 _882 = as_type<uint4>(_867);
            frames._m0[_355] = _882.x;
            frames._m0[_110] = _882.y;
            frames._m0[_111] = _882.z;
            frames._m0[_112] = _882.w;
            _887 = _865;
            _888 = _866;
            _889 = _867;
        }
        uint4 _892 = as_type<uint4>(FrameSettings.projection);
        frames._m0[_360] = _892.x;
        frames._m0[_114] = _892.y;
        frames._m0[_115] = _892.z;
        frames._m0[_116] = _892.w;
        uint4 _899 = as_type<uint4>(FrameSettings.extentClip);
        frames._m0[_365] = _899.x;
        frames._m0[_118] = _899.y;
        frames._m0[_119] = _899.z;
        frames._m0[_120] = _899.w;
        uint4 _908 = as_type<uint4>(float4(FrameSettings.acceptedRoughness, float(_516), 0.0, 0.0));
        frames._m0[_370] = _908.x;
        frames._m0[_122] = _908.y;
        frames._m0[_123] = _908.z;
        frames._m0[_124] = _908.w;
        uint _913 = (_15 + 4u) >> 2u;
        uint4 _335 = uint4(queries._m0[_913], queries._m0[_913 + 1u], queries._m0[_913 + 2u], queries._m0[_913 + 3u]);
        float4 _926;
        float4 _928;
        float4 _930;
        _926 = _887;
        _928 = _888;
        _930 = _889;
        float4 _927;
        float4 _929;
        float4 _931;
        float4 _1231;
        float4 _1232;
        float4 _1233;
        bool _1234;
        uint _924 = 0u;
        for (;;)
        {
            if (_924 < 4u)
            {
                if (_924 >= _517)
                {
                    if (_335[_924] != 4294967295u)
                    {
                        _1231 = _926;
                        _1232 = _928;
                        _1233 = _930;
                        _1234 = true;
                        break;
                    }
                    _927 = _926;
                    _929 = _928;
                    _931 = _930;
                }
                else
                {
                    float4 _1228;
                    float4 _1229;
                    float4 _1230;
                    if ((_519 & (1u << (_924 & 31u))) != 0u)
                    {
                        bool _960;
                        if ((FrameSettings.pathFlags & 2u) != 0u)
                        {
                            _960 = _335[_924] != 4294967293u;
                        }
                        else
                        {
                            _960 = true;
                        }
                        if (_960)
                        {
                            _1231 = _926;
                            _1232 = _928;
                            _1233 = _930;
                            _1234 = true;
                            break;
                        }
                        _1228 = _926;
                        _1229 = _928;
                        _1230 = _930;
                    }
                    else
                    {
                        float4 _1191;
                        float4 _1192;
                        float4 _1193;
                        bool _1194;
                        do
                        {
                            float4 _1068;
                            float4 _1069;
                            float4 _1070;
                            if (_335[_924] == 4294967294u)
                            {
                                bool _980;
                                if ((FrameSettings.pathFlags & 1u) != 0u)
                                {
                                    _980 = FrameSettings.oldPoint.w != 1.0;
                                }
                                else
                                {
                                    _980 = true;
                                }
                                bool _987;
                                if (!_980)
                                {
                                    _987 = FrameSettings.oldNormal.w != 0.0;
                                }
                                else
                                {
                                    _987 = true;
                                }
                                if (_987)
                                {
                                    _1191 = float4(0.0);
                                    _1192 = float4(0.0);
                                    _1193 = float4(0.0);
                                    _1194 = false;
                                    break;
                                }
                                _1068 = float4(FrameSettings.oldPoint.xyz, 2.0);
                                _1069 = float4(FrameSettings.oldNormal.xyz, 2.0);
                                _1070 = float4(0.0, 0.0, 0.0, 2.0);
                            }
                            else
                            {
                                bool _1012;
                                if (_335[_924] < FrameSettings.oldTriangles)
                                {
                                    _1012 = FrameSettings.previousVertices > FrameSettings.previousMapping;
                                }
                                else
                                {
                                    _1012 = true;
                                }
                                bool _1021;
                                if (!_1012)
                                {
                                    _1021 = FrameSettings.previousMapping > FrameSettings.geometryBytes;
                                }
                                else
                                {
                                    _1021 = true;
                                }
                                bool _1030;
                                if (!_1021)
                                {
                                    _1030 = _335[_924] >= ((FrameSettings.previousMapping - FrameSettings.previousVertices) / 48u);
                                }
                                else
                                {
                                    _1030 = true;
                                }
                                if (_1030)
                                {
                                    _1191 = float4(0.0);
                                    _1192 = float4(0.0);
                                    _1193 = float4(0.0);
                                    _1194 = false;
                                    break;
                                }
                                uint _61 = FrameSettings.previousVertices + (_335[_924] * 48u);
                                uint _1035 = _61 >> 2u;
                                uint _1046 = (_61 + 16u) >> 2u;
                                uint _1057 = (_61 + 32u) >> 2u;
                                _1068 = as_type<float4>(uint4(geometry._m0[_1035], geometry._m0[_1035 + 1u], geometry._m0[_1035 + 2u], geometry._m0[_1035 + 3u]));
                                _1069 = as_type<float4>(uint4(geometry._m0[_1046], geometry._m0[_1046 + 1u], geometry._m0[_1046 + 2u], geometry._m0[_1046 + 3u]));
                                _1070 = as_type<float4>(uint4(geometry._m0[_1057], geometry._m0[_1057 + 1u], geometry._m0[_1057 + 2u], geometry._m0[_1057 + 3u]));
                            }
                            bool _1190;
                            do
                            {
                                bool _1079;
                                if (_1068.w == 2.0)
                                {
                                    _1079 = _1069.w == 2.0;
                                }
                                else
                                {
                                    _1079 = false;
                                }
                                bool _1084;
                                if (_1079)
                                {
                                    _1084 = _1070.w == 2.0;
                                }
                                else
                                {
                                    _1084 = false;
                                }
                                bool _1100;
                                if (!_1084)
                                {
                                    bool _1093;
                                    if ((isunordered(_1068.w, 1.0) || _1068.w == 1.0))
                                    {
                                        _1093 = _1069.w != 1.0;
                                    }
                                    else
                                    {
                                        _1093 = true;
                                    }
                                    bool _1099;
                                    if (!_1093)
                                    {
                                        _1099 = _1070.w != 1.0;
                                    }
                                    else
                                    {
                                        _1099 = true;
                                    }
                                    _1100 = _1099;
                                }
                                else
                                {
                                    _1100 = false;
                                }
                                bool _1110;
                                if (!_1100)
                                {
                                    bool4 _1104 = isnan(_1068);
                                    bool4 _1105 = isinf(_1068);
                                    _1110 = !all(not(bool4(_1104.x || _1105.x, _1104.y || _1105.y, _1104.z || _1105.z, _1104.w || _1105.w)));
                                }
                                else
                                {
                                    _1110 = true;
                                }
                                bool _1120;
                                if (!_1110)
                                {
                                    bool4 _1114 = isnan(_1069);
                                    bool4 _1115 = isinf(_1069);
                                    _1120 = !all(not(bool4(_1114.x || _1115.x, _1114.y || _1115.y, _1114.z || _1115.z, _1114.w || _1115.w)));
                                }
                                else
                                {
                                    _1120 = true;
                                }
                                bool _1130;
                                if (!_1120)
                                {
                                    bool4 _1124 = isnan(_1070);
                                    bool4 _1125 = isinf(_1070);
                                    _1130 = !all(not(bool4(_1124.x || _1125.x, _1124.y || _1125.y, _1124.z || _1125.z, _1124.w || _1125.w)));
                                }
                                else
                                {
                                    _1130 = true;
                                }
                                bool _1138;
                                if (!_1130)
                                {
                                    _1138 = any(abs(_1068.xyz) > float3(999999995904.0));
                                }
                                else
                                {
                                    _1138 = true;
                                }
                                bool _1146;
                                if (!_1138)
                                {
                                    _1146 = any(abs(_1069.xyz) > float3(999999995904.0));
                                }
                                else
                                {
                                    _1146 = true;
                                }
                                bool _1154;
                                if (!_1146)
                                {
                                    _1154 = any(abs(_1070.xyz) > float3(999999995904.0));
                                }
                                else
                                {
                                    _1154 = true;
                                }
                                if (_1154)
                                {
                                    _1190 = false;
                                    break;
                                }
                                bool _1162;
                                if (_1084)
                                {
                                    _1162 = any(_1070.xyz != float3(0.0));
                                }
                                else
                                {
                                    _1162 = false;
                                }
                                if (_1162)
                                {
                                    _1190 = false;
                                    break;
                                }
                                float3 _1173;
                                if (_1084)
                                {
                                    _1173 = _1069.xyz;
                                }
                                else
                                {
                                    _1173 = cross(spvFSub(_1069.xyz, _1068.xyz), spvFSub(_1070.xyz, _1068.xyz));
                                }
                                float _75 = dot(_1173, _1173);
                                bool3 _1174 = isnan(_1173);
                                bool3 _1175 = isinf(_1173);
                                bool _1185;
                                if (all(not(bool3(_1174.x || _1175.x, _1174.y || _1175.y, _1174.z || _1175.z))))
                                {
                                    _1185 = !(isnan(_75) || isinf(_75));
                                }
                                else
                                {
                                    _1185 = false;
                                }
                                bool _1189;
                                if (_1185)
                                {
                                    _1189 = _75 > 9.9999996826552253889678874634872e-21;
                                }
                                else
                                {
                                    _1189 = false;
                                }
                                _1190 = _1189;
                                break;
                            } while(false);
                            _1191 = _1068;
                            _1192 = _1069;
                            _1193 = _1070;
                            _1194 = _1190;
                            break;
                        } while(false);
                        if (!_1194)
                        {
                            _1231 = _1191;
                            _1232 = _1192;
                            _1233 = _1193;
                            _1234 = true;
                            break;
                        }
                        uint _35 = _125 + (_924 * 48u);
                        uint _1198 = _35 >> 2u;
                        uint4 _1199 = as_type<uint4>(_1191);
                        frames._m0[_1198] = _1199.x;
                        frames._m0[_1198 + 1u] = _1199.y;
                        frames._m0[_1198 + 2u] = _1199.z;
                        frames._m0[_1198 + 3u] = _1199.w;
                        uint _1208 = (_35 + 16u) >> 2u;
                        uint4 _1209 = as_type<uint4>(_1192);
                        frames._m0[_1208] = _1209.x;
                        frames._m0[_1208 + 1u] = _1209.y;
                        frames._m0[_1208 + 2u] = _1209.z;
                        frames._m0[_1208 + 3u] = _1209.w;
                        uint _1218 = (_35 + 32u) >> 2u;
                        uint4 _1219 = as_type<uint4>(_1193);
                        frames._m0[_1218] = _1219.x;
                        frames._m0[_1218 + 1u] = _1219.y;
                        frames._m0[_1218 + 2u] = _1219.z;
                        frames._m0[_1218 + 3u] = _1219.w;
                        _1228 = _1191;
                        _1229 = _1192;
                        _1230 = _1193;
                    }
                    _927 = _1228;
                    _929 = _1229;
                    _931 = _1230;
                }
                _924++;
                _926 = _927;
                _928 = _929;
                _930 = _931;
                continue;
            }
            else
            {
                _1231 = _926;
                _1232 = _928;
                _1233 = _930;
                _1234 = false;
                break;
            }
        }
        if (_1234)
        {
            break;
        }
        bool _1236 = _520 == 1u;
        if (_1236)
        {
            bool _1242 = _513 < FrameSettings.oldTriangles;
            float4 _1473;
            float4 _1474;
            float4 _1475;
            bool _1476;
            if (_1242)
            {
                float4 _1468;
                float4 _1469;
                float4 _1470;
                bool _1471;
                do
                {
                    float4 _1345;
                    float4 _1346;
                    float4 _1347;
                    if (_513 == 4294967294u)
                    {
                        bool _1260;
                        if ((FrameSettings.pathFlags & 1u) != 0u)
                        {
                            _1260 = FrameSettings.oldPoint.w != 1.0;
                        }
                        else
                        {
                            _1260 = true;
                        }
                        bool _1267;
                        if (!_1260)
                        {
                            _1267 = FrameSettings.oldNormal.w != 0.0;
                        }
                        else
                        {
                            _1267 = true;
                        }
                        if (_1267)
                        {
                            _1468 = float4(0.0);
                            _1469 = float4(0.0);
                            _1470 = float4(0.0);
                            _1471 = false;
                            break;
                        }
                        _1345 = float4(FrameSettings.oldPoint.xyz, 2.0);
                        _1346 = float4(FrameSettings.oldNormal.xyz, 2.0);
                        _1347 = float4(0.0, 0.0, 0.0, 2.0);
                    }
                    else
                    {
                        bool _1289;
                        if (_1242)
                        {
                            _1289 = FrameSettings.previousVertices > FrameSettings.previousMapping;
                        }
                        else
                        {
                            _1289 = true;
                        }
                        bool _1298;
                        if (!_1289)
                        {
                            _1298 = FrameSettings.previousMapping > FrameSettings.geometryBytes;
                        }
                        else
                        {
                            _1298 = true;
                        }
                        bool _1307;
                        if (!_1298)
                        {
                            _1307 = _513 >= ((FrameSettings.previousMapping - FrameSettings.previousVertices) / 48u);
                        }
                        else
                        {
                            _1307 = true;
                        }
                        if (_1307)
                        {
                            _1468 = float4(0.0);
                            _1469 = float4(0.0);
                            _1470 = float4(0.0);
                            _1471 = false;
                            break;
                        }
                        uint _90 = FrameSettings.previousVertices + (_513 * 48u);
                        uint _1312 = _90 >> 2u;
                        uint _1323 = (_90 + 16u) >> 2u;
                        uint _1334 = (_90 + 32u) >> 2u;
                        _1345 = as_type<float4>(uint4(geometry._m0[_1312], geometry._m0[_1312 + 1u], geometry._m0[_1312 + 2u], geometry._m0[_1312 + 3u]));
                        _1346 = as_type<float4>(uint4(geometry._m0[_1323], geometry._m0[_1323 + 1u], geometry._m0[_1323 + 2u], geometry._m0[_1323 + 3u]));
                        _1347 = as_type<float4>(uint4(geometry._m0[_1334], geometry._m0[_1334 + 1u], geometry._m0[_1334 + 2u], geometry._m0[_1334 + 3u]));
                    }
                    bool _1467;
                    do
                    {
                        bool _1356;
                        if (_1345.w == 2.0)
                        {
                            _1356 = _1346.w == 2.0;
                        }
                        else
                        {
                            _1356 = false;
                        }
                        bool _1361;
                        if (_1356)
                        {
                            _1361 = _1347.w == 2.0;
                        }
                        else
                        {
                            _1361 = false;
                        }
                        bool _1377;
                        if (!_1361)
                        {
                            bool _1370;
                            if ((isunordered(_1345.w, 1.0) || _1345.w == 1.0))
                            {
                                _1370 = _1346.w != 1.0;
                            }
                            else
                            {
                                _1370 = true;
                            }
                            bool _1376;
                            if (!_1370)
                            {
                                _1376 = _1347.w != 1.0;
                            }
                            else
                            {
                                _1376 = true;
                            }
                            _1377 = _1376;
                        }
                        else
                        {
                            _1377 = false;
                        }
                        bool _1387;
                        if (!_1377)
                        {
                            bool4 _1381 = isnan(_1345);
                            bool4 _1382 = isinf(_1345);
                            _1387 = !all(not(bool4(_1381.x || _1382.x, _1381.y || _1382.y, _1381.z || _1382.z, _1381.w || _1382.w)));
                        }
                        else
                        {
                            _1387 = true;
                        }
                        bool _1397;
                        if (!_1387)
                        {
                            bool4 _1391 = isnan(_1346);
                            bool4 _1392 = isinf(_1346);
                            _1397 = !all(not(bool4(_1391.x || _1392.x, _1391.y || _1392.y, _1391.z || _1392.z, _1391.w || _1392.w)));
                        }
                        else
                        {
                            _1397 = true;
                        }
                        bool _1407;
                        if (!_1397)
                        {
                            bool4 _1401 = isnan(_1347);
                            bool4 _1402 = isinf(_1347);
                            _1407 = !all(not(bool4(_1401.x || _1402.x, _1401.y || _1402.y, _1401.z || _1402.z, _1401.w || _1402.w)));
                        }
                        else
                        {
                            _1407 = true;
                        }
                        bool _1415;
                        if (!_1407)
                        {
                            _1415 = any(abs(_1345.xyz) > float3(999999995904.0));
                        }
                        else
                        {
                            _1415 = true;
                        }
                        bool _1423;
                        if (!_1415)
                        {
                            _1423 = any(abs(_1346.xyz) > float3(999999995904.0));
                        }
                        else
                        {
                            _1423 = true;
                        }
                        bool _1431;
                        if (!_1423)
                        {
                            _1431 = any(abs(_1347.xyz) > float3(999999995904.0));
                        }
                        else
                        {
                            _1431 = true;
                        }
                        if (_1431)
                        {
                            _1467 = false;
                            break;
                        }
                        bool _1439;
                        if (_1361)
                        {
                            _1439 = any(_1347.xyz != float3(0.0));
                        }
                        else
                        {
                            _1439 = false;
                        }
                        if (_1439)
                        {
                            _1467 = false;
                            break;
                        }
                        float3 _1450;
                        if (_1361)
                        {
                            _1450 = _1346.xyz;
                        }
                        else
                        {
                            _1450 = cross(spvFSub(_1346.xyz, _1345.xyz), spvFSub(_1347.xyz, _1345.xyz));
                        }
                        float _104 = dot(_1450, _1450);
                        bool3 _1451 = isnan(_1450);
                        bool3 _1452 = isinf(_1450);
                        bool _1462;
                        if (all(not(bool3(_1451.x || _1452.x, _1451.y || _1452.y, _1451.z || _1452.z))))
                        {
                            _1462 = !(isnan(_104) || isinf(_104));
                        }
                        else
                        {
                            _1462 = false;
                        }
                        bool _1466;
                        if (_1462)
                        {
                            _1466 = _104 > 9.9999996826552253889678874634872e-21;
                        }
                        else
                        {
                            _1466 = false;
                        }
                        _1467 = _1466;
                        break;
                    } while(false);
                    _1468 = _1345;
                    _1469 = _1346;
                    _1470 = _1347;
                    _1471 = _1467;
                    break;
                } while(false);
                _1473 = _1468;
                _1474 = _1469;
                _1475 = _1470;
                _1476 = !_1471;
            }
            else
            {
                _1473 = _1231;
                _1474 = _1232;
                _1475 = _1233;
                _1476 = true;
            }
            if (_1476)
            {
                break;
            }
            uint4 _1479 = as_type<uint4>(_1473);
            frames._m0[_480] = _1479.x;
            frames._m0[_210] = _1479.y;
            frames._m0[_211] = _1479.z;
            frames._m0[_212] = _1479.w;
            uint4 _1484 = as_type<uint4>(_1474);
            frames._m0[_485] = _1484.x;
            frames._m0[_214] = _1484.y;
            frames._m0[_215] = _1484.z;
            frames._m0[_216] = _1484.w;
            uint4 _1489 = as_type<uint4>(_1475);
            frames._m0[_490] = _1489.x;
            frames._m0[_218] = _1489.y;
            frames._m0[_219] = _1489.z;
            frames._m0[_220] = _1489.w;
        }
        else
        {
            if (_513 != 4294967295u)
            {
                break;
            }
        }
        uint4 _1499 = as_type<uint4>(FrameSettings.oldLiquid[0u]);
        frames._m0[_435] = _1499.x;
        frames._m0[_174] = _1499.y;
        frames._m0[_175] = _1499.z;
        frames._m0[_176] = _1499.w;
        uint4 _1506 = as_type<uint4>(FrameSettings.oldLiquid[1u]);
        frames._m0[_440] = _1506.x;
        frames._m0[_178] = _1506.y;
        frames._m0[_179] = _1506.z;
        frames._m0[_180] = _1506.w;
        uint4 _1513 = as_type<uint4>(FrameSettings.oldLiquid[2u]);
        frames._m0[_445] = _1513.x;
        frames._m0[_182] = _1513.y;
        frames._m0[_183] = _1513.z;
        frames._m0[_184] = _1513.w;
        uint4 _1520 = as_type<uint4>(FrameSettings.oldLiquid[3u]);
        frames._m0[_450] = _1520.x;
        frames._m0[_186] = _1520.y;
        frames._m0[_187] = _1520.z;
        frames._m0[_188] = _1520.w;
        uint4 _1527 = as_type<uint4>(FrameSettings.oldLiquid[4u]);
        frames._m0[_455] = _1527.x;
        frames._m0[_190] = _1527.y;
        frames._m0[_191] = _1527.z;
        frames._m0[_192] = _1527.w;
        uint4 _1534 = as_type<uint4>(FrameSettings.oldLiquid[5u]);
        frames._m0[_460] = _1534.x;
        frames._m0[_194] = _1534.y;
        frames._m0[_195] = _1534.z;
        frames._m0[_196] = _1534.w;
        uint4 _1541 = as_type<uint4>(FrameSettings.oldLiquid[6u]);
        frames._m0[_465] = _1541.x;
        frames._m0[_198] = _1541.y;
        frames._m0[_199] = _1541.z;
        frames._m0[_200] = _1541.w;
        uint4 _1548 = as_type<uint4>(FrameSettings.oldLiquid[7u]);
        frames._m0[_470] = _1548.x;
        frames._m0[_202] = _1548.y;
        frames._m0[_203] = _1548.z;
        frames._m0[_204] = _1548.w;
        uint4 _1559 = as_type<uint4>(float4(_587, float(int(_1236))));
        frames._m0[_475] = _1559.x;
        frames._m0[_206] = _1559.y;
        frames._m0[_207] = _1559.z;
        frames._m0[_208] = _1559.w;
        frames._m0[_495] = _517;
        frames._m0[_222] = _519;
        frames._m0[_223] = uint(_629);
        frames._m0[_224] = _520;
        frames._m0[_500] = 1u;
        frames._m0[_226] = 0u;
        frames._m0[_227] = 0u;
        frames._m0[_228] = 0u;
        break;
    } while(false);
}

