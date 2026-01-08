#pragma once

#include <sys/socket.h>

#include "address.hpp"

namespace zportal {

class Socket {
  public:
    Socket() = default;
    explicit Socket(int fd, sa_family_t family);

    Socket(Socket&&) noexcept;
    Socket& operator=(Socket&&) noexcept;

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    ~Socket() noexcept;

    void close() noexcept;

    int get_fd() const noexcept;
    sa_family_t get_family() const noexcept;

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

    SockAddress get_local_address() const;
    SockAddress get_remote_address() const;

    static Socket create_socket(sa_family_t family);

  private:
    int fd_{-1};
    sa_family_t family_{};
};

} // namespace zportal