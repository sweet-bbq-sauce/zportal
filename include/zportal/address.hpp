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

struct HostPair {
    std::string hostname;
    std::uint16_t port;

    bool is_connectable() const noexcept;
    bool is_bindable() const noexcept; 
};

using Address = std::variant<SockAddress, HostPair>;

} // namespace zportal