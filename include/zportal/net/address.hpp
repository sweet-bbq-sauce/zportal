#pragma once

#include <span>
#include <string>
#include <variant>

#include <cstddef>
#include <cstdint>

#include <sys/socket.h>

namespace zportal {

class SockAddress {
  public:
    SockAddress() noexcept = default;

    std::string str(bool extended = true) const;
    sa_family_t family() const noexcept;
    const sockaddr* get() const;
    socklen_t length() const noexcept;

    explicit operator bool() const noexcept;
    bool is_valid() const noexcept;
    bool is_connectable() const noexcept;
    bool is_bindable() const noexcept;

    bool is_ip() const noexcept;
    bool is_unix() const noexcept;

    static SockAddress from_sockaddr(const sockaddr* addr, socklen_t len);
    static SockAddress ip4_numeric(const std::string& numeric, std::uint16_t port);
    static SockAddress ip6_numeric(const std::string& numeric, std::uint16_t port);
    static SockAddress unix_path(const std::string& path);
    static SockAddress unix_abstract(std::span<const std::byte> name);
    static SockAddress unix_abstract(const std::string& name);

  private:
    sockaddr_storage ss_{};
    socklen_t len_{};
};

class Cidr {
  public:
    Cidr() noexcept = default;
    explicit Cidr(SockAddress address, std::uint8_t prefix);

    const SockAddress& get_address() const noexcept;
    const std::uint8_t get_prefix() const noexcept;

    bool is_ip4() const noexcept;
    bool is_ip6() const noexcept;

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

    Cidr get_network() const noexcept;
    bool is_in_network(const SockAddress& address) const noexcept;

    std::string str() const;

  private:
    SockAddress address_{};
    std::uint8_t prefix_{};
};

struct HostPair {
    std::string hostname;
    std::uint16_t port;

    bool is_connectable() const noexcept;
    bool is_bindable() const noexcept;

    std::string str(bool extended) const;
};

using Address = std::variant<SockAddress, HostPair>;
std::string to_string(Address address, bool extended = true);

Address parse_address(const std::string& address);
Cidr parse_cidr(const std::string& cidr);

} // namespace zportal