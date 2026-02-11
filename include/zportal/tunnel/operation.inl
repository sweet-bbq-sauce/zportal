#pragma once

#include <cstdint>

#include <zportal/tunnel/operation.hpp>

namespace zportal {

inline Operation::Operation(std::uint64_t serialized) noexcept
    : type_(static_cast<Type>(serialized & 0xFFu)), bid_(static_cast<std::uint16_t>((serialized >> 8u) & 0xFFFFu)) {}

inline Operation::Type Operation::get_type() const noexcept {
    return type_;
}

inline std::uint16_t Operation::get_bid() const noexcept {
    return bid_;
}

inline void Operation::set_type(Type type) noexcept {
    type_ = type;
}

inline void Operation::set_bid(std::uint16_t bid) noexcept {
    bid_ = bid;
}

inline std::uint64_t Operation::serialize() const noexcept {
    return static_cast<std::uint64_t>(static_cast<std::uint8_t>(type_)) | (static_cast<std::uint64_t>(bid_) << 8u);
}

} // namespace zportal
