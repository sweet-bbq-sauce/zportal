#pragma once

#include <memory>
#include <vector>

#include <liburing.h>

#include <zportal/iouring/cqe.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

class BufferGroup;

class IoUring {
  public:
    IoUring() noexcept = default;
    static Result<IoUring> create_queue(unsigned entries) noexcept;

    IoUring(IoUring&&) noexcept;
    IoUring& operator=(IoUring&&) noexcept;
    IoUring(const IoUring&) = delete;
    IoUring& operator=(const IoUring&) = delete; 

    ~IoUring() noexcept;
    void close() noexcept;

    Result<io_uring_sqe*> get_sqe() noexcept;
    Result<unsigned> submit() noexcept;

    Result<Cqe> wait() noexcept;

    Result<BufferGroup*> create_buffer_group(std::uint16_t length, std::uint32_t buf_size) noexcept;
    Result<BufferGroup*> get_buffer_group(std::uint16_t bgid) noexcept;

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

  private:
    io_uring ring_{invalid_ring_};

    std::vector<std::unique_ptr<BufferGroup>> buffer_groups_;
    std::uint16_t next_bgid_{0};
    std::uint16_t get_next_bgid_() noexcept;

    static constexpr io_uring invalid_ring_{.ring_fd = -1};
};

} // namespace zportal