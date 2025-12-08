/**
 * @file net/ipv4.cpp
 * @author zuudevs (zuudevs@gmail.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "net/ipv4.hpp"

namespace frqs::net {

void IPv4::append_uint8(std::string& out, uint8_t value) const noexcept {
    if (value >= 100) {
        out += static_cast<char>('0' + (value / 100)) ;
        value %= 100 ;
        out += static_cast<char>('0' + (value / 10)) ;
        value %= 10 ;
    } else if (value >= 10) {
        out += static_cast<char>('0' + (value / 10)) ;
        value %= 10 ;
    }
    out += static_cast<char>('0' + value) ;
}

auto IPv4::invert(std::unsigned_integral auto value) const noexcept -> decltype(value) {
    return IS_LITTLE_ENDIAN ? std::byteswap(value) : value ;
}

IPv4::IPv4(uint32_t address) noexcept {
    address_ = std::bit_cast<std::array<uint8_t, 4>>(invert(address)) ;
}

IPv4::IPv4(const std::string_view& s) noexcept {
    std::array<uint8_t, 4> result{} ;
    std::size_t octet_idx = 0 ;
    uint32_t current_val = 0 ;
    bool parsing_num = false ;

    for (char c : s) {
        if (c >= '0' && c <= '9') {
            current_val = (current_val * 10) + (c - '0') ;
            if (current_val > 255) current_val = 255 ;
            parsing_num = true ;
        } else if (c == '.') {
            if (octet_idx < 4) {
                result[octet_idx++] = static_cast<uint8_t>(current_val) ;
                current_val = 0 ;
                parsing_num = false ;
            }
        }
    }
    if (parsing_num && octet_idx < 4)
        result[octet_idx] = static_cast<uint8_t>(current_val) ;
    address_ = result ;
}

IPv4& IPv4::operator=(uint32_t address) noexcept {
    address_ = std::bit_cast<std::array<uint8_t, 4>>(invert(address)) ;
    return *this ;
}

uint32_t IPv4::toUint32() const noexcept {
    return invert(std::bit_cast<uint32_t>(address_)) ;
}

uint32_t IPv4::Nbo_toUint32() const noexcept {
    return std::bit_cast<uint32_t>(address_) ;
}

std::string IPv4::toString() const noexcept {
    std::string res ;
    res.reserve(15) ;
    append_uint8(res, address_[0]) ;
    res += '.' ;
    append_uint8(res, address_[1]) ;
    res += '.' ;
    append_uint8(res, address_[2]) ;
    res += '.' ;
    append_uint8(res, address_[3]) ;
    return res ;
}

IPv4 IPv4::operator|(const IPv4& o) const noexcept {
    return IPv4(toUint32() | o.toUint32()) ;
}

IPv4 IPv4::operator&(const IPv4& o) const noexcept {
    return IPv4(toUint32() & o.toUint32()) ;
}

IPv4 IPv4::operator^(const IPv4& o) const noexcept {
    return IPv4(toUint32() ^ o.toUint32()) ;
}

IPv4 IPv4::operator~() const noexcept {
    return IPv4(~toUint32()) ;
}

IPv4& IPv4::operator|=(const IPv4& o) noexcept {
    *this = *this | o ;
    return *this ;
}

IPv4& IPv4::operator&=(const IPv4& o) noexcept {
    *this = *this & o ;
    return *this ;
}

IPv4& IPv4::operator^=(const IPv4& o) noexcept {
    *this = *this ^ o ;
    return *this ;
}

IPv4 IPv4::operator+(uint32_t value) const noexcept {
    return IPv4(toUint32() + value) ;
}

IPv4 IPv4::operator-(uint32_t value) const noexcept {
    return IPv4(toUint32() - value) ;
}

IPv4& IPv4::operator+=(uint32_t value) noexcept {
    *this = *this + value ;
    return *this ;
}

IPv4& IPv4::operator-=(uint32_t value) noexcept {
    *this = *this - value ;
    return *this ;
}

IPv4& IPv4::operator++() noexcept {
    *this += 1u ;
    return *this ;
}

IPv4& IPv4::operator--() noexcept {
    *this -= 1u ;
    return *this ;
}

IPv4 IPv4::operator++(int) noexcept {
    auto r = *this ;
    *this += 1u ;
    return r ;
}

IPv4 IPv4::operator--(int) noexcept {
    auto r = *this ;
    *this -= 1u ;
    return r ;
}

IPv4 IPv4::mask(uint8_t prefix) noexcept {
    if (prefix == 0) return IPv4(0u) ;
    if (prefix >= 32) return IPv4(~0u) ;
    return IPv4((~uint32_t{0}) << (32 - std::clamp(prefix, uint8_t{0}, uint8_t{32}))) ;
}

} // namespace frqs::net