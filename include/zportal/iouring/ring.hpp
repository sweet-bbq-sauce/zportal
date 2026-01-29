#pragma once

#include <cstdint>

#include <liburing.h>

namespace zportal {

class Cqe;

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

    void submit();

    [[nodiscard]] std::uint32_t get_sq_entries() const;
    [[nodiscard]] std::uint32_t get_cq_entries() const;
    [[nodiscard]] std::uint32_t get_flags() const;

  private:
    void seen_(io_uring_cqe* cqe);

    io_uring ring_{};
    io_uring_params params_{};
    static inline const io_uring invalid_ring{.ring_fd = -1};
    static inline const io_uring_params empty_params{};
};

} // namespace zportal