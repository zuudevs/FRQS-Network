#pragma once

/**
 * @file net/ipv4.hpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <bit>

namespace frqs::net {

constexpr bool IS_LITTLE_ENDIAN = (std::endian::native == std::endian::little) ;

class IPv4 {
public:
    using element_type = uint8_t ;
    using pointer = uint8_t* ;
    using reference = uint8_t& ;
    using const_pointer = const uint8_t* ;
    using const_reference = const uint8_t& ;
    static constexpr uint8_t capacity = 4 ;

    IPv4() noexcept = default ;
    IPv4(const IPv4&) noexcept = default ;
    IPv4(IPv4&&) noexcept = default ;
    IPv4& operator=(const IPv4&) noexcept = default ;
    IPv4& operator=(IPv4&&) noexcept = default ;
    std::strong_ordering operator<=>(const IPv4&) const noexcept = default ;
    ~IPv4() noexcept = default ;

    template <std::integral E, std::size_t N>
    IPv4(const E (&address)[N]) noexcept ;

    explicit IPv4(uint32_t address) noexcept ;
    IPv4(const std::string_view& s) noexcept ;

    IPv4& operator=(uint32_t address) noexcept ;

    operator uint32_t() const noexcept { return toUint32() ; }
    uint32_t toUint32() const noexcept ;
    uint32_t Nbo_toUint32() const noexcept ;

    std::string toString() const noexcept ;

    pointer data() noexcept { return address_.data() ; }
    const_pointer data() const noexcept { return address_.data() ; }

    IPv4 operator|(const IPv4& o) const noexcept ;
    IPv4 operator&(const IPv4& o) const noexcept ;
    IPv4 operator^(const IPv4& o) const noexcept ;
    IPv4 operator~() const noexcept ;

    IPv4& operator|=(const IPv4& o) noexcept ;
    IPv4& operator&=(const IPv4& o) noexcept ;
    IPv4& operator^=(const IPv4& o) noexcept ;

    IPv4 operator+(uint32_t value) const noexcept ;
    IPv4 operator-(uint32_t value) const noexcept ;

    IPv4& operator+=(uint32_t value) noexcept ;
    IPv4& operator-=(uint32_t value) noexcept ;

    IPv4& operator++() noexcept ;
    IPv4& operator--() noexcept ;
    IPv4 operator++(int) noexcept ;
    IPv4 operator--(int) noexcept ;

    static IPv4 mask(uint8_t prefix) noexcept ;

private:
    std::array<uint8_t, 4> address_{} ;

    void append_uint8(std::string& out, uint8_t value) const noexcept ;
    auto invert(std::unsigned_integral auto value) const noexcept -> decltype(value) ;
} ;

// Template implementations
template <std::integral E, std::size_t N>
IPv4::IPv4(const E (&address)[N]) noexcept {
    auto count = std::min(static_cast<std::size_t>(capacity), N) ;
    for(std::size_t i = 0 ; i < count ; i++)
        address_[i] = static_cast<uint8_t>(std::clamp(address[i], E{0}, E{255})) ;
}

} // namespace frqs::net