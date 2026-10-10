#include "starfox/assets/rom.hpp"
#include "starfox/simulation/map_vm.hpp"
#include "starfox/simulation/rumble_sequencer.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}

int main() try {
    const starfox::assets::SymbolMap symbols = starfox::assets::SymbolMap::parse(
        "RUMBLE_CMD $0000ED\n"
        "RUMBLE_TIME $0000EE\n"
        "RUMBLE_INDEX $0000EF\n"
        "RUMBLE_TABLE $0019FB\n");
    const starfox::assets::RomImage rom(std::vector<std::uint8_t>(0x8000U));
    starfox::simulation::ObjectPool objects;
    starfox::simulation::MapVm map(rom,
        starfox::simulation::MapDatabase(rom, 0U, 0U), objects);
    const starfox::simulation::RumbleSequencer sequencer(symbols);
    require(sequencer.available(), "valid rumble symbols were rejected");

    map.write_native_byte(0x00edU, 0x24U);
    map.write_native_byte(0x00eeU, 2U);
    map.write_native_byte(0x00efU, 0U);
    require(!sequencer.advance(map, false), "disabled rumble emitted an effect");
    require(map.read_native_byte(0x00eeU) == 2U,
        "disabled rumble changed the cartridge timer");
    const auto command = sequencer.advance(map, true);
    require(command == starfox::simulation::RumbleEffect{0x2222U, 0x4444U, 40U},
        "command register frequency conversion changed");
    require(map.read_native_byte(0x00eeU) == 1U
        && map.read_native_byte(0x00efU) == 0U,
        "command register timer/index progression changed");
    require(sequencer.advance(map, true)
            == starfox::simulation::RumbleEffect{0x2222U, 0x4444U, 40U}
        && map.read_native_byte(0x00eeU) == 0U,
        "active command did not retain its output until its timer expired");
    require(sequencer.advance(map, true)
            == starfox::simulation::RumbleEffect{0U, 0U, 40U},
        "expired command did not silence the actuator");

    map.write_native_byte(0x0019fbU, 0x12U);
    map.write_native_byte(0x0019fcU, 0x91U); // Cartridge loop marker.
    map.write_native_byte(0x0019fdU, 0x34U);
    map.write_native_byte(0x0019feU, 0x19U); // Cartridge stop marker.
    map.write_native_byte(0x00eeU, 3U);
    map.write_native_byte(0x00efU, 1U);
    require(sequencer.advance(map, true)
            == starfox::simulation::RumbleEffect{0x1111U, 0x2222U, 40U}
        && map.read_native_byte(0x00efU) == 2U,
        "sequence table byte did not advance its native index");
    require(sequencer.advance(map, true)
            == starfox::simulation::RumbleEffect{0x1111U, 0x2222U, 40U}
        && map.read_native_byte(0x00efU) == 2U,
        "loop marker did not restart at the first authored table byte");
    map.write_native_byte(0x00efU, 4U);
    require(sequencer.advance(map, true)
            == starfox::simulation::RumbleEffect{0U, 0U, 40U}
        && map.read_native_byte(0x00efU) == 0U,
        "stop marker did not reset the native sequence index");

    const auto missing = starfox::assets::SymbolMap::parse("RUMBLE_CMD $0000ED\n");
    const starfox::simulation::RumbleSequencer unavailable(missing);
    require(!unavailable.available() && !unavailable.advance(map, true),
        "incomplete symbol set advanced a rumble sequence");
    std::cout << "Cartridge rumble register sequencing and authored markers passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
