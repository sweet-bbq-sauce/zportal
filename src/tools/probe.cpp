#include <memory>
#include <system_error>

#include <cerrno>

#include <liburing.h>
#include <sys/socket.h>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/probe.hpp>

bool zportal::support_check::read_multishot() {
#if HAVE_IORING_OP_READ_MULTISHOT
    std::unique_ptr<io_uring_probe, decltype(&io_uring_free_probe)> probe(io_uring_get_probe(), &io_uring_free_probe);
    if (!probe)
        throw std::system_error(ENOTSUP, std::generic_category(), "io_uring_get_probe");

    return ::io_uring_opcode_supported(probe.get(), IORING_OP_READ_MULTISHOT);
#else
    return false;
#endif
}

bool zportal::support_check::recv_multishot() {
    zportal::IOUring ring(1);

    // [0] is sender
    // [1] is receiver
    int fd[2];
    ::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fd);
    zportal::Socket socket[2] = {zportal::Socket{fd[0]}, zportal::Socket{fd[1]}};

    zportal::BufferRing br = ring.create_buf_ring(2, 1024);

    auto sqe = ring.get_sqe();
    ::io_uring_prep_recv_multishot(sqe, socket[1].get(), nullptr, 0, MSG_DONTWAIT);
    ::io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
#if HAVE_IO_URING_SQE_SET_BUF_GROUP
    ::io_uring_sqe_set_buf_group(sqe, br.get_bgid());
#else
    sqe->buf_group = br.get_bgid();
#endif
    ring.submit();

    ::send(socket[0].get(), ".", 1, 0);

    const zportal::Cqe cqe = ring.wait_cqe();
    const int result = cqe.get_result();

    if (result == -EINVAL || result == -EOPNOTSUPP || result == -ENOTSUP)
        return false;

    if (result == -EAGAIN || result >= 0)
        return true;

    return false;
}