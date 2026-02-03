#pragma once

#include <deque>
#include <optional>
#include <vector>

#include <cassert>
#include <cstdint>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/tunnel/frame/frame.hpp>
#include <zportal/tunnel/frame/header.hpp>

namespace zportal {

class FrameParser {
  public:
    using Chunk = Segment;
    explicit FrameParser(BufferRing& br);

    void push_buffer(std::uint16_t bid, std::size_t size) noexcept;
    std::optional<Frame> get_frame() noexcept;
    void free_frame(Frame& frame) noexcept;

  private:
    enum { READING_HEADER, READING_PAYLOAD } state_{READING_HEADER};
    FrameHeader header_;
    Frame frame_;
    std::size_t read_progress_{};

    BufferRing* br_;
    std::uint64_t next_frame_id_{};

    std::deque<Chunk> input_queue_;
    std::deque<Frame> ready_frames_;
    std::deque<std::uint16_t> bid_to_return_;
    std::vector<std::uint32_t> bid_refcount_;
};

} // namespace zportal