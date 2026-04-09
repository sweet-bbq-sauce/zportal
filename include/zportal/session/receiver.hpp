#pragma once

#include <queue>

#include <cstdint>

#include <zportal/iouring/buffer_group.hpp>
#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/iouring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/session/frame_header.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

class Receiver {
  public:
    Receiver() noexcept = default;
    Result<Receiver> create_receiver(IoUring& ring, TunDevice& tun, Socket& socket, std::size_t queue_length) noexcept;

    Result<void> arm_recv() noexcept;
    Result<void> handle_cqe(const Cqe& cqe) noexcept;

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

  private:
    IoUring* ring_{};
    TunDevice* tun_{};
    Socket* socket_{};
    BufferGroup* bg_{};

    bool cooling_down_{false};

    struct InSegment {
        std::uint16_t bid;
        std::uint32_t size;
    };
    std::queue<InSegment> segment_queue_;

    Result<void> handle_write_cqe_(const Cqe& cqe) noexcept;
    Result<void> handle_recv_cqe_(const Cqe& cqe) noexcept;
};

} // namespace zportal