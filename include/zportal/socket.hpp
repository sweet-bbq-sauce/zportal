#pragma once

#include <sys/socket.h>

namespace zportal {

class Socket {
  public:
    Socket() = default;
    
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

    static Socket create_socket(sa_family_t family);

  private:
    explicit Socket(int fd, sa_family_t family);
    int fd_{-1};
    sa_family_t family_{};
};

} // namespace zportal