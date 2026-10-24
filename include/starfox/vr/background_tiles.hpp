#pragma once
#include "starfox/simulation/snes_ppu.hpp"
#include "starfox/vr/draw_packet.hpp"
#include <array>
#include <optional>
#include <vector>

namespace starfox::vr {
struct BackgroundTileOptions;
enum class BackgroundLayer { bg1, bg2, bg3 };
struct PhotographicLandscape {
    bool operator==(const PhotographicLandscape&) const = default;
    // Match desktop's source-pixel scale near the forward direction. The
    // repeated panorama closes at the rear without putting imagery on ground.
    float horizon_v{1.F},vertical_scale{512.F/160.F},horizontal_offset{.25F};
    float horizontal_scale{1.F};
    unsigned repeats{6};
    bool full_sphere{};
    bool latitude_uv{}; // All-sky stars: distribute vertically instead of clamping a horizon strip.
    bool orbital_surface{}; // Local-direction surface continuation; blend to separate stars above the limb.
    bool preserve_game_over_front{};
    std::array<float,4> response{1,1,1,1};
    std::array<float,3> palette_shift{};
    // Native BGR555 ramps; index zero selects Titania (1) or Sector K (2).
    // Draw data, not image pixels: weather must never reload the photograph.
    std::array<uint16_t,16> cloud_palette{};
};
struct PhotographicBody {
    bool operator==(const PhotographicBody&) const = default;
    // Source-pixel registration before the shared landscape motion matrix.
    std::array<float,2> center{128,112},diameter{56,56};
    std::array<float,4> response{1,1,1,1};
    // Optional two-tone native shading (bright/dark BGR555).
    std::array<float,3> palette_shift{};
    bool two_tone{};
    uint16_t bright{},dark{};
    // 4: face ramp; 5: cloud limb; 6/7: gray/blue city moons; 8: orbital moon.
    std::array<uint16_t,16> palette{};
};
[[nodiscard]] DrawPacket photographic_body_packet(
    std::shared_ptr<const std::vector<uint32_t>>,const PhotographicBody&,bool srgb=false);
[[nodiscard]] Matrix4 photographic_body_motion(std::array<float,2> center);
// Correct current-tick panorama geometry toward the interpolated scroll. Delta
// is wrapped source pixels; no texture/vertex upload is needed between ticks.
[[nodiscard]] Matrix4 photographic_scroll_correction(float dx,float dy,double alpha,
    float horizontal_scale=1.F,float vertical_scale=512.F/224.F);
// One indexed landmark over a replaced sky. Only selected native ink runs are
// geometry; palette fades stay in the original GPU tile sampler.
[[nodiscard]] DrawPacket landscape_landmark_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,const std::array<unsigned,4>&,
    const std::array<bool,256>&,bool srgb=false);
// Upper hemisphere only. Apply the same landscape_camera_motion as the native
// ground packet. Palette response is independent of the retained pixel payload.
[[nodiscard]] DrawPacket photographic_landscape_packet(
    std::shared_ptr<const std::vector<uint32_t>> texture,
    const PhotographicLandscape& options={},bool srgb=false);
// Opaque enclosure with a 256x224 source-window opening. Draw after world
// geometry with depth disabled, before HUD; caller supplies source placement.
// Space Armada-style chequered tunnels as a real 3D corridor. The cartridge
// fakes the corridor with two BG2 art pages chosen per scanline by
// CHEQUERED_TABLES (indexed by camera Z). Fitted to those tables: ceiling and
// floor 60 units from the eye (focal 256), bands repeating every 256 units of
// world Z, ceiling lit rings over [2,66) and a floor chequer over [184,312).
// The packet is a closed box (source units; place it with
// tunnel_corridor_model): ceiling and floor half_width wide (the authored
// bounds, e.g. MTUNNEL +-90) and the two side walls. Ceiling and floor are
// coloured from the cartridge art per depth row, with the art's tile-stepped
// edge strips fitted to straight lines and scaled so its wall line meets the
// walls; the walls carry what the art paints beyond that line, at its true
// depth. Empty when no page shows a fitted strip; callers then keep the
// native layer.
[[nodiscard]] DrawPacket tunnel_corridor_packet(const simulation::SnesPpuState&,unsigned brightness=15,bool srgb=false,
    float half_width=90.F);
[[nodiscard]] bool is_tunnel_corridor(const DrawPacket&) noexcept;
// Planet landscapes as a sky dome plus a ground plane (one packet), coloured
// by view direction from the BG2 art exactly as the native screen maps it at
// the current tick: horizon art row from the Mode 2 offset line (or BG2
// vertical scroll) at the screen centre, centre column from BG2 horizontal
// scroll. Roll and camera height are applied per frame with
// landscape_world_model, so the horizon rotates as geometry instead of
// 8-pixel offset columns. Model units are camera heights (ground at Y=-1).
// BG2 art (the whole tile map, wrapping): size in pixels, palette index of
// one pixel (0 transparent), and an RGBA rectangle (red low byte) for dumps.
[[nodiscard]] std::array<unsigned,2> bg2_art_size(const simulation::SnesPpuState&) noexcept;
[[nodiscard]] unsigned bg2_art_index(const simulation::SnesPpuState&,int x,int y) noexcept;
[[nodiscard]] std::vector<uint32_t> bg2_art_image(const simulation::SnesPpuState&,int x,int y,unsigned width,unsigned height);
[[nodiscard]] DrawPacket landscape_world_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,float vanishing_y=112,bool srgb=false,bool classic_ground_scroll=false,
    std::optional<float> level_horizon={},std::optional<uint32_t> ground_dot_colour={});
// Art row of the true horizon this tick (vanishing row + Mode 2 offset line
// at the centre column, or BG2 vertical scroll), including the cartridge's
// pitch shift.
[[nodiscard]] float landscape_art_horizon(const simulation::SnesPpuState&,float vanishing_y=112);
[[nodiscard]] bool is_landscape_world(const DrawPacket&) noexcept;
// Placement: roll and pitch (radians; the camera's own rotation applied as
// geometry) and camera height above the ground (source units).
[[nodiscard]] Matrix4 landscape_world_model(double roll,double pitch,double height) noexcept;
// The world-fixed ground plane for a landscape_world_packet (same art and
// mapping; plane Y = 0 in source units). With classic_ground_scroll the
// art's stepped bands are anchored to world Z every 256 units (the dot grid's
// cell) and stream past like the dots; otherwise they stay fixed on screen.
[[nodiscard]] DrawPacket landscape_ground_packet(const DrawPacket& world);
[[nodiscard]] bool is_landscape_ground(const DrawPacket&) noexcept;
[[nodiscard]] Matrix4 landscape_ground_model(double roll,double pitch,double height,double camera_x,double camera_z) noexcept;
// The same placements from the source view rotation (model axes: X = source
// X, Y up, Z = -source Z; the rotation objects are drawn with, yaw included),
// so the ground, its dots and stripes stay fixed to the world the objects
// stand in. The dome keeps the camera heading (its art column comes from the
// BG2 scroll) and shares the ground's up vector, so both horizons agree.
[[nodiscard]] Matrix4 landscape_ground_view_model(const Matrix4& rotation,double height,double camera_x,double camera_z) noexcept;
[[nodiscard]] Matrix4 landscape_dome_view_model(const Matrix4& rotation,double height) noexcept;

// Replacement artwork (RGBA words, r in the low byte) for a landscape.
// Sky: replaces the art above the horizon, mapped like the BG2 strip: the
// image's width spans `art_width` art columns (wrapping) and its height the
// `rows_above` native rows above the horizon, its bottom edge on the horizon.
// Place with landscape_world_model; the horizon row and centre column come
// per draw (SdlFrameRenderer packs them into camera.effects.z).
// Ground: tiles the world-fixed ground every `tile` source units; place with
// landscape_ground_model.
enum class LandscapeTexture {sky,ground};
[[nodiscard]] DrawPacket landscape_texture_packet(LandscapeTexture,std::span<const uint32_t> rgba,
    unsigned width,unsigned height,float art_width,float rows_above,float tile,bool srgb=false);
[[nodiscard]] bool is_landscape_texture(const DrawPacket&,LandscapeTexture) noexcept;
// Headset views: map a landscape world, ground, replacement-art or corridor packet by
// direction (cylindrical: art column from the heading angle, row from the
// tangent of the elevation; seamless all round) instead of the flat screen's
// planar projection, which only holds within 90 degrees of the view axis.
// The ground's art columns then follow the source camera heading passed in
// camera.effects.z (ModernFrame::landscape_heading), not the eye's. A 3D
// tunnel corridor colours its rows by distance along the corridor instead of
// the eye's view depth, and extends behind the seat.
void set_headset_mapping(DrawPacket&) noexcept;
// Placement for a camera at world X/Y/Z (source units, Y down; fractional for
// display-rate motion): the box stays fixed in the world, centred on
// centre_x/centre_y (the authored bounds' middle, e.g. MTUNNEL 0 / -60).
[[nodiscard]] Matrix4 tunnel_corridor_model(double camera_z,double camera_x,double camera_y,
    double centre_y=-60.,double centre_x=0.) noexcept;
[[nodiscard]] DrawPacket tunnel_surround_packet(const std::array<float,4>& colour);
// Space Armada's boss room as a real room around the reactor. The cartridge
// paints it as one fixed BG2 picture (ceiling and floor bands, a dark far
// wall); the 3D room puts that wall on the ring the BOSS_8_4 wall panels
// rotate on, an ellipse radius wide and 2*radius deep about the model origin
// (the ring's centre at world Y 0; source units, +Y up, forward -Z), with the
// ceiling at world Y top and the floor at bottom (Y down), coloured as the
// painting's far wall (nucleus_floor_packet draws the ceiling and floor; the
// eye sits camera_offset units behind the ring's centre at height eye_y).
// Draw it like the layer it replaces: behind every object.
// Empty when the art has no wall band at its centre.
[[nodiscard]] DrawPacket nucleus_room_packet(const simulation::SnesPpuState&,unsigned brightness,bool srgb,
    float radius,float top,float bottom,float eye_y,float camera_offset);
[[nodiscard]] bool is_nucleus_room(const DrawPacket&) noexcept;
// The room's ceiling and floor, in nucleus_room_packet's model space and
// placed as it is: sectors about the reactor's axis (model reactor_x,
// reactor_z), turned by its spin (radians), alternating two of the
// painting's shades and clipped to the drum's ellipse. Draw it before the
// room.
[[nodiscard]] DrawPacket nucleus_floor_packet(const simulation::SnesPpuState&,unsigned brightness,bool srgb,
    float radius,float top,float bottom,float eye_y,float camera_offset,float reactor_x,float reactor_z,double turn,unsigned sectors);
[[nodiscard]] bool is_nucleus_floor(const DrawPacket&) noexcept;
// Placement for a camera at world X/Y/Z (source units, Y down): the room is
// fixed in the world around the ring centre at world Z centre_z.
[[nodiscard]] Matrix4 nucleus_room_model(double camera_x,double camera_y,double camera_z,double centre_z) noexcept;
[[nodiscard]] DrawPacket tunnel_surround_packet(const std::array<float,4>& wall,
    const std::array<float,4>& ceiling,const std::array<float,4>& floor);
// Opaque CGRAM-zero backdrop, with source integer fading followed by target
// color-space conversion. Shared by both eyes, independent of layer enables.
[[nodiscard]] std::array<float,4> source_backdrop_colour(uint16_t bgr555,
    unsigned brightness,bool srgb=false);
[[nodiscard]] std::array<float,4> source_background_border_colour(
    const std::array<uint16_t,256>& palette,unsigned brightness,bool srgb=false);
[[nodiscard]] std::array<float,4> source_menu_background_colour(
    const simulation::SnesPpuState&,unsigned brightness,bool srgb=false);
struct BackgroundTileOptions {
    unsigned priority{}; // 0 all, 1 low, 2 high
    unsigned brightness{15}; // source display brightness, 0..15
    unsigned colour_subtract{}; // BG2-only native HALFFADE, 0..31.
    bool expanded_horizontal{};
    // Tunnels with expanded_horizontal: repeat each row's edge pixel outward
    // (desktop renderer) instead of the closed side-wall colour (VR).
    bool clamp_tunnel_edges{};
    bool transparent_black{}; // Test source palette before brightness fading.
    bool wrap_horizontal{true}; // False: expose only one authored tilemap occurrence.
    unsigned single_occurrence_top_rows{}; // 0..224: unique top band; 512: one complete atlas in both axes.
    bool ex_twin_planets{}; // BG_5_4: unique planet ink, repeatable cloud tiles.
    bool ex_face_planets{}; // BG_6_3H: unique planets/saucers, repeatable stars.
    bool ex_ocean_island{}; // BG_6_2: preserve the first island, repeat open water.
    bool ex_volcanic_horizon{}; // BG_6_6: one bright eruption on a repeatable horizon.
    bool ex_city_planets{}; // BG_5_2: unique planets/palace, surrounding stars/city.
    unsigned guard_inset{}; // Native pixel columns omitted from each side.
    std::array<int32_t,2> horizontal_bounds{0,256}; // Logical pixels, not stretched UVs.
    // Source pixels the quad reaches past every edge, sampling the first row
    // above and the last 12 rows mirrored below (the wrapped tilemap at the
    // sides): a layer rolled as geometry (space horizons) still covers the
    // view's corners.
    unsigned overscan{};
    std::optional<std::array<int16_t,2>> scroll_override;
    // Flat presentation only: fractional scroll (source pixels) added to the
    // integer scroll, and a display-rate Mode 2 horizon line (intercept,
    // slope per 8-pixel column) replacing the VRAM offset-table fit.
    std::optional<std::array<float,2>> scroll_fraction;
    std::optional<std::array<float,2>> mode2_horizon;
};
// Encode source data only: no CPU pixel decoding, projection, or layer order.
// Caller supplies brightness and owns screen enables, clipping and composition.
// Unsupported source modes throw instead of silently substituting a layer.
[[nodiscard]] std::vector<uint32_t> background_tile_payload(
    const simulation::SnesPpuState&,BackgroundLayer,
    const BackgroundTileOptions& = {});
// Logical-coordinate quad (256x224 by default); caller supplies its world transform.
// Disabled main-screen layers produce empty packets, without uploading VRAM.
[[nodiscard]] DrawPacket background_tile_packet(const simulation::SnesPpuState&,
    BackgroundLayer,const BackgroundTileOptions& = {},bool srgb=false);
// Original intro DEMO atlas: its upper 256 rows contain stars only.
[[nodiscard]] DrawPacket intro_star_sphere_packet(const simulation::SnesPpuState&,
    unsigned brightness=15,bool srgb=false,bool full_atlas=false,bool upper_left_only=false,
    bool retain_source_scroll=false,std::optional<std::array<float,2>> scroll_fraction={},
    std::optional<std::array<float,2>> mode2_horizon={});
[[nodiscard]] DrawPacket game_over_star_sphere_packet(const simulation::SnesPpuState&,
    unsigned brightness=15,unsigned colour_subtract=0,bool srgb=false);
// Cut out only exterior black ink; enclosed black facial details stay opaque.
[[nodiscard]] std::vector<SceneVertex> game_over_foreground_vertices(
    const simulation::SnesPpuState&,bool srgb=false);
[[nodiscard]] DrawPacket intro_planet_packet(const simulation::SnesPpuState&,
    unsigned brightness=15,bool srgb=false);
[[nodiscard]] DrawPacket unique_planet_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,const std::array<unsigned,4>& rectangle,bool srgb=false);
[[nodiscard]] Matrix4 intro_planet_motion(int16_t previous_scroll,int16_t current_scroll,
    double alpha,float horizon_y=112);
[[nodiscard]] Matrix4 landscape_pitch_motion(uint16_t previous_pitch,uint16_t current_pitch,double alpha);
[[nodiscard]] Matrix4 landscape_scroll_motion(int16_t previous_scroll,int16_t current_scroll,double alpha,
    uint16_t previous_bank=0,uint16_t current_bank=0,uint16_t atlas_origin=232);
[[nodiscard]] DrawPacket landscape_sphere_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,float horizon_y=112,bool srgb=false,bool unique_half=false,uint16_t atlas_origin=232,bool unique_right_half=false);
// Lower hemisphere becomes a receiver at the grid's camera-relative Y.
// Upper sky/mountains retain the enclosing background projection.
void place_landscape_ground(DrawPacket&,float ground_y,bool gpu=false);
[[nodiscard]] DrawPacket water_surround_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,bool srgb=false);
// Inverse-projected Mode-1 foreground. Height is camera-to-receiver distance.
[[nodiscard]] DrawPacket water_surface_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,float height,bool srgb=false);
[[nodiscard]] Matrix4 water_height_motion(float previous_height,float current_height,double alpha);
// Orbital horizon: preserve the authored vertical band instead of clamping
// its final texture row over a hemisphere. Unique upper rows remain unique.
[[nodiscard]] DrawPacket space_horizon_sphere_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,bool srgb=false);
// Original LSB atlas: stars above the horizon, planet across the entire lower hemisphere.
[[nodiscard]] Matrix4 orbital_horizon_motion(float previous_scroll,float current_scroll,double alpha,
    bool thin_horizon=false,bool entry_horizon=false,bool gameplay=false);
[[nodiscard]] DrawPacket orbital_planet_sphere_packet(const simulation::SnesPpuState&,
    const BackgroundTileOptions&,bool srgb=false,bool thin_horizon=false,bool entry_horizon=false,bool gameplay=false);
// Source Mode-1 title overlays, in painter order after models and before OAM.
// BG1 is omitted for EX's introductory logo backdrop. Black palette entries
// are opaque here: they supply outlines and intentional model occlusion.
[[nodiscard]] std::vector<DrawPacket> title_foreground_packets(
    const simulation::SnesPpuState&,unsigned brightness,bool include_bg1,
    std::optional<std::array<int16_t,2>> bg2_scroll={},bool srgb=false);
}
