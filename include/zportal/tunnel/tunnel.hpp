#pragma once

#include <atomic>
#include <deque>
#include <future>

#include <zportal/iouring/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tunnel/peer.hpp>

namespace zportal {

class Tunnel {
  public:
    explicit Tunnel(IOUring&& ring, TUNInterface&& tun, Socket&& sock, const Config& cfg) noexcept;

    Tunnel(Tunnel&&) = delete;
    Tunnel& operator=(Tunnel&&) = delete;
    Tunnel(const Tunnel&) = delete;
    Tunnel& operator=(const Tunnel&) = delete;

    void wait();
    void stop() noexcept;
    bool running() const noexcept;

  private:
    const Config* cfg_;

    std::future<void> thread_;
    std::atomic_bool running_{true};

    IOUring ring_;
    TUNInterface tun_;
    Peer peer_;
    BufferRing tun_br_;
    std::deque<std::uint16_t> tun_write_queue_;
    bool closing{};

    void loop_();
    void kick_send_();
    void kick_write_();
    bool send_inprogress{};
    bool write_inprogress{};
};

} // namespace zportal
