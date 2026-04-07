#pragma once

#include <cstdint>

#include <zportal/session/operation.hpp>

namespace zportal {

inline Operation::Operation(std::uint64_t serialized) noexcept {
    parse(serialized);
}

inline OperationType Operation::get_type() const noexcept {
    return type_;
}

inline void Operation::set_type(OperationType type) noexcept {
    type_ = type;
}

inline void Operation::parse(std::uint64_t serialized) noexcept {
    type_ = static_cast<OperationType>(serialized & 0xFFu);
}

inline std::uint64_t Operation::serialize() const noexcept {
    return static_cast<std::uint64_t>(static_cast<std::uint8_t>(type_));
}

} // namespace zportal
