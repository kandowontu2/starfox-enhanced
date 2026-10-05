#pragma once
#include "starfox/platform/nintendo_3ds/game_presentation.hpp"
#include "starfox/platform/nintendo_3ds/pica_frame.hpp"

namespace starfox::platform::nintendo_3ds {
struct LandscapePlane {
    // LCD horizon y = centre + slope*(x-200). Height is source world units,
    // not a stereo setting or an arbitrary normalized-depth multiplier.
    double centre{},slope{},height{};
    bool operator==(const LandscapePlane&) const=default;
};
LandscapePlane source_landscape_plane(const GamePresentation&);
bool native_landscape_scene(const GamePresentation&) noexcept;
bool native_water_scene(const GamePresentation&) noexcept;
double source_water_height(const GamePresentation&);
unsigned source_water_guard(const GamePresentation&);
bool native_corridor_scene(const GamePresentation&) noexcept;
// Camera-ray reciprocal-depth planes q=a*x+b*y+c, in LCD coordinates.
// Absent source walls have q=0 and never produce a finite receiver. A physical
// face through the camera also has no reciprocal ray field; prepare_corridor
// retains that face using signed spatial clipping, without dividing by zero.
std::array<std::array<double,3>,4> source_corridor_planes(const GamePresentation&);
unsigned source_corridor_guard(const GamePresentation&);

// Reuses the isolated BG2 raster, NOT a final scene image. Background artwork
// is at infinity; a finite camera plane provides terrain disparity/occlusion.
// One immutable mesh and borrowed horizontal texture strips serve both eyes.
class GameScenery {
public:
    PicaFrame prepare(const GamePresentation&,const PicaFrame& bg2);
    // Explicit atlas contract: arbitrary source LCD quads, not raster strips.
    // Reject whole-geometry overflow without publishing a partial receiver.
    std::optional<PicaFrame> prepare_tiles(const GamePresentation&,const PicaFrame& bg2,
        unsigned available_guard,unsigned vertex_budget);
    PicaFrame prepare_water(const GamePresentation&,const PicaFrame& bg2,unsigned available_guard);
    PicaFrame prepare_corridor(const GamePresentation&,const PicaFrame& bg2,unsigned available_guard);
private:
    std::vector<PicaVertex> vertices_;
    std::vector<PicaDraw> draws_;
    std::vector<PicaImage> images_;
};
} // namespace starfox::platform::nintendo_3ds
