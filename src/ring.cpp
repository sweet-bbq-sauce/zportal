#include <liburing/io_uring.h>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <liburing.h>

#include <zportal/ring.hpp>

zportal::IOUring::IOUring(unsigned entries) {
    if (const int result = ::io_uring_queue_init(entries, &ring_, 0); result < 0)
        throw std::system_error(-result, std::system_category(), "io_uring_queue_init");
}

zportal::IOUring::IOUring(IOUring&& other) noexcept : ring_(std::exchange(other.ring_, io_uring{})) {
    other.ring_.ring_fd = -1;
}

zportal::IOUring& zportal::IOUring::operator=(IOUring&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    ring_ = std::exchange(other.ring_, io_uring{});
    other.ring_.ring_fd = -1;

    return *this;
}

zportal::IOUring::~IOUring() noexcept {
    close();
}

void zportal::IOUring::close() noexcept {
    if (ring_.ring_fd < 0)
        return;

    ::io_uring_queue_exit(&ring_);
    ring_.ring_fd = -1;
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
    io_uring tmp = std::exchange(ring_, io_uring{});
    ring_.ring_fd = -1;

    return tmp;
}

io_uring_sqe* zportal::IOUring::get_sqe() {
    if (!is_valid())
        throw std::logic_error("ring is closed");

    io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
    if (!sqe)
        throw std::runtime_error("SQE is null");

    return sqe;
}

io_uring_cqe* zportal::IOUring::get_cqe() {
    if (!is_valid())
        throw std::logic_error("ring is closed");

    io_uring_cqe* cqe = nullptr;
    if (const int result = ::io_uring_wait_cqe(&ring_, &cqe); result < 0)
        throw std::system_error(-result, std::system_category(), "io_uring_wait_cqe");

    if (!cqe)
        throw std::runtime_error("CQE is null");

    return cqe;
}

void zportal::IOUring::seen(io_uring_cqe* cqe) {
    if (!is_valid())
        throw std::logic_error("ring is closed");

    if (!cqe)
        throw std::invalid_argument("CQE is null");

    ::io_uring_cqe_seen(&ring_, cqe);
}

void zportal::IOUring::submit() {
    if (const int result = ::io_uring_submit(&ring_); result < 0)
        throw std::system_error(-result, std::system_category(), "io_uring_submit");
}