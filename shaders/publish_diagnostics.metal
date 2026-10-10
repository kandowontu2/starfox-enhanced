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

kernel void feature_publish_diagnostics_main(device type_ByteAddressBuffer& packets [[buffer(0)]], device type_RWByteAddressBuffer& image [[buffer(1)]], device type_RWByteAddressBuffer& admission [[buffer(2)]], constant type_PublishSettings& PublishSettings [[buffer(3)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        bool _117;
        if (PublishSettings.publishLobes != 1u)
        {
            _117 = PublishSettings.publishLobes != 8u;
        }
        else
        {
            _117 = false;
        }
        bool _124;
        if (!_117)
        {
            _124 = PublishSettings.publishWidth == 0u;
        }
        else
        {
            _124 = true;
        }
        bool _131;
        if (!_124)
        {
            _131 = PublishSettings.publishHeight == 0u;
        }
        else
        {
            _131 = true;
        }
        bool _138;
        if (!_131)
        {
            _138 = PublishSettings.publishWidth > 16384u;
        }
        else
        {
            _138 = true;
        }
        bool _145;
        if (!_138)
        {
            _145 = PublishSettings.publishHeight > 16384u;
        }
        else
        {
            _145 = true;
        }
        bool _152;
        if (!_145)
        {
            _152 = PublishSettings.publishReserved != 0u;
        }
        else
        {
            _152 = true;
        }
        bool _163;
        if (!_152)
        {
            bool _162;
            if (PublishSettings.publishPathStride != 52u)
            {
                _162 = PublishSettings.publishPathStride != 64u;
            }
            else
            {
                _162 = false;
            }
            _163 = _162;
        }
        else
        {
            _163 = true;
        }
        bool _170;
        if (!_163)
        {
            _170 = PublishSettings.publishRecords > 64u;
        }
        else
        {
            _170 = true;
        }
        bool _177;
        if (!_170)
        {
            _177 = PublishSettings.publishCount == 0u;
        }
        else
        {
            _177 = true;
        }
        bool _184;
        if (!_177)
        {
            _184 = PublishSettings.publishCount > 64u;
        }
        else
        {
            _184 = true;
        }
        bool _191;
        if (!_184)
        {
            _191 = (PublishSettings.publishCount % PublishSettings.publishLobes) != 0u;
        }
        else
        {
            _191 = true;
        }
        bool _198;
        if (!_191)
        {
            _198 = (PublishSettings.publishFirst % PublishSettings.publishLobes) != 0u;
        }
        else
        {
            _198 = true;
        }
        if (_198)
        {
            break;
        }
        uint _13 = gl_GlobalInvocationID.x * PublishSettings.publishLobes;
        if (_13 >= PublishSettings.publishCount)
        {
            break;
        }
        uint _14 = PublishSettings.publishFirst + _13;
        uint _15 = _14 / PublishSettings.publishLobes;
        uint _16 = PublishSettings.publishWidth * PublishSettings.publishHeight;
        bool _217;
        if (_14 >= PublishSettings.publishFirst)
        {
            _217 = _15 >= _16;
        }
        else
        {
            _217 = true;
        }
        if (_217)
        {
            break;
        }
        uint _17 = _13 * 24576u;
        uint _220 = (_17 + 512u) >> 2u;
        uint _19 = _220 + 1u;
        uint _20 = _220 + 2u;
        uint _21 = _220 + 3u;
        uint _228 = packets._m0[_21];
        uint4 _229 = uint4(packets._m0[_220], packets._m0[_19], packets._m0[_20], _228);
        uint _230 = (_17 + 528u) >> 2u;
        bool _237;
        if (packets._m0[_220] == 1u)
        {
            _237 = packets._m0[_19] != 0u;
        }
        else
        {
            _237 = true;
        }
        bool _242;
        if (!_237)
        {
            _242 = packets._m0[_20] != _15;
        }
        else
        {
            _242 = true;
        }
        bool _247;
        if (!_242)
        {
            _247 = _228 == 0u;
        }
        else
        {
            _247 = true;
        }
        bool _254;
        if (!_247)
        {
            _254 = _228 >= (1u << (PublishSettings.publishLobes & 31u));
        }
        else
        {
            _254 = true;
        }
        if (_254)
        {
            break;
        }
        uint _257 = (_15 * 4u) >> 2u;
        if ((packets._m0[_230] & 4278190080u) != (image._m0[_257] & 4278190080u))
        {
            break;
        }
        bool _371;
        uint _266 = 0u;
        for (;;)
        {
            if (_266 < PublishSettings.publishLobes)
            {
                uint _25 = (_13 + _266) * 24576u;
                uint _271 = (_25 + 512u) >> 2u;
                bool _290;
                if (!any(uint4(packets._m0[_271], packets._m0[_271 + 1u], packets._m0[_271 + 2u], packets._m0[_271 + 3u]) != _229))
                {
                    _290 = packets._m0[(_25 + 528u) >> 2u] != packets._m0[_230];
                }
                else
                {
                    _290 = true;
                }
                bool _298;
                if (!_290)
                {
                    _298 = packets._m0[(_25 + 540u) >> 2u] != 0u;
                }
                else
                {
                    _298 = true;
                }
                bool _306;
                if (!_298)
                {
                    _306 = packets._m0[(_25 + 556u) >> 2u] != 0u;
                }
                else
                {
                    _306 = true;
                }
                bool _322;
                if (!_306)
                {
                    uint _310 = (_25 + 560u) >> 2u;
                    _322 = any(uint4(packets._m0[_310], packets._m0[_310 + 1u], packets._m0[_310 + 2u], packets._m0[_310 + 3u]) != uint4(0u));
                }
                else
                {
                    _322 = true;
                }
                bool _344;
                if (!_322)
                {
                    uint _326 = (_25 + 544u) >> 2u;
                    uint _334 = (_17 + 544u) >> 2u;
                    _344 = any(uint3(packets._m0[_326], packets._m0[_326 + 1u], packets._m0[_326 + 2u]) != uint3(packets._m0[_334], packets._m0[_334 + 1u], packets._m0[_334 + 2u]));
                }
                else
                {
                    _344 = true;
                }
                if (_344)
                {
                    _371 = true;
                    break;
                }
                uint _351 = (_25 + 532u) >> 2u;
                if ((_228 & (1u << (_266 & 31u))) != 0u)
                {
                    if ((packets._m0[_351] >> 24u) != 255u)
                    {
                        _371 = true;
                        break;
                    }
                }
                else
                {
                    if (packets._m0[_351] != image._m0[(((_16 * PublishSettings.publishRecords) + ((_14 + _266) * PublishSettings.publishPathStride)) + 36u) >> 2u])
                    {
                        _371 = true;
                        break;
                    }
                }
                _266++;
                continue;
            }
            else
            {
                _371 = false;
                break;
            }
        }
        if (_371)
        {
            break;
        }
        image._m0[_257] = packets._m0[_230];
        for (uint _374 = 0u; _374 < PublishSettings.publishLobes; )
        {
            image._m0[(((_16 * PublishSettings.publishRecords) + ((_14 + _374) * PublishSettings.publishPathStride)) + 36u) >> 2u] = packets._m0[(((_13 + _374) * 24576u) + 532u) >> 2u];
            _374++;
            continue;
        }
        admission._m0[_257] = _228;
        break;
    } while(false);
}

