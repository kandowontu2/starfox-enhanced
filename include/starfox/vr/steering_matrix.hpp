#pragma once
#include <array>
namespace starfox::vr {
// Column-major projection of native pad directions into cockpit pad axes.
// Shared presentation data: contains no OpenXR input or physical head pose.
using SteeringMatrix=std::array<float,4>;
}
