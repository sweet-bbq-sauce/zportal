#pragma once

#include <cstdint>

#include <zportal/iouring/cqe.hpp>
#include <zportal/tunnel/operation.hpp>

namespace zportal {

inline Operation::Operation(std::uint64_t serialized) noexcept {
    parse(serialized);
}

inline Operation::Operation(const Cqe& cqe) noexcept {
    parse(cqe.user_data64());
}

inline Operation::Type Operation::get_type() const noexcept {
    return type_;
}

inline void Operation::set_type(Type type) noexcept {
    type_ = type;
}

inline void Operation::parse(std::uint64_t serialized) noexcept {
    type_ = static_cast<Type>(serialized & 0xFFu);
}

inline std::uint64_t Operation::serialize() const noexcept {
    return static_cast<std::uint64_t>(static_cast<std::uint8_t>(type_));
}

} // namespace zportal
