#pragma once

#include <optional>
#include <queue>
#include <vector>

#include <cstddef>
#include <cstdint>

#include <sys/socket.h>
#include <sys/uio.h>

#include <zportal/iouring/buffer_group.hpp>
#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/iouring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/session/frame_header.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

class Transmitter {
  public:
    Transmitter() noexcept = default;
    static Result<Transmitter> create_transmitter(IoUring& ring, TunDevice& tun, Socket& sock,
                                                  std::uint16_t queue_length) noexcept;

    Result<void> arm_read() noexcept;
    Result<void> handle_cqe(const Cqe& cqe) noexcept;

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

  private:
    IoUring* ring_{};
    TunDevice* tun_{};
    BufferGroup* bg_{};
    Socket* sock_{};

    struct OutFrame {
        std::uint16_t bid;
        std::uint32_t size;
    };
    std::queue<OutFrame> frame_queue_;
    bool cooling_down_{false};

    struct CurrentFrameState {
        OutFrame frame;
        FrameHeader header{};

        std::size_t bytes_sent{};
        std::vector<iovec> segments;
        msghdr message_header{};
    };
    bool send_in_progress_{false};
    std::optional<CurrentFrameState> current_frame_state_{};

    Result<void> handle_read_cqe_(const Cqe& cqe) noexcept;
    Result<void> handle_send_cqe_(const Cqe& cqe) noexcept;

    Result<FrameHeader> create_frame_header_(const OutFrame& frame) noexcept;
    Result<void> kick_send_() noexcept;
};

} // namespace zportal