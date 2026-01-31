#pragma once

#include <vector>

#include <cstddef>
#include <cstdint>

#include <liburing.h>

namespace zportal {

class IOUring;

class BufferRing {
  public:
    friend IOUring;

    BufferRing(BufferRing&&) noexcept;
    BufferRing& operator=(BufferRing&&) noexcept;

    BufferRing(const BufferRing&) = delete;
    BufferRing& operator=(const BufferRing&) = delete;

    ~BufferRing() noexcept;
    void close() noexcept;

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] std::uint16_t get_bgid() const noexcept;
    [[nodiscard]] std::uint16_t get_count() const noexcept;

    void return_buffer(std::uint16_t bid) noexcept;
    void flush_returns() noexcept;

    [[nodiscard]] bool is_bid_valid(std::uint16_t bid) const noexcept;

    [[nodiscard]] std::size_t buffers_size() const noexcept;
    [[nodiscard]] std::byte* buffer_ptr(std::uint16_t bid) const noexcept;

  private:
    explicit BufferRing(IOUring& ring, std::uint16_t bgid, std::uint16_t count, std::uint32_t size,
                         std::uint16_t threshold = 10);

    IOUring* ring_;
    io_uring_buf_ring* br_;
    std::uint16_t bgid_;

    std::uint16_t threshold_;
    std::vector<std::uint16_t> to_return_queue_;

    std::byte* data_;
    std::uint32_t size_;
    std::uint16_t count_;
    std::uint16_t mask_;
};

} // namespace zportal