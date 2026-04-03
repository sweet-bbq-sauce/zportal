#pragma once

#include <optional>

#include <liburing.h>

#include <zportal/iouring/cqe.hpp>

namespace zportal {

inline std::uint64_t Cqe::user_data64() const noexcept {
    return user_data_;
}

inline void* Cqe::user_data() const noexcept {
    return reinterpret_cast<void*>(user_data_);
}

inline std::int32_t Cqe::result() const noexcept {
    return result_;
}

inline std::uint32_t Cqe::flags() const noexcept {
    return flags_;
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

inline Cqe::Cqe(const io_uring_cqe& cqe) noexcept : user_data_(cqe.user_data), result_(cqe.res), flags_(cqe.flags) {}

} // namespace zportal