#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

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

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

kernel void feature_reduce_main(constant type_Settings& Settings [[buffer(0)]], device type_RWByteAddressBuffer& atlas [[buffer(1)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        bool _96;
        if (!any(gl_GlobalInvocationID.xy >= Settings.levels[Settings.workLevel].yz))
        {
            _96 = gl_GlobalInvocationID.z >= Settings.lobes;
        }
        else
        {
            _96 = true;
        }
        if (_96)
        {
            break;
        }
        uint _100;
        float3 _103;
        uint _105;
        float3 _107;
        _100 = 0u;
        _103 = float3(-3.4028234663852885981170418348452e+38);
        _105 = 0u;
        _107 = float3(3.4028234663852885981170418348452e+38);
        uint _101;
        float3 _104;
        uint _106;
        float3 _108;
        for (uint _109 = 0u; _109 < 2u; _100 = _101, _103 = _104, _105 = _106, _107 = _108, _109++)
        {
            _101 = _100;
            _106 = _105;
            _104 = _103;
            _108 = _107;
            uint _114;
            uint _116;
            float3 _117;
            float3 _118;
            for (uint _119 = 0u; _119 < 2u; _101 = _114, _106 = _116, _104 = _117, _108 = _118, _119++)
            {
                uint2 _11 = (gl_GlobalInvocationID.xy * uint2(2u)) + uint2(_119, _109);
                uint _12 = Settings.workLevel - 1u;
                if (any(_11 >= Settings.levels[_12].yz))
                {
                    _114 = _101;
                    _116 = _106;
                    _117 = _104;
                    _118 = _108;
                    continue;
                }
                uint _34 = ((((gl_GlobalInvocationID.z * Settings.totalNodes) + Settings.levels[_12].x) + (_11.y * Settings.levels[_12].y)) + _11.x) * 32u;
                uint _140 = _34 >> 2u;
                uint _15 = _140 + 3u;
                uint _150 = (_34 + 16u) >> 2u;
                uint _19 = _150 + 3u;
                _114 = _101 | atlas._m0[_19];
                _116 = _106 | atlas._m0[_15];
                _117 = precise::max(_104, as_type<float3>(uint4(atlas._m0[_150], atlas._m0[_150 + 1u], atlas._m0[_150 + 2u], atlas._m0[_19]).xyz));
                _118 = precise::min(_108, as_type<float3>(uint4(atlas._m0[_140], atlas._m0[_140 + 1u], atlas._m0[_140 + 2u], atlas._m0[_15]).xyz));
            }
        }
        uint _40 = ((((gl_GlobalInvocationID.z * Settings.totalNodes) + Settings.levels[Settings.workLevel].x) + (gl_GlobalInvocationID.y * Settings.levels[Settings.workLevel].y)) + gl_GlobalInvocationID.x) * 32u;
        uint _177 = _40 >> 2u;
        uint3 _178 = as_type<uint3>(_107);
        atlas._m0[_177] = _178.x;
        atlas._m0[_177 + 1u] = _178.y;
        atlas._m0[_177 + 2u] = _178.z;
        atlas._m0[_177 + 3u] = _105;
        uint _186 = (_40 + 16u) >> 2u;
        uint3 _187 = as_type<uint3>(_103);
        atlas._m0[_186] = _187.x;
        atlas._m0[_186 + 1u] = _187.y;
        atlas._m0[_186 + 2u] = _187.z;
        atlas._m0[_186 + 3u] = _100;
        break;
    } while(false);
}

