#pragma once
#include "starfox/platform/nintendo_3ds/cockpit_layout.hpp"
#include <memory>

#include "starfox/input/buttons.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace starfox::platform::nintendo_3ds {
inline constexpr unsigned top_width=400, bottom_width=320, screen_height=240;

enum class ScreenUse { setup, menu_preview, world, front_end };
struct StereoSettings {
    float strength{1}, separation{12}, convergence{1024}; // Source world units.
    float focal_x{256}, focal_y{256}, near_plane{1}, far_plane{65536};
};
struct Eye { float x{}, projection_offset{}; };
struct FramePlan {
    bool stereo{};
    float slider{}, separation{}, convergence{}, focal_x{}, focal_y{}, near_plane{}, far_plane{};
    std::array<Eye,2> eyes{};
    unsigned eye_count{1};
};
// Sample hardware once before rendering. Both eyes must consume this immutable
// plan and the same game snapshot; moving the slider never advances simulation.
inline FramePlan plan_frame(float slider,bool stereoscopic_hardware,ScreenUse use,
    const StereoSettings& settings={}) {
    for(float value:{settings.strength,settings.separation,settings.convergence,
                    settings.focal_x,settings.focal_y,settings.near_plane,settings.far_plane})
        if(!std::isfinite(value)) throw std::invalid_argument("Non-finite 3DS stereo settings");
    if(settings.strength<0 || settings.strength>2 || settings.separation<0 || settings.separation>64
        || settings.near_plane<=0 || settings.convergence<=settings.near_plane
        || settings.convergence>=settings.far_plane || settings.far_plane>65536
        || settings.focal_x<=0 || settings.focal_y<=0)
        throw std::invalid_argument("Invalid 3DS stereo settings");
    if(!std::isfinite(slider) || slider<0) slider=0;
    if(slider>1) slider=1;
    const bool world=use==ScreenUse::world || use==ScreenUse::menu_preview;
    FramePlan result;
    result.slider=world && stereoscopic_hardware?slider:0;
    result.separation=result.slider*settings.strength*settings.separation;
    result.stereo=result.separation>0;
    result.eye_count=result.stereo?2:1;
    result.convergence=settings.convergence;
    result.focal_x=settings.focal_x;result.focal_y=settings.focal_y;
    result.near_plane=settings.near_plane;result.far_plane=settings.far_plane;
    for(unsigned eye=0;eye<2;++eye) {
        result.eyes[eye].x=result.separation*(eye?.5F:-.5F);
        result.eyes[eye].projection_offset=settings.focal_x*result.eyes[eye].x/settings.convergence;
    }
    return result;
}
// Parallel off-axis cameras, not toe-in or a displacement of a finished image.
// Input is camera-local X-right/Y-up/Z-forward, in cartridge world units.
inline std::optional<std::array<float,2>> project(const FramePlan& frame,unsigned eye,
    float x,float y,float z) {
    if(eye>=frame.eye_count) throw std::invalid_argument("Inactive 3DS eye");
    if(!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
        || z<frame.near_plane || z>frame.far_plane) return std::nullopt;
    const std::array<float,2> point{top_width*.5F+frame.focal_x*(x-frame.eyes[eye].x)/z
        +frame.eyes[eye].projection_offset,screen_height*.5F-frame.focal_y*y/z};
    if(!std::isfinite(point[0]) || !std::isfinite(point[1])) return std::nullopt;
    return point;
}
inline float background_offset(const FramePlan& frame,unsigned eye) {
    if(eye>=frame.eye_count) throw std::invalid_argument("Inactive 3DS background eye");
    return frame.eyes[eye].projection_offset; // Infinite-distance scenery, not screen-depth art.
}

// libctru KEY_* positions. Names follow Nintendo's printed face buttons, which
// already match the SNES arrangement; do not inherit Xbox/Deck face swaps.
inline input::ButtonMask buttons(std::uint32_t held,int circle_x=0,int circle_y=0,int deadzone=40) {
    if(deadzone<0 || deadzone>156) throw std::invalid_argument("Invalid 3DS Circle Pad deadzone");
    constexpr std::array<input::ButtonMask,12> map{input::a,input::b,input::select,input::start,
        input::right,input::left,input::up,input::down,input::right_shoulder,input::left_shoulder,input::x,input::y};
    input::ButtonMask result{};
    for(unsigned bit=0;bit<map.size();++bit) if(held&(1U<<bit)) result|=map[bit];
    if(circle_x>deadzone) result|=input::right;
    if(circle_x<-deadzone) result|=input::left;
    if(circle_y>deadzone) result|=input::up;
    if(circle_y<-deadzone) result|=input::down;
    return result;
}

struct Rgb { std::uint8_t r{},g{},b{}; bool operator==(const Rgb&) const=default; };
struct ImageView {
    std::span<const std::uint8_t> pixels;
    unsigned width{},height{},pitch{}; // Top-left, row-major RGB24; no eye packing.
};
inline bool valid_image(ImageView image,unsigned width,unsigned height) noexcept {
    return width>0 && width<=top_width && height>0 && height<=screen_height
        && image.width==width && image.height==height && image.pitch>=width*3
        && image.pitch<=4096 && image.pixels.size()>=std::size_t(image.pitch)*(height-1)+width*3;
}
// Nintendo LCD memory is rotated and BGR, unlike our top-left RGB canvases.
// Validate first so a malformed eye can never publish a half-updated screen.
inline void copy_lcd(ImageView source,std::span<std::uint8_t> destination) {
    if((source.width!=top_width && source.width!=bottom_width)
        || !valid_image(source,source.width,screen_height)
        || destination.size()<std::size_t(source.width)*screen_height*3)
        throw std::invalid_argument("Invalid 3DS LCD image");
    const auto src_address=reinterpret_cast<std::uintptr_t>(source.pixels.data());
    const auto dst_address=reinterpret_cast<std::uintptr_t>(destination.data());
    const auto dst_bytes=std::size_t(source.width)*screen_height*3;
    if((dst_address>=src_address && dst_address-src_address<source.pixels.size())
        || (src_address>=dst_address && src_address-dst_address<dst_bytes))
        throw std::invalid_argument("3DS LCD conversion requires independent storage");
    for(unsigned y=0;y<screen_height;++y) for(unsigned x=0;x<source.width;++x) {
        const auto src=std::size_t(y)*source.pitch+x*3;
        const auto dst=(std::size_t(x)*screen_height+(screen_height-1-y))*3;
        destination[dst]=source.pixels[src+2];destination[dst+1]=source.pixels[src+1];destination[dst+2]=source.pixels[src];
    }
}

class Canvas {
public:
    explicit Canvas(unsigned width=bottom_width):width_(width) {
        if(width!=bottom_width && width!=top_width) throw std::invalid_argument("Invalid 3DS canvas");
        pixels_.resize(std::size_t(width)*screen_height*3);
    }
    Canvas(unsigned width,unsigned height):width_(width),height_(height) {
        if(!width || width>top_width || !height || height>screen_height) throw std::invalid_argument("Invalid 3DS artwork canvas");
        pixels_.resize(std::size_t(width)*height*3);
    }
    ImageView view() const {return {pixels_,width_,height_,width_*3};}
    void clear(Rgb colour);
    void rectangle(int x,int y,int width,int height,Rgb colour);
    void line(int x0,int y0,int x1,int y1,Rgb colour);
    void text(int x,int y,std::string_view text,Rgb colour,unsigned scale=1,
        unsigned box_width=0,unsigned box_height=0);
    void image(int x,int y,ImageView source);
    void begin_artwork() {coverage_.assign(std::size_t(width_)*height_,0);}
    [[nodiscard]] std::span<const std::uint8_t> coverage() const {return coverage_;}
    void scaled_artwork(int x,int y,ImageView,std::span<const std::uint8_t> coverage,unsigned quarters);
    void write_bmp(std::string_view path) const; // Host diagnostic only, never per-frame.
private:
    void pixel(int x,int y,Rgb colour);
    unsigned width_,height_{screen_height};std::vector<std::uint8_t> pixels_,coverage_;
};
struct HudCounters {
    unsigned lives{},bombs{}; // Reserve lives, not total ships including active one.
    bool operator==(const HudCounters&) const=default;
};
struct HudState {
    unsigned shield_percent{100},boost_percent{100},lives{},bombs{};
    std::optional<unsigned> boss_percent;
    std::array<std::optional<unsigned>,3> ally_percent{};
    std::string_view radio_message;
    ImageView portrait{};
    // Cartridge-font radio artwork, prepared independently of the world LCD.
    ImageView radio_artwork{};
    bool meters_enabled{true},boost_enabled{true},counters_enabled{true};
    std::optional<unsigned> second_shield_percent;
    bool second_player_view{};
    std::optional<HudCounters> second_counters;
    CockpitLayout layout{};
};
// Lazy, bounded isolated HUD panels, never cropped from a composed world.
class CockpitWidgets {
public:
    void draw(Canvas&,const HudState&);
    [[nodiscard]] std::size_t bytes() const noexcept;
private:
    std::array<std::unique_ptr<Canvas>,hud_widget_count> panels_;
};
// Independent lower-screen drawing. No crop/erase of an already composed
// world image; source sprites/messages must be routed before composition.
void draw_cockpit(Canvas& destination,const HudState& state);

// The original 3DS should not rasterize an unchanged dashboard for every top-
// screen eye or presentation. Own the small source inputs: a retained string
// view or portrait pointer alone cannot detect in-place source updates.
class CockpitDashboard {
public:
    // Returns true only when the lower LCD artwork was redrawn. Sampling the
    // slider does not invalidate this cache. No drawing on an invalid input.
    bool update(const HudState& state);
    [[nodiscard]] ImageView view() const {return canvas_.view();}
private:
    Canvas canvas_;
    CockpitWidgets widgets_;
    std::optional<HudState> previous_;
    std::string message_;
    std::vector<std::uint8_t> portrait_,radio_;
};
} // namespace starfox::platform::nintendo_3ds
