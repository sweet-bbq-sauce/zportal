#pragma once

#include <liburing.h>

namespace zportal {

class IOUring {
  public:
    explicit IOUring(unsigned entries);

    IOUring(IOUring&&) noexcept;
    IOUring& operator=(IOUring&&) noexcept;

    IOUring(const IOUring&) = delete;
    IOUring& operator=(const IOUring&) = delete;

    ~IOUring() noexcept;
    void close() noexcept;

    [[nodiscard]] io_uring* get() noexcept;

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] io_uring release() noexcept;

    [[nodiscard]] io_uring_sqe* get_sqe();
    [[nodiscard]] io_uring_cqe* get_cqe();

    void seen(io_uring_cqe* cqe);
    void submit();

    [[nodiscard]] unsigned get_sq_entries() const;

  private:
    io_uring ring_{};
};

} // namespace zportal