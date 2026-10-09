#pragma once
#include "starfox/input/buttons.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace starfox::platform::nintendo_3ds {
// Stable native physical source IDs; Circle Pad directions are independent of
// D-pad keys. No SDL IDs or libctru KEY_* values go into a settings file.
inline constexpr std::array<input::ButtonMask,12> physical_button_bits{
    input::b,input::y,input::select,input::start,input::up,input::down,
    input::left,input::right,input::a,input::x,input::left_shoulder,input::right_shoulder};
inline constexpr std::array<std::string_view,16> physical_source_names{
    "B","Y","SELECT","START","D-PAD UP","D-PAD DOWN","D-PAD LEFT","D-PAD RIGHT",
    "A","X","L","R","CIRCLE UP","CIRCLE DOWN","CIRCLE LEFT","CIRCLE RIGHT"};
inline constexpr std::array<input::ButtonMask,12> game_action_bits{
    input::up,input::down,input::left,input::right,input::a,input::b,input::x,input::y,
    input::left_shoulder,input::right_shoulder,input::select,input::start};
inline constexpr std::array<std::string_view,12> game_action_names{
    "UP","DOWN","LEFT","RIGHT","A","B","X","Y","L","R","SELECT","START"};
struct PadSample {
    input::ButtonMask physical{};
    int circle_x{},circle_y{};
};
struct GameBindings {
    static constexpr std::uint8_t unbound=255;
    std::array<std::uint8_t,12> sources{4,5,6,7,8,0,9,1,10,11,2,3};
    std::uint8_t deadzone{40};
    bool operator==(const GameBindings&) const=default;
    [[nodiscard]] bool valid() const noexcept {
        if(deadzone>156) return false;
        for(auto source:sources) if(source>=16 && source!=unbound) return false;
        return true;
    }
};
inline std::uint32_t physical_sources(PadSample sample,unsigned deadzone) {
    if(deadzone>156) throw std::invalid_argument("Invalid native Circle Pad deadzone");
    std::uint32_t result{};
    for(unsigned i=0;i<physical_button_bits.size();++i)
        if(sample.physical&physical_button_bits[i]) result|=1U<<i;
    if(sample.circle_y>int(deadzone)) result|=1U<<12;
    if(sample.circle_y<-int(deadzone)) result|=1U<<13;
    if(sample.circle_x<-int(deadzone)) result|=1U<<14;
    if(sample.circle_x>int(deadzone)) result|=1U<<15;
    return result;
}
inline input::ButtonMask mapped_buttons(const GameBindings& bindings,PadSample sample) {
    if(!bindings.valid()) throw std::invalid_argument("Invalid native input bindings");
    const auto sources=physical_sources(sample,bindings.deadzone);
    input::ButtonMask result{};
    for(unsigned i=0;i<bindings.sources.size();++i) {
        const auto source=bindings.sources[i];
        if(source<16 && (sources&(1U<<source))) result|=game_action_bits[i];
    }
    // The default D-pad bindings also accept the Circle Pad, as before. Once
    // an action is rebound to another key/axis, do not add a hidden old source.
    for(unsigned i=0;i<4;++i)
        if(bindings.sources[i]==4+i && (sources&(1U<<(12+i)))) result|=game_action_bits[i];
    return result;
}
inline input::ButtonMask fixed_menu_buttons(PadSample sample) {
    auto result=sample.physical;
    if(sample.circle_y>40) result|=input::up;
    if(sample.circle_y<-40) result|=input::down;
    if(sample.circle_x<-40) result|=input::left;
    if(sample.circle_x>40) result|=input::right;
    return result;
}
inline std::string_view binding_name(std::uint8_t source) {
    if(source==GameBindings::unbound) return "UNBOUND";
    if(source>=physical_source_names.size()) throw std::invalid_argument("Invalid native physical source");
    return physical_source_names[source];
}
} // namespace starfox::platform::nintendo_3ds
