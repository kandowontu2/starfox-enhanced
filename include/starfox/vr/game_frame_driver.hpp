#pragma once
#include "starfox/vr/game_input.hpp"
#include "starfox/vr/game_scene.hpp"
#include "starfox/simulation/game_simulation.hpp"
#include "starfox/timing/fixed_step.hpp"
#include <functional>
#include <optional>
#include <utility>
namespace starfox::vr {
struct GameFrameAdvance {
    unsigned video_phases{},logic_ticks{},audio_blocks{};
    double raster_fraction{};
    bool duplicate{},time_clamped{};
};
class GameFrameDriver {
public:
    // Callback runs at native 20 Hz even when gameplay uses slower original
    // pace. It must process both event streams and return current APU ports.
    using AudioTick=std::function<std::array<uint8_t,4>(std::span<const simulation::ApuPortWrite>,std::span<const simulation::MsuRegisterWrite>)>;
    // Called once after each 60 Hz source raster is presented, before a
    // possible logic tick; duplicate XR eye submissions never invoke it.
    using SourceRaster=std::function<void()>;
    GameFrameDriver(simulation::GameSimulation& game,AudioTick audio,
        GameSceneHistory* scenes=nullptr,SourceRaster source_raster={});
    GameFrameDriver(const GameFrameDriver&)=delete;
    GameFrameDriver& operator=(const GameFrameDriver&)=delete;
    GameFrameAdvance advance(XrTime predicted_time,const VrControls&,bool focused,
        const PresentationPreferences& presentation={},bool fixed_face_buttons=false);
    // After an explicit stage replacement: discard old input/audio timing.
    void reset_for_scene_change() noexcept;
private:
    simulation::GameSimulation& game_;AudioTick audio_;
    GameSceneHistory* scenes_{}; // Must outlive the driver, if supplied.
    SourceRaster source_raster_;
    timing::FixedStepClock clock_{60};VrGameInput input_;
    std::optional<XrTime> previous_;
    std::vector<simulation::ApuPortWrite> apu_;
    std::vector<simulation::MsuRegisterWrite> msu_;
    unsigned audio_phase_{};bool failed_{};
    double fraction_{};
};
}
