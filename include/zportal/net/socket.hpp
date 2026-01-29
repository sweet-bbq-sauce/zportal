#pragma once

#include <sys/socket.h>

#include <zportal/net/address.hpp>
#include <zportal/tools/file_descriptor.hpp>

namespace zportal {

class Socket {
  public:
    Socket() noexcept = default;
    explicit Socket(int fd, sa_family_t family = AF_UNSPEC);

    Socket(Socket&&) noexcept;
    Socket& operator=(Socket&&) noexcept;

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    ~Socket() noexcept;
    void close() noexcept;

    [[nodiscard]] int get() const noexcept;

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    [[nodiscard]] SockAddress get_local_address() const;
    [[nodiscard]] SockAddress get_remote_address() const;

    [[nodiscard]] sa_family_t get_family() const noexcept;

    [[nodiscard]] static Socket create_socket(sa_family_t family, int flags = 0);

    sa_family_t detect_family() const;

  private:
    FileDescriptor fd_;
    mutable sa_family_t family_{AF_UNSPEC};
};

} // namespace zportal