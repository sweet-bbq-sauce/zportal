#pragma once

#include <optional>

#include <liburing.h>

#include <zportal/iouring/cqe.hpp>
#include <zportal/session/operation.hpp>

namespace zportal {

inline std::int32_t Cqe::result() const noexcept {
    return result_;
}

inline std::uint32_t Cqe::flags() const noexcept {
    return flags_;
}

inline Operation Cqe::operation() const noexcept {
    return operation_;
}

inline std::optional<std::uint16_t> Cqe::bid() const noexcept {
    if (flags_ & IORING_CQE_F_BUFFER)
        return static_cast<std::uint16_t>(flags_ >> IORING_CQE_BUFFER_SHIFT);

    return std::nullopt;
}

inline bool Cqe::more() const noexcept {
    return flags_ & IORING_CQE_F_MORE;
}

inline bool Cqe::ok() const noexcept {
    return result_ >= 0;
}

inline Cqe::operator bool() const noexcept {
    return ok();
}

inline std::int32_t Cqe::error() const noexcept {
    return ok() ? 0 : -result_;
}

inline Cqe::Cqe(const io_uring_cqe& cqe) noexcept : operation_(cqe.user_data), result_(cqe.res), flags_(cqe.flags) {}

} // namespace zportal