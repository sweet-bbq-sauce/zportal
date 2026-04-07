#pragma once

#include <memory>
#include <optional>
#include <span>

#include <cstddef>
#include <cstdint>

#include <liburing.h>

#include <zportal/tools/error.hpp>

namespace zportal {

class IoUring;

class BufferGroup {
  public:
    ~BufferGroup() noexcept;

    Result<std::span<std::byte>> get_buffer(std::uint16_t bid, std::optional<std::uint32_t> size = {}) noexcept;
    Result<void> return_buffer(std::uint16_t bid) noexcept;

    std::size_t get_buffer_count() const noexcept;
    std::uint32_t get_buffer_size() const noexcept;
    std::uint16_t get_bgid() const noexcept;

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

    BufferGroup(BufferGroup&&) = delete;
    BufferGroup& operator=(BufferGroup&&) = delete;
    BufferGroup(const BufferGroup&) = delete;
    BufferGroup& operator=(const BufferGroup&) = delete;

  private:
    friend class IoUring;
    explicit BufferGroup(io_uring* ring) noexcept;

    io_uring* const ring_;
    io_uring_buf_ring* br_;
    int mask_;

    std::unique_ptr<std::byte[]> data_;
    std::size_t size_;

    std::uint16_t bgid_, buffer_count_;
    std::uint32_t buffer_size_;
};

}; // namespace zportal