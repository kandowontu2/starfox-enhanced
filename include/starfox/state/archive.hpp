#pragma once

#include "starfox/compat/bit_cast.hpp"

#include <array>
#include <algorithm>
#include <bit>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <unordered_map>

namespace starfox::state {

// Explicit little-endian fields only. Never persist object memory, pointers,
// padding, size_t-dependent containers, or a compiler's struct layout.
class Writer {
public:
    template<class... T> void operator()(const T&... fields) { (value(fields), ...); }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
    template<class T, std::size_t N> void fixed(std::span<T,N> fields) {
        if constexpr (std::same_as<std::remove_cv_t<T>,std::uint8_t>)
            bytes_.insert(bytes_.end(),fields.begin(),fields.end());
        else for (const auto& field:fields) value(field);
    }

private:
    template<std::integral T> void value(T field) {
        if constexpr (std::same_as<T, bool>) {
            bytes_.push_back(field ? 1U : 0U);
        } else {
            using U = std::make_unsigned_t<T>;
            const auto bits = starfox::bit_cast<U>(field);
            for (unsigned i=0; i<sizeof(T); ++i)
                bytes_.push_back(static_cast<std::uint8_t>(bits >> (i*8U)));
        }
    }
    template<class T> requires std::is_enum_v<T>
    void value(T field) { value(static_cast<std::underlying_type_t<T>>(field)); }
    template<std::floating_point T> void value(T field) {
        static_assert(sizeof(T)==4 || sizeof(T)==8);
        using U = std::conditional_t<sizeof(T)==4, std::uint32_t, std::uint64_t>;
        value(starfox::bit_cast<U>(field));
    }
    template<class T, std::size_t N> void value(const std::array<T,N>& fields) {
        for (const auto& field : fields) value(field);
    }
    template<class T> void value(const std::vector<T>& fields) {
        if (fields.size()>std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error{"save-state vector is too large"};
        value(static_cast<std::uint32_t>(fields.size()));
        if constexpr (std::same_as<T,std::uint8_t>)
            bytes_.insert(bytes_.end(),fields.begin(),fields.end());
        else for (const auto& field : fields) value(field);
    }
    template<class T> void value(const std::optional<T>& field) {
        value(field.has_value());
        if (field) value(*field);
    }
    template<std::integral K,class V> void value(const std::unordered_map<K,V>& fields) {
        std::vector<K> keys;
        keys.reserve(fields.size());
        for(const auto& [key,unused]:fields) keys.push_back(key);
        std::sort(keys.begin(),keys.end());
        if(keys.size()>std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error{"save-state map is too large"};
        value(static_cast<std::uint32_t>(keys.size()));
        for(const auto key:keys) { value(key); value(fields.at(key)); }
    }
    template<class T> requires (!std::is_arithmetic_v<T> && !std::is_enum_v<T>)
    void value(const T& field) { serialize(*this, field); }
    std::vector<std::uint8_t> bytes_;
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {
        if (bytes.size()>64U*1024U*1024U) throw std::runtime_error{"save state is too large"};
    }
    template<class... T> void operator()(T&... fields) { (value(fields), ...); }
    // Fixed, already allocated machine RAM. Keep its address stable because
    // the CPU bus maps pointers into it. Length is part of the component schema.
    template<class T, std::size_t N> void fixed(std::span<T,N> fields) {
        if constexpr (std::same_as<T,std::uint8_t>) {
            if(bytes_.size()<fields.size()) throw std::runtime_error{"truncated save state"};
            std::copy_n(bytes_.begin(),fields.size(),fields.begin());
            bytes_=bytes_.subspan(fields.size());
        } else for (auto& field:fields) value(field);
    }
    void finish() const {
        if (!bytes_.empty()) throw std::runtime_error{"save state has trailing fields"};
    }
    [[nodiscard]] bool empty() const noexcept { return bytes_.empty(); }

private:
    template<std::integral T> void value(T& field) {
        if (bytes_.size()<sizeof(T)) throw std::runtime_error{"truncated save state"};
        if constexpr (std::same_as<T,bool>) {
            if (bytes_[0]>1U) throw std::runtime_error{"invalid save-state boolean"};
            field=bytes_[0]!=0U;
        } else {
            using U = std::make_unsigned_t<T>;
            U bits{};
            for (unsigned i=0;i<sizeof(T);++i)
                bits |= static_cast<U>(static_cast<U>(bytes_[i]) << (i*8U));
            field=starfox::bit_cast<T>(bits);
        }
        bytes_=bytes_.subspan(sizeof(T));
    }
    template<class T> requires std::is_enum_v<T>
    void value(T& field) {
        std::underlying_type_t<T> raw{}; value(raw); field=static_cast<T>(raw);
    }
    template<std::floating_point T> void value(T& field) {
        static_assert(sizeof(T)==4 || sizeof(T)==8);
        using U = std::conditional_t<sizeof(T)==4,std::uint32_t,std::uint64_t>;
        U bits{}; value(bits); field=starfox::bit_cast<T>(bits);
    }
    template<class T,std::size_t N> void value(std::array<T,N>& fields) {
        for (auto& field:fields) value(field);
    }
    template<class T> void value(std::vector<T>& fields) {
        std::uint32_t count{}; value(count);
        // Every supported serialized element consumes at least one byte.
        // Charge allocation separately: nested containers cannot multiply
        // a tiny input into unbounded allocations before validation.
        if (count>bytes_.size() || count>allocation_budget_/sizeof(T))
            throw std::runtime_error{"invalid save-state vector length"};
        allocation_budget_-=static_cast<std::size_t>(count)*sizeof(T);
        std::vector<T> decoded(count);
        if constexpr (std::same_as<T,std::uint8_t>) {
            std::copy_n(bytes_.begin(),count,decoded.begin());
            bytes_=bytes_.subspan(count);
        } else for (auto& field:decoded) value(field);
        fields=std::move(decoded);
    }
    template<class T> void value(std::optional<T>& field) {
        bool present{}; value(present);
        if (present) { T decoded{}; value(decoded); field=std::move(decoded); }
        else field.reset();
    }
    template<std::integral K,class V> void value(std::unordered_map<K,V>& fields) {
        std::uint32_t count{}; value(count);
        constexpr auto entry_budget=sizeof(std::pair<const K,V>)+sizeof(void*)*4U;
        if(count>bytes_.size()/2U || count>allocation_budget_/entry_budget)
            throw std::runtime_error{"invalid save-state map length"};
        allocation_budget_-=static_cast<std::size_t>(count)*entry_budget;
        std::unordered_map<K,V> decoded;
        decoded.reserve(count);
        std::optional<K> previous;
        for(std::uint32_t i=0;i<count;++i) {
            K key{}; V field{}; value(key); value(field);
            if(previous && key<=*previous) throw std::runtime_error{"noncanonical save-state map"};
            previous=key;
            decoded.emplace(key,std::move(field));
        }
        fields=std::move(decoded);
    }
    template<class T> requires (!std::is_arithmetic_v<T> && !std::is_enum_v<T>)
    void value(T& field) { serialize(*this,field); }
    std::span<const std::uint8_t> bytes_;
    std::size_t allocation_budget_{64U*1024U*1024U};
};

} // namespace starfox::state
