#include <array>
#include <charconv>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>

#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <zportal/net/address.hpp>

constexpr auto is_binary = [](std::span<const char> data) noexcept {
    for (const auto c : data) {
        if (!std::isprint(static_cast<unsigned char>(c)))
            return true;
    }

    return false;
};

constexpr auto to_hex = [](std::span<const std::byte> data) {
    constexpr char up[] = "0123456789ABCDEF";

    std::string out;
    out.resize(data.size() * 2);

    std::size_t j = 0;
    for (std::byte b : data) {
        auto v = std::to_integer<unsigned int>(b);
        out[j++] = up[(v >> 4) & 0xF];
        out[j++] = up[v & 0xF];
    }
    return out;
};

constexpr auto is_supported_family = [](sa_family_t family) noexcept {
    return family == AF_INET || family == AF_INET6 || family == AF_UNIX;
};

std::string zportal::SockAddress::str(bool extended) const {
    if (!is_valid())
        throw std::logic_error("unspecified address");

    switch (family()) {
    case AF_INET: {
        std::string buffer;
        std::array<char, INET_ADDRSTRLEN> address{};
        const auto* ip4 = reinterpret_cast<const sockaddr_in*>(&ss_);

        if (::inet_ntop(AF_INET, &ip4->sin_addr, address.data(), address.size()) == nullptr)
            throw std::system_error(errno, std::system_category(), "inet_ntop");

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
            throw std::system_error(errno, std::system_category(), "inet_ntop");

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

    case AF_UNIX: {
        std::string buffer;
        const auto* unix = reinterpret_cast<const sockaddr_un*>(&ss_);

        const bool abstract = unix->sun_path[0] == '\0';

        const char* ptr = abstract ? unix->sun_path + 1 : unix->sun_path;
        const std::size_t size = length() - 1 - offsetof(sockaddr_un, sun_path);

        const bool binary = is_binary({ptr, size});
        if (!abstract && binary)
            throw std::logic_error("invalid address");

        if (extended)
            buffer.append(abstract ? "unixa:" : "unix:");

        buffer.append(binary ? to_hex({reinterpret_cast<const std::byte*>(ptr), size}) : std::string(ptr, size));

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
    if (family() == AF_INET)
        return length() == sizeof(sockaddr_in);
    else if (family() == AF_INET6)
        return length() == sizeof(sockaddr_in6);
    else if (family() == AF_UNIX) {
        if (length() < offsetof(sockaddr_un, sun_path) + 1)
            return false;
        if (length() > sizeof(sockaddr_un))
            return false;

        auto* unix = reinterpret_cast<const sockaddr_un*>(&ss_);
        bool abstract = unix->sun_path[0] == '\0';

        if (!abstract) {
            if (length() < offsetof(sockaddr_un, sun_path) + 2)
                return false;
        }
        return true;
    }

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
    } else if (family() == AF_UNIX)
        return true;

    return false;
}

bool zportal::SockAddress::is_bindable() const noexcept {
    if (!is_valid())
        return false;

    return true;
}

bool zportal::SockAddress::is_ip() const noexcept {
    if (!is_valid())
        return false;

    return family() == AF_INET || family() == AF_INET6;
}

bool zportal::SockAddress::is_unix() const noexcept {
    if (!is_valid())
        return false;

    return family() == AF_UNIX;
}

zportal::SockAddress zportal::SockAddress::from_sockaddr(const sockaddr* addr, socklen_t len) {
    if (addr == nullptr)
        throw std::invalid_argument("addr is null");

    if (len == 0 || len > sizeof(sockaddr_storage))
        throw std::invalid_argument("invalid socklen");

    if (!is_supported_family(addr->sa_family))
        throw std::invalid_argument("unsupported family");

    if (addr->sa_family == AF_INET && len != sizeof(sockaddr_in))
        throw std::invalid_argument("invalid socklen");
    else if (addr->sa_family == AF_INET6 && len != sizeof(sockaddr_in6))
        throw std::invalid_argument("invalid socklen");

    SockAddress buffer;
    std::memcpy(&buffer.ss_, addr, len);
    buffer.len_ = len;

    return buffer;
}

zportal::SockAddress zportal::SockAddress::ip4_numeric(const std::string& numeric, std::uint16_t port) {
    SockAddress buffer;
    auto* ip4 = reinterpret_cast<sockaddr_in*>(&buffer.ss_);

    ip4->sin_family = AF_INET;
    ip4->sin_port = ::htons(port);

    if (const int result = ::inet_pton(AF_INET, numeric.c_str(), &ip4->sin_addr); result == -1)
        throw std::system_error(errno, std::system_category(), "inet_pton");
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

zportal::SockAddress zportal::SockAddress::unix_path(const std::string& path) {
    if (path.size() > sizeof(sockaddr_un::sun_path) - 1)
        throw std::invalid_argument("unix path size is too long");

    if (is_binary(path))
        throw std::invalid_argument("unix path contains invalid characters");

    SockAddress buffer;
    auto* unix = reinterpret_cast<sockaddr_un*>(&buffer.ss_);
    unix->sun_family = AF_UNIX;
    buffer.len_ = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + path.size() + 1);
    std::strcpy(unix->sun_path, path.c_str());

    return buffer;
}

zportal::SockAddress zportal::SockAddress::unix_abstract(std::span<const std::byte> name) {
    if (name.size() > sizeof(sockaddr_un::sun_path) - 1)
        throw std::invalid_argument("unix name size is too long");

    SockAddress buffer;
    auto* unix = reinterpret_cast<sockaddr_un*>(&buffer.ss_);
    unix->sun_family = AF_UNIX;
    buffer.len_ = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + 1 + name.size());
    unix->sun_path[0] = '\0';
    std::memcpy(unix->sun_path + 1, name.data(), name.size());

    return buffer;
}

zportal::SockAddress zportal::SockAddress::unix_abstract(const std::string& name) {
    std::span<const std::byte> view{reinterpret_cast<const std::byte*>(name.data()), name.size()};
    return unix_abstract(view);
}

std::string zportal::HostPair::str(bool extended) const {
    return extended ? (hostname + ":" + std::to_string(port)) : hostname;
}

std::string zportal::to_string(Address address, bool extended) {
    return std::holds_alternative<SockAddress>(address) ? std::get<SockAddress>(address).str(extended)
                                                        : std::get<HostPair>(address).str(extended);
}