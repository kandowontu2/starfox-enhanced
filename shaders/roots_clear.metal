#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

struct type_ClearSettings
{
    uint clearBytes;
    uint clearReserved0;
    uint clearReserved1;
    uint clearReserved2;
};

kernel void feature_roots_clear_main(device type_RWByteAddressBuffer& results [[buffer(0)]], constant type_ClearSettings& ClearSettings [[buffer(1)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        uint _8 = gl_GlobalInvocationID.x * 16u;
        bool _47;
        if (ClearSettings.clearReserved0 == 0u)
        {
            _47 = ClearSettings.clearReserved1 != 0u;
        }
        else
        {
            _47 = true;
        }
        bool _54;
        if (!_47)
        {
            _54 = ClearSettings.clearReserved2 != 0u;
        }
        else
        {
            _54 = true;
        }
        bool _61;
        if (!_54)
        {
            _61 = ClearSettings.clearBytes != 1572864u;
        }
        else
        {
            _61 = true;
        }
        bool _68;
        if (!_61)
        {
            _68 = _8 >= ClearSettings.clearBytes;
        }
        else
        {
            _68 = true;
        }
        if (_68)
        {
            break;
        }
        uint _71 = _8 >> 2u;
        results._m0[_71] = 0u;
        results._m0[_71 + 1u] = 0u;
        results._m0[_71 + 2u] = 0u;
        results._m0[_71 + 3u] = 0u;
        break;
    } while(false);
}

