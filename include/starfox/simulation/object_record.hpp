#pragma once

#include "starfox/compat/bit_cast.hpp"
#include "starfox/simulation/object_pool.hpp"

#include <algorithm>
#include <span>
#include <stdexcept>

namespace starfox::simulation::object_record {

// Do not memcpy GameObject: the cartridge records are packed little-endian,
// differ between Original and EX, and intentionally omit host-only members.
inline void check_size(ObjectMemoryLayout layout, std::size_t size, bool extended) {
    const auto expected = extended
        ? (layout == ObjectMemoryLayout::starfox_ex ? 56U : 54U)
        : (layout == ObjectMemoryLayout::starfox_ex ? 57U : 56U);
    if (size != expected) throw std::invalid_argument{"incorrect native object record size"};
}

inline void encode_base(const GameObject& o, ObjectMemoryLayout layout,
                        std::span<std::uint8_t> b) {
    check_size(layout, b.size(), false);
    const auto word = [&b](std::size_t offset, std::uint16_t value) {
        b[offset] = static_cast<std::uint8_t>(value);
        b[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
    };
    word(4, o.shape); word(6, o.attached);
    b[8] = o.flags; b[9] = o.type; b[10] = o.count; b[11] = o.count1;
    word(12, starfox::bit_cast<std::uint16_t>(o.world_x));
    word(14, starfox::bit_cast<std::uint16_t>(o.world_y));
    word(16, starfox::bit_cast<std::uint16_t>(o.world_z));
    b[18] = o.rotation_x; b[19] = o.rotation_y; b[20] = o.rotation_z;
    b[21] = starfox::bit_cast<std::uint8_t>(o.velocity);
    b[22] = static_cast<std::uint8_t>(o.strategy_address);
    b[23] = static_cast<std::uint8_t>(o.strategy_address >> 8U);
    b[24] = static_cast<std::uint8_t>(o.strategy_address >> 16U);
    word(25, o.immune_object); word(27, o.collision_object);
    std::copy(o.strategy_flags.begin(), o.strategy_flags.end(), b.begin() + 29);
    b[33] = starfox::bit_cast<std::uint8_t>(o.skid_y);
    for (std::size_t i = 0; i < 4; ++i)
        b[34 + i] = starfox::bit_cast<std::uint8_t>(o.scratch_bytes[i]);
    word(38, starfox::bit_cast<std::uint16_t>(o.scratch_words[0]));
    word(40, starfox::bit_cast<std::uint16_t>(o.scratch_words[1]));
    b[42] = o.health; b[43] = o.attack_power;
    const bool ex = layout == ObjectMemoryLayout::starfox_ex;
    b[ex ? 53 : 44] = o.weapon_type;
    b[ex ? 44 : 45] = o.collision_count;
    b[ex ? 45 : 46] = o.collision_flags;
    word(ex ? 46 : 47, starfox::bit_cast<std::uint16_t>(o.velocity_x));
    word(ex ? 48 : 49, starfox::bit_cast<std::uint16_t>(o.velocity_y));
    word(ex ? 50 : 51, starfox::bit_cast<std::uint16_t>(o.velocity_z));
    b[ex ? 52 : 53] = o.hit_flags;
    if (ex) b[54] = o.open_al;
    b[ex ? 55 : 54] = starfox::bit_cast<std::uint8_t>(o.scratch_bytes[4]);
    b[ex ? 56 : 55] = starfox::bit_cast<std::uint8_t>(o.scratch_bytes[5]);
}

inline void decode_base(GameObject& o, ObjectMemoryLayout layout,
                        std::span<const std::uint8_t> b) {
    check_size(layout, b.size(), false);
    const auto word = [&b](std::size_t offset) {
        return static_cast<std::uint16_t>(b[offset] | (std::uint16_t{b[offset + 1U]} << 8U));
    };
    o.shape = word(4); o.attached = word(6);
    o.flags = b[8]; o.type = b[9]; o.count = b[10]; o.count1 = b[11];
    o.world_x = starfox::bit_cast<std::int16_t>(word(12));
    o.world_y = starfox::bit_cast<std::int16_t>(word(14));
    o.world_z = starfox::bit_cast<std::int16_t>(word(16));
    o.rotation_x = b[18]; o.rotation_y = b[19]; o.rotation_z = b[20];
    o.velocity = starfox::bit_cast<std::int8_t>(b[21]);
    // The bytewise adapter overwrites only the cartridge's low 24 bits.
    o.strategy_address = (o.strategy_address & 0xff000000U) | b[22]
        | (std::uint32_t{b[23]} << 8U) | (std::uint32_t{b[24]} << 16U);
    o.immune_object = word(25); o.collision_object = word(27);
    std::copy_n(b.begin() + 29, 4, o.strategy_flags.begin());
    o.skid_y = starfox::bit_cast<std::int8_t>(b[33]);
    for (std::size_t i = 0; i < 4; ++i)
        o.scratch_bytes[i] = starfox::bit_cast<std::int8_t>(b[34 + i]);
    o.scratch_words[0] = starfox::bit_cast<std::int16_t>(word(38));
    o.scratch_words[1] = starfox::bit_cast<std::int16_t>(word(40));
    o.health = b[42]; o.attack_power = b[43];
    const bool ex = layout == ObjectMemoryLayout::starfox_ex;
    o.weapon_type = b[ex ? 53 : 44];
    o.collision_count = b[ex ? 44 : 45]; o.collision_flags = b[ex ? 45 : 46];
    o.velocity_x = starfox::bit_cast<std::int16_t>(word(ex ? 46 : 47));
    o.velocity_y = starfox::bit_cast<std::int16_t>(word(ex ? 48 : 49));
    o.velocity_z = starfox::bit_cast<std::int16_t>(word(ex ? 50 : 51));
    o.hit_flags = b[ex ? 52 : 53];
    if (ex) o.open_al = b[54];
    o.scratch_bytes[4] = starfox::bit_cast<std::int8_t>(b[ex ? 55 : 54]);
    o.scratch_bytes[5] = starfox::bit_cast<std::int8_t>(b[ex ? 56 : 55]);
}

inline void decode_extended(GameObject& o, ObjectMemoryLayout layout,
                            std::span<const std::uint8_t> b) {
    check_size(layout, b.size(), true);
    std::copy(b.begin(), b.end(), o.extended.begin());
    o.strategy_state = b[18];
    o.fire_object = static_cast<ObjectHandle>(b[19] | (std::uint16_t{b[20]} << 8U));
    const auto shift = layout == ObjectMemoryLayout::starfox_ex ? 2U : 0U;
    o.colour_frame = b[28U + shift]; o.animation_frame = b[29U + shift];
    o.sound1 = b[30U + shift]; o.sound2 = b[31U + shift];
    o.colour_table = static_cast<std::uint16_t>(
        b[32U + shift] | (std::uint16_t{b[33U + shift]} << 8U));
    o.texture_scroll_x = b[42U + shift]; o.texture_scroll_y = b[43U + shift];
}

} // namespace starfox::simulation::object_record
