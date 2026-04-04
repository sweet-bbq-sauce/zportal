#pragma once

#include <optional>

#include <cstdint>

#include <liburing.h>

namespace zportal {

class IoUring;

class Cqe {
  public:
    std::uint64_t user_data64() const noexcept;
    void* user_data() const noexcept;
    std::int32_t result() const noexcept;
    std::uint32_t flags() const noexcept;

    std::optional<std::uint16_t> bid() const noexcept;
    bool more() const noexcept;

    bool ok() const noexcept;
    explicit operator bool() const noexcept;

    std::int32_t error() const noexcept;

  private:
    friend class IoUring;
    explicit Cqe(const io_uring_cqe& cqe) noexcept;

    std::uint64_t user_data_;
    std::int32_t result_;
    std::uint32_t flags_;
};

} // namespace zportal

#include <zportal/iouring/cqe.inl>