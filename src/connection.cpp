#include <array>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <span>
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

static const auto socket_recv = [](zportal::Socket& socket, std::span<std::uint8_t> data) {
    if (data.empty())
        return;

    if (!socket)
        throw std::logic_error("invalid socket");

    std::size_t processed{};
    while (processed < data.size()) {
        const ssize_t n = ::recv(socket.get_fd(), data.data() + processed, data.size() - processed, 0);
        if (n == 0) {
            socket.close();
            throw std::runtime_error("peer closed connection");
        } else if (n < 0) {
            const auto err = errno;
            if (err == EINTR)
                continue;

            else if (err == EAGAIN || err == EWOULDBLOCK)
                throw std::runtime_error("recv timeout");

            socket.close();
            throw std::system_error(err, std::system_category(), "recv");
        }

        processed += static_cast<std::size_t>(n);
    }
};

static const auto socket_send = [](zportal::Socket& socket, std::span<const std::uint8_t> data) {
    if (data.empty())
        return;

    if (!socket)
        throw std::logic_error("invalid socket");

    std::size_t processed{};
    while (processed < data.size()) {
        const ssize_t n = ::send(socket.get_fd(), data.data() + processed, data.size() - processed, MSG_NOSIGNAL);
        if (n == 0) {
            socket.close();
            throw std::runtime_error("send() returned 0");
        } else if (n < 0) {
            const auto err = errno;
            if (err == EINTR)
                continue;

            else if (err == EAGAIN || err == EWOULDBLOCK)
                throw std::runtime_error("send timeout");

            socket.close();
            throw std::system_error(err, std::system_category(), "send");
        }

        processed += static_cast<std::size_t>(n);
    }
};

zportal::Socket zportal::connect_to(const Address& target, const std::vector<Address>& proxies) {

    const Address& first = proxies.empty() ? target : proxies.front();
    const SockAddress address = resolve(first);

    if (!address.is_connectable())
        throw std::invalid_argument("address is not connectable");

    Socket sock = Socket::create_socket(address.family());
    if (::connect(sock.get_fd(), address.get(), address.length()) != 0)
        throw std::system_error(errno, std::system_category(), "connect");

    if (proxies.empty())
        return sock;

    const auto socks5_handshake = [&sock](const Address& next) {
        if (std::holds_alternative<HostPair>(next) && std::get<HostPair>(next).hostname.size() > 255)
            throw std::invalid_argument("hostname size is too long");

        // Auth method negotiation
        static const std::array<std::uint8_t, 3> method_request = {0x05, 0x01, 0x00}; // "no auth" only
        std::array<std::uint8_t, 2> method_response;
        socket_send(sock, method_request);
        socket_recv(sock, method_response);

        if (method_response[1] == 0xFF)
            throw std::runtime_error("SOCKS5: no acceptable auth method");
        if (method_response[1] != 0x00)
            throw std::runtime_error("SOCKS5: proxy requires unsupported auth method");

        // CONNECT command
        std::size_t connect_request_length{};
        std::array<std::uint8_t, 4 /* header */ + 1 + 255 /* domain length */ + 2 /* port */> connect_request = {
            0x05, 0x01, 0x00};
        if (std::holds_alternative<HostPair>(next)) {
            connect_request[3] = 0x03; // DOMAIN
            const auto& domain = std::get<HostPair>(next);

            connect_request[4] = static_cast<std::uint8_t>(domain.hostname.size());
            std::memcpy(connect_request.data() + 4 + 1, domain.hostname.data(), domain.hostname.size());

            const std::uint16_t nport = ::htons(domain.port);
            std::memcpy(connect_request.data() + 4 + 1 + domain.hostname.size(), &nport, sizeof(nport));

            connect_request_length = 4 + 1 + domain.hostname.size() + sizeof(nport);
        } else {
            const auto& sa = std::get<SockAddress>(next);
            if (sa.family() == AF_INET) {
                connect_request[3] = 0x01; // IP4
                const auto* sa_in = reinterpret_cast<const sockaddr_in*>(sa.get());

                std::memcpy(connect_request.data() + 4, &sa_in->sin_addr.s_addr, 4);
                std::memcpy(connect_request.data() + 4 + 4, &sa_in->sin_port, 2);

                connect_request_length = 4 + 4 + 2;
            } else if (sa.family() == AF_INET6) {
                connect_request[3] = 0x04; // IP6
                const auto* sa_in6 = reinterpret_cast<const sockaddr_in6*>(sa.get());

                std::memcpy(connect_request.data() + 4, &sa_in6->sin6_addr, 16);
                std::memcpy(connect_request.data() + 4 + 16, &sa_in6->sin6_port, 2);

                connect_request_length = 4 + 16 + 2;
            } else
                throw std::invalid_argument("unsupported address family");
        }

        socket_send(sock, {connect_request.data(), connect_request_length});

        std::array<std::uint8_t, 4> command_response;
        std::array<std::uint8_t, 1 + 255 + 2> hole_for_response_address;
        socket_recv(sock, command_response);

        if (command_response[1] != 0x00)
            throw std::runtime_error("SOCKS5: can't connect to target");

        std::size_t response_address_length{};
        if (command_response[3] == 0x01)
            response_address_length = 4 + 2;
        else if (command_response[3] == 0x04)
            response_address_length = 16 + 2;
        else if (command_response[3] == 0x03) {
            std::uint8_t len;
            socket_recv(sock, {&len, sizeof(len)});
            response_address_length = len + 2;
        } else
            throw std::runtime_error("SOCKS5: unknown address family");

        socket_recv(sock, {hole_for_response_address.data(), response_address_length});
    };

    for (auto it = proxies.begin(); it != proxies.end(); it++) {
        const auto& next = std::next(it) == proxies.end() ? target : *std::next(it);
        socks5_handshake(next);
    }

    return sock;
}

zportal::Socket zportal::create_listener(const Address& address) {
    const auto resolved = resolve(address);
    if (!resolved.is_bindable())
        throw std::logic_error("address is not bindable");

    Socket sock = Socket::create_socket(resolved.family());

    const int yes = 1;
    if (::setsockopt(sock.get_fd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0)
        throw std::system_error(errno, std::system_category(), "setsockopt");

    if (::bind(sock.get_fd(), resolved.get(), resolved.length()) != 0)
        throw std::system_error(errno, std::system_category(), "bind");

    if (::listen(sock.get_fd(), 1) != 0)
        throw std::system_error(errno, std::system_category(), "listen");

    return sock;
}

zportal::Socket zportal::accept_from(const Socket& listener) {
    if (!listener)
        throw std::logic_error("listener is not valid");

    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    const int clientfd = ::accept(listener.get_fd(), reinterpret_cast<sockaddr*>(&addr), &len);
    if (clientfd < 0)
        throw std::system_error(errno, std::system_category(), "accept");

    return Socket(clientfd, listener.get_family());
}