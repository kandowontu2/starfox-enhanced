#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct type_RWByteAddressBuffer
{
    uint _m0[1];
};

struct type_AdmissionSettings
{
    uint admissionCount;
    uint admissionReserved0;
    uint admissionReserved1;
    uint admissionReserved2;
};

kernel void feature_admission_clear_main(device type_RWByteAddressBuffer& admission [[buffer(0)]], constant type_AdmissionSettings& AdmissionSettings [[buffer(1)]], uint3 gl_GlobalInvocationID [[thread_position_in_grid]])
{
    do
    {
        uint _9 = gl_GlobalInvocationID.x + (gl_GlobalInvocationID.y * 4194240u);
        bool _44;
        if (AdmissionSettings.admissionCount != 0u)
        {
            _44 = AdmissionSettings.admissionCount > 268435456u;
        }
        else
        {
            _44 = true;
        }
        bool _51;
        if (!_44)
        {
            _51 = AdmissionSettings.admissionReserved0 != 0u;
        }
        else
        {
            _51 = true;
        }
        bool _58;
        if (!_51)
        {
            _58 = AdmissionSettings.admissionReserved1 != 0u;
        }
        else
        {
            _58 = true;
        }
        bool _65;
        if (!_58)
        {
            _65 = AdmissionSettings.admissionReserved2 != 0u;
        }
        else
        {
            _65 = true;
        }
        bool _70;
        if (!_65)
        {
            _70 = _9 >= AdmissionSettings.admissionCount;
        }
        else
        {
            _70 = true;
        }
        if (_70)
        {
            break;
        }
        admission._m0[(_9 * 4u) >> 2u] = 0u;
        break;
    } while(false);
}

