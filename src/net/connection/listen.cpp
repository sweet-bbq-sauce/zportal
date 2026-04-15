#include <sys/socket.h>

#include <zportal/net/connection.hpp>
#include <zportal/net/resolve.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/error.hpp>

zportal::Result<zportal::Socket> zportal::create_listener(const Address& address) noexcept {
    const auto result = resolve(address);
    if (!result)
        return fail(result.error());

    const auto& resolved = *result;
    auto sock = Socket::create_socket(resolved.family(), SOCK_CLOEXEC);
    if (!sock)
        return fail(sock.error());

    const int yes = 1;
    if (::setsockopt((*sock).get(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0)
        return fail({ErrorCode::SetSockOptFailed, errno});

    if (::bind((*sock).get(), resolved.get(), resolved.length()) != 0)
        return fail({ErrorCode::BindFailed, errno});

    if (::listen((*sock).get(), 1) != 0)
        return fail({ErrorCode::ListenFailed, errno});

    return sock;
}

std::expected<zportal::Socket, zportal::Error> zportal::accept_from(const Socket& listener) noexcept {
    if (!listener)
        return fail(ErrorCode::InvalidSocket);

    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);

    const int clientfd = ::accept4(listener.get(), reinterpret_cast<sockaddr*>(&addr), &len, SOCK_CLOEXEC);
    if (clientfd < 0)
        return fail({ErrorCode::AcceptFailed, errno});

    return Socket(clientfd);
}