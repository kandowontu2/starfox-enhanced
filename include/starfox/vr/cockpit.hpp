#pragma once
#include "starfox/vr/source_models.hpp"
#include <unordered_map>
namespace starfox::vr {
// Cockpit C presentation rig. Source geometry stays in the user's bundle.
Matrix4 cockpit_instrument_mount(bool extended=false) noexcept;
void mount_cockpit_instruments(std::span<DrawPacket>,bool extended=false);
DrawPacket cockpit_rear_packet(bool srgb=false,unsigned brightness=15);
struct CockpitCutout {std::array<float,3> low,high;};
// Pilot-space volumes (metres) where the cabin replaces the hull: the tub
// from the window lip back past the rear modules, and the canopy around the
// head. Nose, wing blades, wings and tail outside them stay visible.
inline constexpr std::array<CockpitCutout,2> cockpit_hull_cutouts{{
    {{-1.12F,-2.2F,-2.1F},{1.12F,-.28F,.62F}},
    {{-.5F,-.28F,-1.F},{.5F,.6F,.62F}}}};
// Live player or repair-flash geometry in the 12x ship/seat rig, outside the cut-outs.
// `keep_x` (ship-local metres) trims a lost wing to match a damaged live ship.
DrawPacket cockpit_ship_packet(const DrawPacket& source,std::optional<std::array<float,2>> keep_x=std::nullopt);
// Validates the authored front topology before applying the authored face materials.
DrawPacket cockpit_front_packet(const assets::Shape&,bool srgb=false,unsigned brightness=15);
class CockpitGeometry {
public:
    CockpitGeometry(const assets::RomImage&,const assets::SymbolMap&);
    // Replaces the player with its complete ship-local hull and attaches
    // the exact native repair flash, preserving its complete source geometry.
    std::vector<DrawPacket> assemble(SourceModelPackets&,const GameSceneSnapshot&,
        const PresentationPreferences&,bool srgb=false);
private:
    assets::ShapeDecoder decoder_;
    const assets::SymbolMap& symbols_;
    uint32_t flash_player_{};
    std::optional<std::array<int,2>> intact_x_; // MYSHIP_4 x extent, source units
    std::unordered_map<uint32_t,std::array<int,2>> live_x_;
    std::optional<std::array<float,2>> damaged_extent(uint32_t live_shape);
    std::array<int,2> x_extent(uint32_t shape);
    std::optional<assets::Shape> front_;
    std::array<std::optional<std::array<DrawPacket,2>>,32> cabin_;
};
}
