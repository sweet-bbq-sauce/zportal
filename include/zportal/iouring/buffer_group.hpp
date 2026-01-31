#pragma once

#include <vector>

#include <cstddef>
#include <cstdint>

#include <liburing.h>

namespace zportal {

class IOUring;

class BufferGroup {
  public:
    friend IOUring;

    BufferGroup(BufferGroup&&) noexcept;
    BufferGroup& operator=(BufferGroup&&) noexcept;

    BufferGroup(const BufferGroup&) = delete;
    BufferGroup& operator=(const BufferGroup&) = delete;

    ~BufferGroup() noexcept;
    void close() noexcept;

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] std::uint16_t get_bgid() const noexcept;
    [[nodiscard]] std::uint16_t get_count() const noexcept;

    void return_buffer(std::uint16_t bid) noexcept;
    void flush_return() noexcept;

    [[nodiscard]] bool is_bid_valid(std::uint16_t bid) const noexcept;

    [[nodiscard]] std::size_t buffers_size() const noexcept;
    [[nodiscard]] std::byte* buffer_ptr(std::uint16_t bid) const noexcept;

  private:
    explicit BufferGroup(IOUring& ring, std::uint16_t bgid, std::uint16_t count, std::uint32_t size,
                         std::uint16_t threshold = 10);

    IOUring* ring_;
    std::uint16_t bgid_;
    io_uring_buf_ring* br_;

    std::uint16_t threshold_;
    std::vector<std::uint16_t> to_return_queue_;

    std::byte* data_;
    std::uint16_t count_;
    std::uint32_t size_;
    std::uint16_t mask_;
};

} // namespace zportal