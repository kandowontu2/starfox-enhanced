#pragma once
#include <algorithm>
#include <cmath>
namespace starfox::render::water_optics {
using std::max;using std::exp;
#define WATER_OPTICS_FN inline
#include "water_transmission.inc"
#undef WATER_OPTICS_FN
}
