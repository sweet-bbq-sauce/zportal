#pragma once

#include <deque>
#include <unordered_map>
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
    FrameParser() noexcept = default;
    using Chunk = Segment;
    explicit FrameParser(BufferRing& br);

    void push_buffer(std::uint16_t bid, std::size_t size) noexcept;
    std::optional<std::uint64_t> get_frame() noexcept;
    void free_frame(std::uint64_t frame) noexcept;
    Frame* get_frame_by_fd(std::uint64_t fd) noexcept;
    const Frame* get_frame_by_fd(std::uint64_t fd) const noexcept;

  private:
    enum { READING_HEADER, READING_PAYLOAD } state_{READING_HEADER};
    FrameHeader header_;
    Frame frame_;
    std::size_t read_progress_{};

    BufferRing* br_;
    std::uint64_t next_frame_id_{};

    std::deque<Chunk> input_queue_;
    std::deque<std::uint64_t> ready_frames_;
    std::unordered_map<std::uint64_t, Frame> frames_;
    std::deque<std::uint16_t> bid_to_return_;
    std::vector<std::uint32_t> bid_refcount_;
};

} // namespace zportal
