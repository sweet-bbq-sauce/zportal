#include <vector>

#include <sys/socket.h>

#include <zportal/net/connection.hpp>
#include <zportal/net/resolve.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/error.hpp>

zportal::Result<zportal::Socket> zportal::connect_to(const Address& target,
                                                     const std::vector<Address>& proxies) noexcept {

    const Address& first = proxies.empty() ? target : proxies.front();
    const auto result = resolve(first);
    if (!result)
        return fail(result.error());

    const auto& address = *result;
    auto sock = Socket::create_socket(address.family(), SOCK_CLOEXEC);
    if (!sock)
        return fail(sock.error());

    if (::connect((*sock).get(), address.get(), address.length()) != 0)
        return fail({ErrorCode::ConnectFailed, errno});

    if (proxies.empty())
        return sock;

    for (auto it = proxies.begin(); it != proxies.end(); it++) {
        const auto& next = std::next(it) == proxies.end() ? target : *std::next(it);
        if (const auto socks_result = socks5_connect(*sock, next); !socks_result)
            return fail(socks_result.error());
    }

    return sock;
}