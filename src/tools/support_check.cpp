#include <memory>
#include <optional>

#include <cerrno>

#include <liburing.h>
#include <sys/socket.h>

#include <zportal/iouring/buffer_group.hpp>
#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/iouring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tools/support_check.hpp>

zportal::Result<bool> zportal::support_check::read_multishot() noexcept {
    static std::optional<bool> cache{};
    if (cache)
        return *cache;

#if HAVE_IORING_OP_READ_MULTISHOT
    std::unique_ptr<io_uring_probe, decltype(&io_uring_free_probe)> probe(io_uring_get_probe(), &io_uring_free_probe);
    if (!probe)
        return fail(ErrorCode::RingProbeNotSupported);

    cache = static_cast<bool>(::io_uring_opcode_supported(probe.get(), IORING_OP_READ_MULTISHOT));
#else
    cache = false;
#endif

    return *cache;
}

zportal::Result<bool> zportal::support_check::recv_multishot() noexcept {
    static std::optional<bool> cache{};
    if (cache)
        return *cache;

#if HAVE_IO_URING_PREP_RECV_MULTISHOT
    auto ring = IoUring::create_queue(1);
    if (!ring)
        return fail(ring.error());

    // [0] is sender
    // [1] is receiver
    int fd[2];
    if (const int result = ::socketpair(AF_UNIX, SOCK_STREAM, 0, fd); result < 0)
        return fail({ErrorCode::SocketPairFailed, errno});

    zportal::Socket socket[2] = {zportal::Socket{fd[0]}, zportal::Socket{fd[1]}};

    auto bg = ring->create_buffer_group(2, 1024);
    if (!bg)
        return fail(bg.error());

    auto sqe = ring->get_sqe();
    if (!sqe)
        return fail(bg.error());

    ::io_uring_prep_recv_multishot(*sqe, socket[1].get(), nullptr, 0, MSG_DONTWAIT);
    ::io_uring_sqe_set_flags(*sqe, IOSQE_BUFFER_SELECT);
    (*sqe)->buf_group = (*bg)->get_bgid();
    if (auto result = ring->submit(); !result)
        return fail(result.error());

    ::send(socket[0].get(), ".", 1, 0);

    const auto cqe = ring->wait();
    if (!cqe)
        return fail(cqe.error());

    const int result = cqe->result();

    if (result == -EAGAIN || result >= 0)
        cache = true;
    else
        cache = false;
#else
    cache = false;
#endif

    return *cache;
}

#if defined(__x86_64__) || defined(__i386__)
    #include <cpuid.h>
    #include <nmmintrin.h>
#endif

bool zportal::support_check::sse4() noexcept {
    static std::optional<bool> cache{};
    if (cache)
        return *cache;

#if defined(__x86_64__) || defined(__i386__)
    unsigned eax, ebx, ecx, edx;
    if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
        cache = false;

    cache = (ecx & bit_SSE4_2) != 0;
#else
    cache = false;
#endif

    return *cache;
}