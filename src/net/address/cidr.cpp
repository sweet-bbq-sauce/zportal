#include <stdexcept>
#include <string>

#include <cstring>

#include <netinet/in.h>
#include <sys/socket.h>

#include <zportal/net/address.hpp>

zportal::Cidr::Cidr(zportal::SockAddress address, std::uint8_t prefix) : address_(address), prefix_(prefix) {
    if (address.family() == AF_INET) {
        if (prefix > 32)
            throw std::invalid_argument("prefix for IPv4 must be 0-32");
    } else if (address.family() == AF_INET6) {
        if (prefix > 128)
            throw std::invalid_argument("prefix for IPv6 must be 0-128");
    } else
        throw std::invalid_argument("address must be IPv4 or IPv6");
}

const zportal::SockAddress& zportal::Cidr::get_address() const noexcept {
    return address_;
}

const std::uint8_t zportal::Cidr::get_prefix() const noexcept {
    return prefix_;
}

bool zportal::Cidr::is_ip4() const noexcept {
    return address_.family() == AF_INET;
}

bool zportal::Cidr::is_ip6() const noexcept {
    return address_.family() == AF_INET6;
}

bool zportal::Cidr::is_valid() const noexcept {
    return address_.is_ip();
}

zportal::Cidr::operator bool() const noexcept {
    return is_valid();
}

std::string zportal::Cidr::str() const {
    return address_.str(false) + '/' + std::to_string(prefix_);
}

zportal::Cidr zportal::Cidr::get_network() const noexcept {
    if (!is_valid() || !address_.is_ip())
        return {};

    const sockaddr* sa = address_.get();
    if (!sa)
        return {};

    if (sa->sa_family == AF_INET) {
        if (prefix_ > 32)
            return {};

        sockaddr_in sin{};
        std::memcpy(&sin, sa, sizeof(sockaddr_in));

        std::uint32_t host = ntohl(sin.sin_addr.s_addr);
        std::uint32_t mask = (prefix_ == 0) ? 0u : (0xFFFFFFFFu << (32u - prefix_));
        host &= mask;
        sin.sin_addr.s_addr = htonl(host);

        SockAddress net = SockAddress::from_sockaddr(reinterpret_cast<const sockaddr*>(&sin),
                                                     static_cast<socklen_t>(sizeof(sockaddr_in)));

        return Cidr{std::move(net), prefix_};
    }

    if (sa->sa_family == AF_INET6) {
        if (prefix_ > 128)
            return {};

        sockaddr_in6 sin6{};
        std::memcpy(&sin6, sa, sizeof(sockaddr_in6));

        std::uint8_t* b = reinterpret_cast<std::uint8_t*>(&sin6.sin6_addr);

        std::uint8_t bits = prefix_;
        std::size_t full_bytes = bits / 8;
        std::uint8_t rem_bits = static_cast<std::uint8_t>(bits % 8);

        for (std::size_t i = full_bytes + (rem_bits ? 1 : 0); i < 16; ++i)
            b[i] = 0;

        if (rem_bits != 0 && full_bytes < 16) {
            std::uint8_t m = static_cast<std::uint8_t>(0xFFu << (8u - rem_bits));
            b[full_bytes] = static_cast<std::uint8_t>(b[full_bytes] & m);
        }

        SockAddress net = SockAddress::from_sockaddr(reinterpret_cast<const sockaddr*>(&sin6),
                                                     static_cast<socklen_t>(sizeof(sockaddr_in6)));

        return Cidr{std::move(net), prefix_};
    }

    return {};
}

bool zportal::Cidr::is_in_network(const SockAddress& address) const noexcept {
    if (!is_valid() || !address_.is_ip() || !address.is_ip())
        return false;

    const sockaddr* net_sa = address_.get();
    const sockaddr* addr_sa = address.get();
    if (!net_sa || !addr_sa)
        return false;

    if (net_sa->sa_family != addr_sa->sa_family)
        return false;

    if (net_sa->sa_family == AF_INET) {
        if (prefix_ > 32)
            return false;

        sockaddr_in net{};
        sockaddr_in a{};
        std::memcpy(&net, net_sa, sizeof(sockaddr_in));
        std::memcpy(&a, addr_sa, sizeof(sockaddr_in));

        std::uint32_t net_host = ntohl(net.sin_addr.s_addr);
        std::uint32_t a_host = ntohl(a.sin_addr.s_addr);

        std::uint32_t mask = (prefix_ == 0) ? 0u : (0xFFFFFFFFu << (32u - prefix_));

        return (net_host & mask) == (a_host & mask);
    }

    if (net_sa->sa_family == AF_INET6) {
        if (prefix_ > 128)
            return false;

        sockaddr_in6 net6{};
        sockaddr_in6 a6{};
        std::memcpy(&net6, net_sa, sizeof(sockaddr_in6));
        std::memcpy(&a6, addr_sa, sizeof(sockaddr_in6));

        const std::uint8_t* nb = reinterpret_cast<const std::uint8_t*>(&net6.sin6_addr);
        const std::uint8_t* ab = reinterpret_cast<const std::uint8_t*>(&a6.sin6_addr);

        std::uint8_t bits = prefix_;
        std::size_t full_bytes = bits / 8;
        std::uint8_t rem_bits = static_cast<std::uint8_t>(bits % 8);

        for (std::size_t i = 0; i < full_bytes; ++i) {
            if (nb[i] != ab[i])
                return false;
        }

        if (rem_bits != 0 && full_bytes < 16) {
            std::uint8_t m = static_cast<std::uint8_t>(0xFFu << (8u - rem_bits));
            if (static_cast<std::uint8_t>(nb[full_bytes] & m) != static_cast<std::uint8_t>(ab[full_bytes] & m)) {
                return false;
            }
        }

        return true;
    }

    return false;
}