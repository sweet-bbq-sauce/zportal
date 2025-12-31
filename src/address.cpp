#include <array>
#include <charconv>
#include <stdexcept>
#include <string>
#include <system_error>

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <zportal/address.hpp>

std::string zportal::SockAddress::str(bool extended) const {
    if (!is_valid())
        throw std::logic_error("unspecified address");

    switch (family()) {
    case AF_INET: {
        std::string buffer;
        std::array<char, INET_ADDRSTRLEN> address{};
        const auto* ip4 = reinterpret_cast<const sockaddr_in*>(&ss_);

        if (::inet_ntop(AF_INET, &ip4->sin_addr, address.data(), address.size()) == nullptr)
            throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

        buffer.append(address.data());

        if (!extended)
            return buffer;

        buffer.append(":");
        std::array<char, 6> port_str{};
        auto [ptr, ec] = std::to_chars(port_str.begin(), port_str.end(), ::ntohs(ip4->sin_port));
        if (ec != std::errc{})
            throw std::runtime_error(std::make_error_code(ec).message());
        buffer.append(port_str.data());

        return buffer;
    } break;

    case AF_INET6: {
        std::string buffer;
        std::array<char, INET6_ADDRSTRLEN> address{};
        const auto* ip6 = reinterpret_cast<const sockaddr_in6*>(&ss_);

        if (extended)
            buffer.append("[");

        if (::inet_ntop(AF_INET6, &ip6->sin6_addr, address.data(), address.size()) == nullptr)
            throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

        buffer.append(address.data());

        if (IN6_IS_ADDR_LINKLOCAL(&ip6->sin6_addr) && ip6->sin6_scope_id != 0) {
            buffer.append("%");
            std::array<char, IF_NAMESIZE> ifname{};
            if (::if_indextoname(ip6->sin6_scope_id, ifname.data()) == nullptr) {
                auto [ptr, ec] = std::to_chars(ifname.begin(), ifname.end(), ip6->sin6_scope_id);
                if (ec != std::errc{})
                    throw std::runtime_error(std::make_error_code(ec).message());
            }
            buffer.append(ifname.data());
        }

        if (!extended)
            return buffer;

        buffer.append("]:");
        std::array<char, 6> port_str{};
        auto [ptr, ec] = std::to_chars(port_str.begin(), port_str.end(), ::ntohs(ip6->sin6_port));
        if (ec != std::errc{})
            throw std::runtime_error(std::make_error_code(ec).message());
        buffer.append(port_str.data());

        return buffer;

    } break;
    }

    throw std::runtime_error("unsupported address family: " + std::to_string(family()));
}

sa_family_t zportal::SockAddress::family() const noexcept {
    return ss_.ss_family;
}

const sockaddr* zportal::SockAddress::get() const {
    if (!is_valid())
        throw std::logic_error("invalid address");

    return reinterpret_cast<const sockaddr*>(&ss_);
}

socklen_t zportal::SockAddress::length() const noexcept {
    return len_;
}

zportal::SockAddress::operator bool() const noexcept {
    return is_valid();
}

bool zportal::SockAddress::is_valid() const noexcept {
    // Currently supported: IP4
    if (family() == AF_INET)
        return length() == sizeof(sockaddr_in);
    else if (family() == AF_INET6)
        return length() == sizeof(sockaddr_in6);

    return false;
}

bool zportal::SockAddress::is_connectable() const noexcept {
    if (!is_valid())
        return false;

    if (family() == AF_INET) {
        const auto* ip4 = reinterpret_cast<const sockaddr_in*>(&ss_);
        if (ip4->sin_addr.s_addr == 0 || ip4->sin_port == 0)
            return false;
        return true;
    } else if (family() == AF_INET6) {
        const auto* ip6 = reinterpret_cast<const sockaddr_in6*>(&ss_);
        if (IN6_IS_ADDR_UNSPECIFIED(&ip6->sin6_addr) || IN6_IS_ADDR_MULTICAST(&ip6->sin6_addr) || ip6->sin6_port == 0 ||
            (IN6_IS_ADDR_LINKLOCAL(&ip6->sin6_addr) && ip6->sin6_scope_id == 0))
            return false;
        return true;
    } /*else if (family() == AF_UNIX)
        return true;
    */

    return false;
}

bool zportal::SockAddress::is_bindable() const noexcept {
    if (!is_valid())
        return false;

    return true;
}

zportal::SockAddress zportal::SockAddress::ip4_numeric(const std::string& numeric, std::uint16_t port) {
    SockAddress buffer;
    auto* ip4 = reinterpret_cast<sockaddr_in*>(&buffer.ss_);

    ip4->sin_family = AF_INET;
    ip4->sin_port = ::htons(port);

    if (const int result = ::inet_pton(AF_INET, numeric.c_str(), &ip4->sin_addr); result == -1)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());
    else if (result == 0)
        throw std::runtime_error("invalid IP4 presentation");

    buffer.len_ = sizeof(sockaddr_in);

    return buffer;
}

zportal::SockAddress zportal::SockAddress::ip6_numeric(const std::string& numeric, std::uint16_t port) {
    SockAddress buffer;

    addrinfo request{}, *result = nullptr;
    request.ai_family = AF_INET6;
    request.ai_flags = AI_NUMERICHOST;

    if (const int gai_result = ::getaddrinfo(numeric.c_str(), nullptr, &request, &result); gai_result != 0)
        throw std::runtime_error("getaddrinfo() error: " + std::string(::gai_strerror(gai_result)));

    if (result == nullptr || result->ai_family != AF_INET6 || result->ai_addr == nullptr ||
        result->ai_addrlen != sizeof(sockaddr_in6)) {
        ::freeaddrinfo(result);
        throw std::runtime_error("unexpected behaviour from getaddrinfo()");
    }

    std::memcpy(&buffer.ss_, result->ai_addr, result->ai_addrlen);

    reinterpret_cast<sockaddr_in6*>(&buffer.ss_)->sin6_port = ::htons(port);
    buffer.len_ = result->ai_addrlen;

    ::freeaddrinfo(result);

    return buffer;
}