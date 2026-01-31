#include <cstdint>

#include <liburing.h>

#include <zportal/iouring/cqe.hpp>

std::uint64_t zportal::Cqe::get_data64() const noexcept {
    return user_data_;
}

void* zportal::Cqe::get_data() const noexcept {
    return reinterpret_cast<void*>(user_data_);
}

std::int32_t zportal::Cqe::get_result() const noexcept {
    return res_;
}
std::uint32_t zportal::Cqe::get_flags() const noexcept {
    return flags_;
}

zportal::Cqe::Cqe(std::uint64_t user_data, std::int32_t res, std::uint32_t flags) noexcept
    : user_data_(user_data), res_(res), flags_(flags) {}

zportal::Cqe::Cqe(const io_uring_cqe& cqe) noexcept : user_data_(cqe.user_data), res_(cqe.res), flags_(cqe.flags) {}

bool zportal::Cqe::ok() const noexcept {
    return res_ >= 0;
}

int zportal::Cqe::error() const noexcept {
    return res_ < 0 ? -res_ : 0;
}

std::optional<std::uint16_t> zportal::Cqe::get_buffer_id() const noexcept {
    if (flags_ & IORING_CQE_F_BUFFER)
        return static_cast<std::uint16_t>(flags_ >> IORING_CQE_BUFFER_SHIFT);
    return std::nullopt;
}