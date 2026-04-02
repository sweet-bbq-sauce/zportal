#include <utility>

#include <cerrno>

#include <sys/socket.h>

#include <zportal/net/address.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tools/file_descriptor.hpp>

zportal::Socket::Socket(Socket&& other) noexcept
    : fd_(std::move(other.fd_)), family_(std::exchange(other.family_, AF_UNSPEC)) {}

zportal::Socket& zportal::Socket::operator=(Socket&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    fd_ = std::move(other.fd_);
    family_ = std::exchange(other.family_, AF_UNSPEC);

    return *this;
}

zportal::Socket::~Socket() noexcept {
    close();
}

void zportal::Socket::close() noexcept {
    fd_.close();
    family_ = AF_UNSPEC;
}

int zportal::Socket::get() const noexcept {
    return fd_.get();
}

sa_family_t zportal::Socket::get_family() const noexcept {
    return family_;
}

bool zportal::Socket::is_valid() const noexcept {
    return fd_.is_valid();
}

zportal::Socket::operator bool() const noexcept {
    return is_valid();
}

zportal::Result<zportal::Socket> zportal::Socket::create_socket(sa_family_t family, int flags) noexcept {
    if (family != AF_INET && family != AF_INET6 && family != AF_UNIX)
        return Fail(ErrorCode::InvalidSocketFamily);

    const int fd = ::socket(family, SOCK_STREAM | flags, 0);
    if (fd < 0)
        return Fail({ErrorCode::SocketCreateFailed, errno});

    return Socket(fd, family);
}

zportal::Socket::Socket(int fd, sa_family_t family) : fd_(fd), family_(family) {}

zportal::Result<zportal::SockAddress> zportal::Socket::get_local_address() const noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidSocket);

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);

    if (::getsockname(fd_.get(), reinterpret_cast<sockaddr*>(&ss), &len) < 0)
        return Fail({ErrorCode::GetSockNameFailed, errno});

    return SockAddress::from_sockaddr(reinterpret_cast<const sockaddr*>(&ss), len);
}

zportal::Result<zportal::SockAddress> zportal::Socket::get_remote_address() const noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidSocket);

    sockaddr_storage ss{};
    socklen_t len = sizeof(ss);

    if (::getpeername(fd_.get(), reinterpret_cast<sockaddr*>(&ss), &len) < 0)
        return Fail({ErrorCode::GetPeerNameFailed, errno});

    return SockAddress::from_sockaddr(reinterpret_cast<const sockaddr*>(&ss), len);
}

zportal::Result<sa_family_t> zportal::Socket::detect_family() const noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidSocket);

    if (family_ != AF_UNSPEC)
        return family_;

    const auto result = get_local_address();
    if (!result)
        return Fail(result.error());

    family_ = (*result).family();
    return family_;
}