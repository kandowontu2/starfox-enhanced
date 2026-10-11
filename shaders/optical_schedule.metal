#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

inline uint sfe_shared_uint(const threadgroup uint& value)
{
    return value;
}

struct type_ScheduleSettings
{
    uint queryCount;
    uint reserved0;
    uint reserved1;
    uint reserved2;
};

struct type_ByteAddressBuffer
{
    uint _m0[1];
};

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

kernel void feature_optical_schedule_main(constant type_ScheduleSettings& ScheduleSettings [[buffer(0)]], device type_ByteAddressBuffer& regions [[buffer(1)]], device type_RWByteAddressBuffer& results [[buffer(2)]], device type_RWByteAddressBuffer& workMap [[buffer(3)]], device type_RWByteAddressBuffer& dispatchArgs [[buffer(4)]], uint3 gl_WorkGroupID [[threadgroup_position_in_grid]], uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]])
{
    threadgroup uint workBase;
    threadgroup uint workCount;
    do
    {
        bool _81;
        if (ScheduleSettings.queryCount != 0u)
        {
            _81 = ScheduleSettings.queryCount > 64u;
        }
        else
        {
            _81 = true;
        }
        bool _86;
        if (!_81)
        {
            _86 = gl_WorkGroupID.x >= ScheduleSettings.queryCount;
        }
        else
        {
            _86 = true;
        }
        bool _92;
        if (!_86)
        {
            _92 = gl_WorkGroupID.y != 0u;
        }
        else
        {
            _92 = true;
        }
        bool _98;
        if (!_92)
        {
            _98 = gl_WorkGroupID.z != 0u;
        }
        else
        {
            _98 = true;
        }
        bool _105;
        if (!_98)
        {
            _105 = ScheduleSettings.reserved0 != 0u;
        }
        else
        {
            _105 = true;
        }
        bool _112;
        if (!_105)
        {
            _112 = ScheduleSettings.reserved1 != 0u;
        }
        else
        {
            _112 = true;
        }
        bool _119;
        if (!_112)
        {
            _119 = ScheduleSettings.reserved2 != 0u;
        }
        else
        {
            _119 = true;
        }
        if (_119)
        {
            break;
        }
        uint _16 = gl_WorkGroupID.x * 6160u;
        uint _124 = regions._m0[_16 >> 2u];
        uint _125 = (_16 + 4u) >> 2u;
        uint _127 = regions._m0[_125];
        uint _20 = (gl_WorkGroupID.x * 4096u) + (gl_LocalInvocationIndex * 32u);
        uint _128 = _20 >> 2u;
        results._m0[_128] = 0u;
        results._m0[_128 + 1u] = _127;
        results._m0[_128 + 2u] = 0u;
        results._m0[_128 + 3u] = 0u;
        uint _133 = (_20 + 16u) >> 2u;
        results._m0[_133] = 0u;
        results._m0[_133 + 1u] = 0u;
        results._m0[_133 + 2u] = 0u;
        results._m0[_133 + 3u] = 0u;
        if (gl_LocalInvocationIndex == 0u)
        {
            uint _142;
            _142 = 0u;
            uint _30;
            for (uint _144 = 0u; _144 < gl_WorkGroupID.x; _142 = _30, _144++)
            {
                uint _28 = _144 * 6160u;
                uint _159;
                if (regions._m0[(_28 + 4u) >> 2u] != 0u)
                {
                    _159 = 0u;
                }
                else
                {
                    _159 = min(regions._m0[_28 >> 2u], 128u);
                }
                _30 = _142 + _159;
            }
            workBase = _142;
            uint _165;
            if (_127 != 0u)
            {
                _165 = 0u;
            }
            else
            {
                _165 = min(_124, 128u);
            }
            workCount = _165;
            if ((gl_WorkGroupID.x + 1u) == ScheduleSettings.queryCount)
            {
                dispatchArgs._m0[0u] = _142 + workCount;
                dispatchArgs._m0[1u] = 1u;
                dispatchArgs._m0[2u] = 1u;
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (gl_LocalInvocationIndex < sfe_shared_uint(workCount))
        {
            uint _178 = ((sfe_shared_uint(workBase) + gl_LocalInvocationIndex) * 8u) >> 2u;
            workMap._m0[_178] = gl_WorkGroupID.x;
            workMap._m0[_178 + 1u] = gl_LocalInvocationIndex;
        }
        break;
    } while(false);
}

