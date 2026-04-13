#pragma once

#include <queue>
#include <vector>

#include <cstddef>
#include <cstdint>

#include <sys/uio.h>

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
    static Result<Receiver> create_receiver(IoUring& ring, TunDevice& tun, Socket& socket, std::size_t queue_length,
                                            std::size_t buffer_size) noexcept;

    Receiver(Receiver&&) noexcept;
    Receiver& operator=(Receiver&&) noexcept;
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

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
    std::size_t used_buffers_{};

    struct InputBuffer {
        std::uint16_t bid;
        std::size_t size;
        std::size_t offset{};
    };
    std::queue<InputBuffer> input_buffer_queue_;
    std::vector<std::uint32_t> buffer_refcounts_;

    enum class ParseState { PARSING_HEADER, PARSING_PAYLOAD } state_{ParseState::PARSING_HEADER};

    FrameHeader header_{};
    std::size_t header_progress_{};

    struct OutputFrame {
        std::vector<std::uint16_t> bid;
        std::vector<iovec> segments;
    };
    OutputFrame frame_;
    std::size_t payload_progress_{};

    std::queue<OutputFrame> output_frame_queue_;
    bool write_in_progress_{false};

    Result<void> handle_write_cqe_(const Cqe& cqe) noexcept;
    Result<void> handle_recv_cqe_(const Cqe& cqe) noexcept;

    Result<void> kick_parse_() noexcept;
    Result<void> kick_write_() noexcept;
};

} // namespace zportal
