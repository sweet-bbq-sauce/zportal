#pragma once

#include <deque>

#include <zportal/iouring/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tunnel/peer.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

class Tunnel {
  public:
    explicit Tunnel(IOUring&& ring, TUNInterface&& tun, Socket&& sock, const Config& cfg) noexcept;

    Tunnel(Tunnel&&) = delete;
    Tunnel& operator=(Tunnel&&) = delete;
    Tunnel(const Tunnel&) = delete;
    Tunnel& operator=(const Tunnel&) = delete;

    Result<void> run();
    void close(Error reason = {}) noexcept;

  private:
    const Config* cfg_;

    bool closing_{};
    Error close_reason_{};

    IOUring ring_;
    TUNInterface tun_;
    Peer peer_;
    BufferRing tun_br_;
    std::deque<std::uint16_t> tun_write_queue_;

    void kick_send_();
    void kick_write_();
    bool send_inprogress{};
    bool write_inprogress{};
};

} // namespace zportal
