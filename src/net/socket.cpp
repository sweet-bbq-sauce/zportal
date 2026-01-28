#include <stdexcept>
#include <system_error>
#include <utility>

#include <cerrno>

#include <sys/socket.h>
#include <unistd.h>

#include <zportal/net/address.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/file_descriptor.hpp>

zportal::Socket::Socket(Socket&& other) noexcept : fd_(std::move(other.fd_)) {}

zportal::Socket& zportal::Socket::operator=(Socket&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    fd_ = std::move(other.fd_);

    return *this;
}

zportal::Socket::~Socket() noexcept {
    close();
}

void zportal::Socket::close() noexcept {
    fd_.close();
}

int zportal::Socket::get() const noexcept {
    return fd_.get();
}

sa_family_t zportal::Socket::get_family() const noexcept {
    if (!is_valid())
        return AF_UNSPEC;

    return get_local_address().family();
}

bool zportal::Socket::is_valid() const noexcept {
    return fd_.is_valid();
}

zportal::Socket::operator bool() const noexcept {
    return is_valid();
}

zportal::Socket zportal::Socket::create_socket(sa_family_t family, int flags) {
    const int fd = ::socket(family, SOCK_STREAM | flags, 0);
    if (fd < 0)
        throw std::system_error(errno, std::system_category(), "socket");

    return Socket(fd);
}

zportal::Socket::Socket(int fd) : fd_(fd) {
    const sa_family_t family = get_family();
    if (family != AF_INET && family != AF_INET6 && family != AF_UNIX)
        throw std::invalid_argument("socket family must be AF_INET, AF_INET6 or AF_UNIX");
}

zportal::SockAddress zportal::Socket::get_local_address() const {
    if (!is_valid())
        throw std::logic_error("socket is invalid");

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);

    if (::getsockname(fd_.get(), reinterpret_cast<sockaddr*>(&ss), &len) < 0)
        throw std::system_error(errno, std::system_category(), "getsockname");

    return SockAddress::from_sockaddr(reinterpret_cast<const sockaddr*>(&ss), len);
}

zportal::SockAddress zportal::Socket::get_remote_address() const {
    if (!is_valid())
        throw std::logic_error("socket is invalid");

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);

    if (::getpeername(fd_.get(), reinterpret_cast<sockaddr*>(&ss), &len) < 0)
        throw std::system_error(errno, std::system_category(), "getpeername");

    return SockAddress::from_sockaddr(reinterpret_cast<const sockaddr*>(&ss), len);
}