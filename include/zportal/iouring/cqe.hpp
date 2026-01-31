#pragma once

#include <optional>

#include <cstdint>

#include <liburing.h>

#include <zportal/iouring/ring.hpp>

namespace zportal {

class Cqe {
  public:
    friend class IOUring;
    
    std::uint64_t get_data64() const noexcept;
    void* get_data() const noexcept;

    std::int32_t get_result() const noexcept;
    std::uint32_t get_flags() const noexcept;

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] int error() const noexcept;

    [[nodiscard]] std::optional<std::uint16_t> get_buffer_id() const noexcept;

  private:
    explicit Cqe(std::uint64_t user_data, std::int32_t res, std::uint32_t flags) noexcept;
    explicit Cqe(const io_uring_cqe& cqe) noexcept;

    std::uint64_t user_data_;
    std::int32_t res_;
    std::uint32_t flags_;
};

} // namespace zportal