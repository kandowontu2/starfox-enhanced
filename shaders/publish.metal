#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

struct type_PublishSettings
{
    uint publishWidth;
    uint publishHeight;
    uint publishRecords;
    uint publishPathStride;
    uint publishFirst;
    uint publishCount;
    uint publishLobes;
    uint publishReserved;
};

kernel void feature_publish_main(device type_ByteAddressBuffer& packets [[buffer(0)]], device type_RWByteAddressBuffer& image [[buffer(1)]], constant type_PublishSettings& PublishSettings [[buffer(2)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        bool _116;
        if (PublishSettings.publishLobes != 1u)
        {
            _116 = PublishSettings.publishLobes != 8u;
        }
        else
        {
            _116 = false;
        }
        bool _123;
        if (!_116)
        {
            _123 = PublishSettings.publishWidth == 0u;
        }
        else
        {
            _123 = true;
        }
        bool _130;
        if (!_123)
        {
            _130 = PublishSettings.publishHeight == 0u;
        }
        else
        {
            _130 = true;
        }
        bool _137;
        if (!_130)
        {
            _137 = PublishSettings.publishWidth > 16384u;
        }
        else
        {
            _137 = true;
        }
        bool _144;
        if (!_137)
        {
            _144 = PublishSettings.publishHeight > 16384u;
        }
        else
        {
            _144 = true;
        }
        bool _151;
        if (!_144)
        {
            _151 = PublishSettings.publishReserved != 0u;
        }
        else
        {
            _151 = true;
        }
        bool _162;
        if (!_151)
        {
            bool _161;
            if (PublishSettings.publishPathStride != 52u)
            {
                _161 = PublishSettings.publishPathStride != 64u;
            }
            else
            {
                _161 = false;
            }
            _162 = _161;
        }
        else
        {
            _162 = true;
        }
        bool _169;
        if (!_162)
        {
            _169 = PublishSettings.publishRecords > 64u;
        }
        else
        {
            _169 = true;
        }
        bool _176;
        if (!_169)
        {
            _176 = PublishSettings.publishCount == 0u;
        }
        else
        {
            _176 = true;
        }
        bool _183;
        if (!_176)
        {
            _183 = PublishSettings.publishCount > 64u;
        }
        else
        {
            _183 = true;
        }
        bool _190;
        if (!_183)
        {
            _190 = (PublishSettings.publishCount % PublishSettings.publishLobes) != 0u;
        }
        else
        {
            _190 = true;
        }
        bool _197;
        if (!_190)
        {
            _197 = (PublishSettings.publishFirst % PublishSettings.publishLobes) != 0u;
        }
        else
        {
            _197 = true;
        }
        if (_197)
        {
            break;
        }
        uint _12 = gl_GlobalInvocationID.x * PublishSettings.publishLobes;
        if (_12 >= PublishSettings.publishCount)
        {
            break;
        }
        uint _13 = PublishSettings.publishFirst + _12;
        uint _14 = _13 / PublishSettings.publishLobes;
        uint _15 = PublishSettings.publishWidth * PublishSettings.publishHeight;
        bool _216;
        if (_13 >= PublishSettings.publishFirst)
        {
            _216 = _14 >= _15;
        }
        else
        {
            _216 = true;
        }
        if (_216)
        {
            break;
        }
        uint _16 = _12 * 24576u;
        uint _219 = (_16 + 512u) >> 2u;
        uint _18 = _219 + 1u;
        uint _19 = _219 + 2u;
        uint _20 = _219 + 3u;
        uint4 _228 = uint4(packets._m0[_219], packets._m0[_18], packets._m0[_19], packets._m0[_20]);
        uint _229 = (_16 + 528u) >> 2u;
        bool _236;
        if (packets._m0[_219] == 1u)
        {
            _236 = packets._m0[_18] != 0u;
        }
        else
        {
            _236 = true;
        }
        bool _241;
        if (!_236)
        {
            _241 = packets._m0[_19] != _14;
        }
        else
        {
            _241 = true;
        }
        bool _246;
        if (!_241)
        {
            _246 = packets._m0[_20] == 0u;
        }
        else
        {
            _246 = true;
        }
        bool _253;
        if (!_246)
        {
            _253 = packets._m0[_20] >= (1u << (PublishSettings.publishLobes & 31u));
        }
        else
        {
            _253 = true;
        }
        if (_253)
        {
            break;
        }
        uint _256 = (_14 * 4u) >> 2u;
        if ((packets._m0[_229] & 4278190080u) != (image._m0[_256] & 4278190080u))
        {
            break;
        }
        bool _370;
        uint _265 = 0u;
        for (;;)
        {
            if (_265 < PublishSettings.publishLobes)
            {
                uint _24 = (_12 + _265) * 24576u;
                uint _270 = (_24 + 512u) >> 2u;
                bool _289;
                if (!any(uint4(packets._m0[_270], packets._m0[_270 + 1u], packets._m0[_270 + 2u], packets._m0[_270 + 3u]) != _228))
                {
                    _289 = packets._m0[(_24 + 528u) >> 2u] != packets._m0[_229];
                }
                else
                {
                    _289 = true;
                }
                bool _297;
                if (!_289)
                {
                    _297 = packets._m0[(_24 + 540u) >> 2u] != 0u;
                }
                else
                {
                    _297 = true;
                }
                bool _305;
                if (!_297)
                {
                    _305 = packets._m0[(_24 + 556u) >> 2u] != 0u;
                }
                else
                {
                    _305 = true;
                }
                bool _321;
                if (!_305)
                {
                    uint _309 = (_24 + 560u) >> 2u;
                    _321 = any(uint4(packets._m0[_309], packets._m0[_309 + 1u], packets._m0[_309 + 2u], packets._m0[_309 + 3u]) != uint4(0u));
                }
                else
                {
                    _321 = true;
                }
                bool _343;
                if (!_321)
                {
                    uint _325 = (_24 + 544u) >> 2u;
                    uint _333 = (_16 + 544u) >> 2u;
                    _343 = any(uint3(packets._m0[_325], packets._m0[_325 + 1u], packets._m0[_325 + 2u]) != uint3(packets._m0[_333], packets._m0[_333 + 1u], packets._m0[_333 + 2u]));
                }
                else
                {
                    _343 = true;
                }
                if (_343)
                {
                    _370 = true;
                    break;
                }
                uint _350 = (_24 + 532u) >> 2u;
                if ((packets._m0[_20] & (1u << (_265 & 31u))) != 0u)
                {
                    if ((packets._m0[_350] >> 24u) != 255u)
                    {
                        _370 = true;
                        break;
                    }
                }
                else
                {
                    if (packets._m0[_350] != image._m0[(((_15 * PublishSettings.publishRecords) + ((_13 + _265) * PublishSettings.publishPathStride)) + 36u) >> 2u])
                    {
                        _370 = true;
                        break;
                    }
                }
                _265++;
                continue;
            }
            else
            {
                _370 = false;
                break;
            }
        }
        if (_370)
        {
            break;
        }
        image._m0[_256] = packets._m0[_229];
        for (uint _373 = 0u; _373 < PublishSettings.publishLobes; )
        {
            image._m0[(((_15 * PublishSettings.publishRecords) + ((_13 + _373) * PublishSettings.publishPathStride)) + 36u) >> 2u] = packets._m0[(((_12 + _373) * 24576u) + 532u) >> 2u];
            _373++;
            continue;
        }
        break;
    } while(false);
}

