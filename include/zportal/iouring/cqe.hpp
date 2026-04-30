#pragma once

#include <optional>

#include <cstdint>

#include <liburing.h>

#include <zportal/session/operation.hpp>

namespace zportal {

class IoUring;

class Cqe {
  public:
    std::int32_t result() const noexcept;
    std::uint32_t flags() const noexcept;
    Operation operation() const noexcept;

    std::optional<std::uint16_t> bid() const noexcept;
    bool more() const noexcept;

    bool ok() const noexcept;
    explicit operator bool() const noexcept;

    std::int32_t error() const noexcept;

  private:
    friend class IoUring;
    explicit Cqe(const io_uring_cqe& cqe) noexcept;

    Operation operation_;
    std::int32_t result_;
    std::uint32_t flags_;
};

} // namespace zportal

#include <zportal/iouring/cqe.inl>