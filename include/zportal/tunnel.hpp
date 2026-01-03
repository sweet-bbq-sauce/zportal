#pragma once

#include <vector>

#include <cstdint>

#include <liburing.h>

#include "socket.hpp"
#include "tun.hpp"

namespace zportal {

struct Header {
    std::uint32_t size;
};

enum class OperationType {
    // RX
    TCP_RECV_HEADER,
    TCP_RECV,
    TUN_WRITE,

    // TX
    TUN_READ,
    TCP_SEND_HEADER,
    TCP_SEND
};

struct Operation {
    OperationType type;
    io_uring* ring;
};

class Tunnel {
  public:
    explicit Tunnel(io_uring* ring, Socket&& tcp, const TUNInterface* tun, bool* exited);

    Tunnel(Tunnel&&) noexcept;
    Tunnel& operator=(Tunnel&&) noexcept;

    Tunnel(const Tunnel&) = delete;
    Tunnel& operator=(const Tunnel&) = delete;

    ~Tunnel() noexcept;

    void close() noexcept;

    // RX
    void tcp_recv_header();
    void tcp_recv();
    void tun_write();

    // TX
    void tun_read();
    void tcp_send_header();
    void tcp_send();

    void handle_cqe(io_uring_cqe* cqe);

  private:
    Socket tcp_;
    const TUNInterface* tun_{};
    io_uring* ring_{};
    bool* exited_{};

    Header rx_header{}, tx_header{};
    std::vector<std::byte> rx, tx;
    std::size_t rx_current_processed{}, tx_current_processed{};
    OperationType rx_current_operation, tx_current_operation;
};

} // namespace zportal