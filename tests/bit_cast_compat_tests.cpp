#include "starfox/compat/bit_cast.hpp"
#include "starfox/state/archive.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace {

template<class To, class From>
concept CanBitCast = requires(const From& source) {
    starfox::bit_cast<To>(source);
};

struct NonTrivial {
    NonTrivial() {}
    ~NonTrivial() {}
    std::uint32_t bits{};
};

static_assert(CanBitCast<std::uint32_t, float>);
static_assert(CanBitCast<std::int16_t, std::uint16_t>);
static_assert(!CanBitCast<std::uint64_t, float>);
static_assert(!CanBitCast<std::uint32_t, NonTrivial>);

struct SavedFields {
    std::int16_t signed_value{-2};
    float negative_zero{-0.0F};
};

}

namespace starfox::state {

void serialize(Writer& writer, const SavedFields& fields) {
    writer(fields.signed_value, fields.negative_zero);
}

void serialize(Reader& reader, SavedFields& fields) {
    reader(fields.signed_value, fields.negative_zero);
}

}

int main() try {
    constexpr auto negative_zero_bits = starfox::bit_cast<std::uint32_t>(-0.0F);
    static_assert(negative_zero_bits == 0x80000000U);
    constexpr auto signed_bits = starfox::bit_cast<std::uint16_t>(std::int16_t{-2});
    static_assert(signed_bits == 0xfffeU);

    const SavedFields fields;
    starfox::state::Writer writer;
    writer(fields);
    const std::array<std::uint8_t, 6> expected{0xfe, 0xff, 0x00, 0x00, 0x00, 0x80};
    if (!std::equal(writer.bytes().begin(), writer.bytes().end(), expected.begin(), expected.end()))
        throw std::runtime_error("C++20 bit-cast shim changed save-state bytes");

    SavedFields restored;
    starfox::state::Reader reader(writer.bytes());
    reader(restored);
    reader.finish();
    if (restored.signed_value != fields.signed_value
        || starfox::bit_cast<std::uint32_t>(restored.negative_zero)
            != negative_zero_bits)
        throw std::runtime_error("C++20 bit-cast shim changed signed or floating bits");

    std::cout << "C++20 bit-cast constraints and save-state representation passed.\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
