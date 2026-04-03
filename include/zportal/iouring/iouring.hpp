#pragma once

#include <liburing.h>

#include <zportal/tools/error.hpp>
#include <zportal/iouring/cqe.hpp>

namespace zportal {

class IoUring {
  public:
    IoUring() noexcept = default;
    Result<IoUring> create_queue(unsigned entries) noexcept;

    ~IoUring() noexcept;
    void close() noexcept;

    Result<io_uring*> get_ring() noexcept;
    Result<const io_uring*> get_ring() const noexcept;

    Result<io_uring_sqe*> get_sqe() noexcept;

    Result<Cqe> wait() noexcept;

    // Result<std::uint16_t> create_buf_ring(std::uint16_t length, std::uint32_t buf_size);

  private:
    io_uring ring_{invalid_ring_};

    static constexpr io_uring invalid_ring_{.ring_fd = -1};
};

} // namespace zportal