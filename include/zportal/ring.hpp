#pragma once

#include <liburing.h>
#include <liburing/io_uring.h>

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

    io_uring* get() noexcept;

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

    io_uring release() noexcept;

    io_uring_sqe* get_sqe();
    io_uring_cqe* get_cqe();

    void seen(io_uring_cqe* cqe);
    void submit();

  private:
    io_uring ring_{};
};

} // namespace zportal