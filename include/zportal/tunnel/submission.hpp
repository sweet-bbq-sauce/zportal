#pragma once

#include <cstdint>

#include <liburing.h>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tunnel/operation.hpp>

namespace zportal {

void prepare_read(io_uring_sqe* sqe, const Operation& op, int fd, std::uint16_t bid) noexcept;
void prepare_recv(io_uring_sqe* sqe, const Operation& op, int fd, std::uint16_t bid) noexcept;

} // namespace zportal

#include <zportal/tunnel/submission.inl>