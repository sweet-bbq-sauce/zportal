#pragma once

#include "zportal/iouring/buffer_ring.hpp"
#include <atomic>
#include <thread>

#include <zportal/iouring/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tunnel/peer.hpp>

namespace zportal {

class Tunnel {
  public:
    explicit Tunnel(IOUring&& ring, TUNInterface&& tun, Socket&& sock) noexcept;

    void wait() {
        if (jt_.joinable())
            jt_.join();
    }

  private:
    std::jthread jt_;
    std::atomic_bool running_{true};

    IOUring ring_;
    TUNInterface tun_;
    Peer peer_;
    BufferRing tun_br_;
    bool closing{};

    void loop_();
    void kick_send_();
    bool send_inprogress{};
};

} // namespace zportal