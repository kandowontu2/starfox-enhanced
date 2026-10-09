#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace starfox::render {
namespace lava_detail {
using std::min;using std::max;using std::floor;using std::sin;
using std::cos;using std::sqrt;using std::pow;
using std::abs;
#define LAVA_FN inline
#define LAVA_UINT std::uint32_t
#include "lava_surface.inc"
#undef LAVA_FN
#undef LAVA_UINT
}
using lava_detail::lava_surface;
using lava_detail::lava_shade;
}
