#pragma once

#include <bit>
#include <type_traits>

#if defined(__has_builtin)
#    if __has_builtin(__builtin_bit_cast)
#        define STARFOX_COMPILER_HAS_BIT_CAST 1
#    endif
#endif

#if (!defined(__cpp_lib_bit_cast) || __cpp_lib_bit_cast < 201806L \
        || defined(STARFOX_FORCE_BIT_CAST_BUILTIN)) \
    && !defined(STARFOX_COMPILER_HAS_BIT_CAST)
#error "Star Fox requires std::bit_cast or the compiler bit-cast builtin"
#endif

namespace starfox {

// GCC 10's libstdc++ ships <bit> but predates std::bit_cast. Keep its exact
// C++20 constraints while using Clang's equivalent builtin on that toolchain.
template<class To, class From>
    requires (sizeof(To) == sizeof(From)
        && std::is_trivially_copyable_v<To>
        && std::is_trivially_copyable_v<From>)
[[nodiscard]] constexpr To bit_cast(const From& source) noexcept {
#if defined(STARFOX_FORCE_BIT_CAST_BUILTIN) \
    || !defined(__cpp_lib_bit_cast) || __cpp_lib_bit_cast < 201806L
    return __builtin_bit_cast(To, source);
#else
    return std::bit_cast<To>(source);
#endif
}

} // namespace starfox

#undef STARFOX_COMPILER_HAS_BIT_CAST
