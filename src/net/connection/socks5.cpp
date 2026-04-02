#include <variant>

#include <cstring>

#include <netinet/in.h>

#include <zportal/net/connection.hpp>
#include <zportal/tools/error.hpp>

static const auto socket_recv = [](zportal::Socket& socket, std::span<std::uint8_t> data) -> zportal::Error {
    if (data.empty())
        return {};

    if (!socket)
        return zportal::Error{zportal::ErrorCode::InvalidSocket};

    std::size_t processed{};
    while (processed < data.size()) {
        const ssize_t n = ::recv(socket.get(), data.data() + processed, data.size() - processed, 0);
        if (n == 0) {
            socket.close();
            return zportal::Error{zportal::ErrorCode::SocksPeerClosed};
        } else if (n < 0) {
            const auto err = errno;
            if (err == EINTR)
                continue;

            else if (err == EAGAIN || err == EWOULDBLOCK)
                return zportal::Error{zportal::ErrorCode::SocksRecvTimeout};

            socket.close();
            return zportal::Error{zportal::ErrorCode::SocksErrno, "", err};
        }

        processed += static_cast<std::size_t>(n);
    }

    return {};
};

static const auto socket_send = [](zportal::Socket& socket, std::span<const std::uint8_t> data) -> zportal::Error {
    if (data.empty())
        return {};

    if (!socket)
        return zportal::Error{zportal::ErrorCode::InvalidSocket};

    std::size_t processed{};
    while (processed < data.size()) {
        const ssize_t n = ::send(socket.get(), data.data() + processed, data.size() - processed, MSG_NOSIGNAL);
        if (n == 0) {
            socket.close();
            return zportal::Error{zportal::ErrorCode::SendFailed};
        } else if (n < 0) {
            const auto err = errno;
            if (err == EINTR)
                continue;

            else if (err == EAGAIN || err == EWOULDBLOCK)
                return zportal::Error{zportal::ErrorCode::SocksSendTimeout};

            socket.close();
            return zportal::Error{zportal::ErrorCode::SocksErrno, "", err};
        }

        processed += static_cast<std::size_t>(n);
    }

    return {};
};

zportal::Error zportal::socks5_connect(Socket& socket, const Address& address) noexcept {
    if (std::holds_alternative<HostPair>(address) && std::get<HostPair>(address).hostname.size() > 255)
        return Error{ErrorCode::SocksHostnameTooLong};

    // Auth method negotiation
    static const std::array<std::uint8_t, 3> method_request = {0x05, 0x01, 0x00}; // "no auth" only
    std::array<std::uint8_t, 2> method_response;
    socket_send(socket, method_request);
    socket_recv(socket, method_response);

    if (method_response[1] == 0xFF)
        return Error{ErrorCode::SocksAuthMethodUnsupported};
    if (method_response[1] != 0x00)
        return Error{ErrorCode::SocksAuthMethodUnsupported};

    // CONNECT command
    std::size_t connect_request_length{};
    std::array<std::uint8_t, 4 /* header */ + 1 + 255 /* domain length */ + 2 /* port */> connect_request = {0x05, 0x01,
                                                                                                             0x00};
    if (std::holds_alternative<HostPair>(address)) {
        connect_request[3] = 0x03; // DOMAIN
        const auto& domain = std::get<HostPair>(address);

        connect_request[4] = static_cast<std::uint8_t>(domain.hostname.size());
        std::memcpy(connect_request.data() + 4 + 1, domain.hostname.data(), domain.hostname.size());

        const std::uint16_t nport = ::htons(domain.port);
        std::memcpy(connect_request.data() + 4 + 1 + domain.hostname.size(), &nport, sizeof(nport));

        connect_request_length = 4 + 1 + domain.hostname.size() + sizeof(nport);
    } else {
        const auto& sa = std::get<SockAddress>(address);
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
            return Error{ErrorCode::SocksUnsupportedFamily};
    }

    socket_send(socket, {connect_request.data(), connect_request_length});

    std::array<std::uint8_t, 4> command_response;
    std::array<std::uint8_t, 1 + 255 + 2> hole_for_response_address;
    socket_recv(socket, command_response);

    if (command_response[1] != 0x00)
        return Error{ErrorCode::SocksConnectFailed};

    std::size_t response_address_length{};
    if (command_response[3] == 0x01)
        response_address_length = 4 + 2;
    else if (command_response[3] == 0x04)
        response_address_length = 16 + 2;
    else if (command_response[3] == 0x03) {
        std::uint8_t len;
        socket_recv(socket, {&len, sizeof(len)});
        response_address_length = len + 2;
    } else
        return Error{ErrorCode::SocksUnsupportedFamily};

    socket_recv(socket, {hole_for_response_address.data(), response_address_length});

    return {};
};