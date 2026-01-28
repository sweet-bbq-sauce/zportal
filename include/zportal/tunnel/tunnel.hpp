#pragma once

#include <vector>

#include <cstdint>

#include <liburing.h>

#include <zportal/net/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>

namespace zportal {

constexpr std::uint32_t magic = 0x5A504F52;

struct Header {
    std::uint32_t magic;
    std::uint32_t size;
    std::uint32_t crc;
};

enum class OperationType {
    // RX
    TCP_RECV_HEADER,
    TCP_RECV,
    TUN_WRITE,

    // TX
    TUN_READ,
    TCP_SEND_HEADER,
    TCP_SEND,

    MONITOR_REFRESH
};

struct Operation {
    OperationType type;
};

class Tunnel {
  public:
    explicit Tunnel(IOUring& ring, Socket&& tcp, const TUNInterface* tun, bool* exited);

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

    // Monitor refreshing
    void wait_for_refresh();
    void refresh_monitor();

    void handle_cqe(io_uring_cqe* cqe);

  private:
    void ensure_() const;

    Socket tcp_;
    const TUNInterface* tun_{};
    IOUring* ring_;
    bool* exited_{};

    Header rx_header{}, tx_header{};
    std::vector<std::byte> rx, tx;
    std::size_t rx_current_processed{}, tx_current_processed{};
    std::uintmax_t rx_total{}, tx_total{};
};

} // namespace zportal