#include <array>
#include <charconv>
#include <stdexcept>
#include <string>
#include <system_error>

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <zportal/address.hpp>

std::string zportal::SockAddress::str(bool extended) const {
    if (!is_valid())
        throw std::logic_error("unspecified address");

    switch (family()) {
    case AF_INET: {
        constexpr std::size_t max_ip4_representation_size = INET_ADDRSTRLEN /*IP4 + NUL*/ + 6 /* :XXXXX */;
        std::array<char, max_ip4_representation_size> buffer{};
        const auto* ip4 = reinterpret_cast<const sockaddr_in*>(&ss_);

        if (::inet_ntop(AF_INET, &ip4->sin_addr, buffer.data(), sizeof(buffer)) == nullptr)
            throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

        // Return only IP4 representation
        if (!extended)
            return buffer.data();

        // Extended info for IP4 contains `:port`
        const auto addr_length = std::strlen(buffer.data());
        buffer[addr_length] = ':';

        char* first = buffer.data() + addr_length + 1;
        char* last = buffer.data() + buffer.size() - 1;
        const auto port = ::ntohs(ip4->sin_port);
        auto [ptr, ec] = std::to_chars(first, last, port);
        if (ec != std::errc{})
            throw std::runtime_error(std::make_error_code(ec).message());

        // Just in case
        *ptr = '\0';

        return buffer.data();
    } break;
    }

    throw std::runtime_error("unsupported address family: " + std::to_string(family()));
}

sa_family_t zportal::SockAddress::family() const noexcept {
    return ss_.ss_family;
}

const sockaddr* zportal::SockAddress::get() const noexcept {
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
    return family() == AF_INET;
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
        if (IN6_IS_ADDR_UNSPECIFIED(&ip6->sin6_addr) || ip6->sin6_port == 0)
            return false;
        return true;
    } else if (family() == AF_UNIX)
        return true;

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