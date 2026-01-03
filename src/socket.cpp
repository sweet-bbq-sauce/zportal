#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <sys/socket.h>
#include <unistd.h>

#include <zportal/socket.hpp>

zportal::Socket::Socket(Socket&& other) noexcept
    : fd_(std::exchange(other.fd_, -1)), family_(std::exchange(other.family_, AF_UNSPEC)) {}

zportal::Socket& zportal::Socket::operator=(Socket&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    fd_ = std::exchange(other.fd_, -1);
    family_ = std::exchange(other.family_, AF_UNSPEC);

    return *this;
}

zportal::Socket::~Socket() noexcept {
    close();
}

void zportal::Socket::close() noexcept {
    if (!is_valid())
        return;

    ::close(fd_);
    fd_ = -1;
    family_ = AF_UNSPEC;
}

int zportal::Socket::get_fd() const noexcept {
    return fd_;
}

sa_family_t zportal::Socket::get_family() const noexcept {
    return family_;
}

bool zportal::Socket::is_valid() const noexcept {
    return fd_ >= 0;
}

zportal::Socket::operator bool() const noexcept {
    return is_valid();
}

zportal::Socket zportal::Socket::create_socket(sa_family_t family) {
    const int fd = ::socket(family, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    return Socket(fd, family);
}

zportal::Socket::Socket(int fd, sa_family_t family) : fd_(fd), family_(family) {}