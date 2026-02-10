#pragma once

#include <cstdint>

#include <liburing.h>

namespace zportal {

class Cqe;
class BufferRing;

class IOUring {
  public:
    explicit IOUring(std::uint32_t entries);

    IOUring(IOUring&&) noexcept;
    IOUring& operator=(IOUring&&) noexcept;

    IOUring(const IOUring&) = delete;
    IOUring& operator=(const IOUring&) = delete;

    ~IOUring() noexcept;
    void close() noexcept;

    [[nodiscard]] io_uring* get() noexcept;

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] io_uring_sqe* get_sqe();
    [[nodiscard]] Cqe wait_cqe();
    [[nodiscard]] Cqe wait_cqe_timeout(const __kernel_timespec& ts);

    void submit();

    [[nodiscard]] std::uint32_t get_sq_entries() const;
    [[nodiscard]] std::uint32_t get_cq_entries() const;
    [[nodiscard]] std::uint32_t get_flags() const;

    [[nodiscard]] BufferRing create_buf_ring(std::uint16_t count, std::uint32_t size, std::uint16_t threshold = 10);

  private:
    void seen_(io_uring_cqe* cqe);

    io_uring ring_{};
    io_uring_params params_{};

    std::uint16_t next_bid_{};

    static inline const io_uring invalid_ring{.ring_fd = -1};
    static inline const io_uring_params empty_params{};
};

} // namespace zportal