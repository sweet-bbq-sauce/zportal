#include <asm-generic/socket.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>

#include <cerrno>

#include <netdb.h>
#include <sys/socket.h>

#include <zportal/address.hpp>
#include <zportal/connection.hpp>
#include <zportal/socket.hpp>

static const auto resolve = [](const zportal::Address& address) -> zportal::SockAddress {
    if (std::holds_alternative<zportal::SockAddress>(address))
        return std::get<zportal::SockAddress>(address);

    const auto& hostpair = std::get<zportal::HostPair>(address);

    addrinfo request{}, *result = nullptr;
    request.ai_family = AF_UNSPEC;
    request.ai_socktype = SOCK_STREAM;

    if (const int code =
            ::getaddrinfo(hostpair.hostname.c_str(), std::to_string(hostpair.port).c_str(), &request, &result);
        code != 0)
        throw std::runtime_error("getaddrinfo() for " + hostpair.hostname + ": " + ::gai_strerror(code));

    zportal::SockAddress resolved;
    try {
        resolved = zportal::SockAddress::from_sockaddr(result->ai_addr, result->ai_addrlen);
    } catch (...) {
        ::freeaddrinfo(result);
        throw;
    }

    ::freeaddrinfo(result);

    return resolved;
};

zportal::Socket zportal::connect_to(const Address& address) {
    const auto resolved = resolve(address);
    if (!resolved.is_connectable())
        throw std::logic_error("address is not connectable");

    Socket sock = Socket::create_socket(resolved.family());

    if (::connect(sock.get_fd(), resolved.get(), resolved.length()) != 0)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    return sock;
}

zportal::Socket zportal::create_listener(const Address& address) {
    const auto resolved = resolve(address);
    if (!resolved.is_bindable())
        throw std::logic_error("address is not bindable");

    Socket sock = Socket::create_socket(resolved.family());

    const int yes = 1;
    if (::setsockopt(sock.get_fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    if (::bind(sock.get_fd(), resolved.get(), resolved.length()) != 0)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    if (::listen(sock.get_fd(), 1) != 0)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    return sock;
}

zportal::Socket zportal::accept_from(const Socket& listener) {
    if (!listener)
        throw std::logic_error("listener is not valid");

    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    const int clientfd = ::accept(listener.get_fd(), reinterpret_cast<sockaddr*>(&addr), &len);
    if (clientfd < 0)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    std::cout << "New connection from: "
              << SockAddress::from_sockaddr(reinterpret_cast<sockaddr*>(&addr), len).str(true) << std::endl;

    return Socket(clientfd, listener.get_family());
}