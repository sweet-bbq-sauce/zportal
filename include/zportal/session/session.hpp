#pragma once

#include <zportal/iouring/iouring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/session/receiver.hpp>
#include <zportal/session/transmitter.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

class Session {
  public:
    Session() noexcept = default;
    static Result<Session> create_session(IoUring&& ring, TunDevice&& tun, Socket&& socket, std::size_t tx_queue_length,
                                          std::size_t rx_queue_length, std::size_t rx_buffer_size,
                                          const Config& cfg) noexcept;

    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Result<void> run() noexcept;

  private:
    IoUring ring_;
    TunDevice tun_;
    Socket socket_;

    const Config* cfg_;

    Receiver receiver_;
    Transmitter transmitter_;
};

} // namespace zportal