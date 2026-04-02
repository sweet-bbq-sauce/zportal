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
        return Fail(result.error());

    const auto& address = *result;

    Socket sock = Socket::create_socket(address.family());
    if (::connect(sock.get(), address.get(), address.length()) != 0)
        return Fail(ErrorCode::ConnectFailed);

    if (proxies.empty())
        return sock;

    for (auto it = proxies.begin(); it != proxies.end(); it++) {
        const auto& next = std::next(it) == proxies.end() ? target : *std::next(it);
        socks5_connect(sock, next);
    }

    return sock;
}