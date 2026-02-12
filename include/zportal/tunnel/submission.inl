#pragma once

#include <liburing.h>

#include <zportal/tools/probe.hpp>
#include <zportal/tunnel/peer.hpp>
#include <zportal/tunnel/submission.hpp>

namespace zportal {

inline void prepare_read(io_uring_sqe* sqe, const Operation& op, int fd, std::uint16_t bid) noexcept {
#if HAVE_IO_URING_PREP_READ_MULTISHOT
    if (support_check::read_multishot())
        ::io_uring_prep_read_multishot(sqe, fd, 0, -1, bid);
    else {
        ::io_uring_prep_read(sqe, fd, nullptr, 0, -1);
        sqe->flags |= IOSQE_BUFFER_SELECT;
        sqe->buf_group = bid;
    }
#else
    ::io_uring_prep_read(sqe, fd, nullptr, 0, -1);
    sqe->flags |= IOSQE_BUFFER_SELECT;
    sqe->buf_group = bid;
#endif

    ::io_uring_sqe_set_data64(sqe, op.serialize());
}

inline void prepare_recv(io_uring_sqe* sqe, const Operation& op, int fd, std::uint16_t bid) noexcept {
    if (support_check::recv_multishot())
        ::io_uring_prep_recv_multishot(sqe, fd, nullptr, 0, 0);
    else
        ::io_uring_prep_recv(sqe, fd, nullptr, 0, 0);

    sqe->flags |= IOSQE_BUFFER_SELECT;
    sqe->buf_group = bid;

    ::io_uring_sqe_set_data64(sqe, op.serialize());
}

} // namespace zportal