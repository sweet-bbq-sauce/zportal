#include <stdexcept>
#include <system_error>
#include <utility>

#include <cerrno>

#include <liburing.h>

#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/ring.hpp>

zportal::IOUring::IOUring(unsigned entries) {
    if (const int result = ::io_uring_queue_init(entries, &ring_, 0); result < 0)
        throw std::system_error(-result, std::generic_category(), "io_uring_queue_init");
}

zportal::IOUring::IOUring(IOUring&& other) noexcept : ring_(std::exchange(other.ring_, invalid_ring)) {}

zportal::IOUring& zportal::IOUring::operator=(IOUring&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    ring_ = std::exchange(other.ring_, invalid_ring);

    return *this;
}

zportal::IOUring::~IOUring() noexcept {
    close();
}

void zportal::IOUring::close() noexcept {
    if (ring_.ring_fd < 0)
        return;

    ::io_uring_queue_exit(&ring_);
    ring_ = invalid_ring;
}

io_uring* zportal::IOUring::get() noexcept {
    return &ring_;
}

bool zportal::IOUring::is_valid() const noexcept {
    return ring_.ring_fd >= 0;
}

zportal::IOUring::operator bool() const noexcept {
    return is_valid();
}

io_uring zportal::IOUring::release() noexcept {
    io_uring tmp = std::exchange(ring_, invalid_ring);
    return tmp;
}

io_uring_sqe* zportal::IOUring::get_sqe() {
    if (!is_valid())
        throw std::logic_error("ring is closed");

    return ::io_uring_get_sqe(&ring_);
}

zportal::Cqe zportal::IOUring::wait_cqe() {
    if (!is_valid())
        throw std::logic_error("ring is closed");

    io_uring_cqe* cqe = nullptr;
    for (;;) {
        const int result = ::io_uring_wait_cqe(&ring_, &cqe);
        if (result == -EINTR)
            continue;
        else if (result < 0)
            throw std::system_error(-result, std::generic_category(), "io_uring_wait_cqe");

        break;
    }

    if (!cqe)
        throw std::runtime_error("io_uring_wait_cqe returned null CQE");

    Cqe copy(*cqe);
    seen_(cqe);

    return copy;
}

void zportal::IOUring::seen_(io_uring_cqe* cqe) {
    if (!is_valid())
        throw std::logic_error("ring is closed");

    if (!cqe)
        throw std::invalid_argument("CQE is null");

    ::io_uring_cqe_seen(&ring_, cqe);
}

void zportal::IOUring::submit() {
    if (!is_valid())
        throw std::logic_error("ring is closed");

    if (const int result = ::io_uring_submit(&ring_); result < 0)
        throw std::system_error(-result, std::generic_category(), "io_uring_submit");
}

unsigned zportal::IOUring::get_sq_entries() const {
    if (!is_valid())
        throw std::logic_error("ring is closed");

    return ring_.sq.ring_entries;
}