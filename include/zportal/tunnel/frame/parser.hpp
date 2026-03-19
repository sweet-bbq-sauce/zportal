#pragma once

#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

#include <cassert>
#include <cstdint>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tunnel/frame/frame.hpp>
#include <zportal/tunnel/frame/header.hpp>

namespace zportal {

class FrameParser {
  public:
    FrameParser() noexcept = default;
    using Chunk = Segment;
    explicit FrameParser(BufferRing& br, const Config& cfg);

    FrameParser(FrameParser&&) noexcept;
    FrameParser& operator=(FrameParser&&) noexcept;
    FrameParser(const FrameParser&) = delete;
    FrameParser& operator=(const FrameParser&) = delete;

    enum class ParserError { OK, WRONG_MAGIC, INVALID_SIZE, INTERNAL_ERROR };

    [[nodiscard]] ParserError push_buffer(std::uint16_t bid, std::size_t size);
    [[nodiscard]] std::optional<std::uint16_t> get_frame() noexcept;
    void free_frame(std::uint16_t frame) noexcept;
    Frame* get_frame_by_fd(std::uint16_t fd) noexcept;
    const Frame* get_frame_by_fd(std::uint16_t fd) const noexcept;

  private:
    const Config* cfg_;
    enum { READING_HEADER, READING_PAYLOAD } state_{READING_HEADER};
    FrameHeader header_;
    Frame frame_;
    std::size_t read_progress_{};

    BufferRing* br_;
    std::uint16_t next_frame_id_{};

    std::deque<Chunk> input_queue_;
    std::deque<std::uint16_t> ready_frames_;
    std::unordered_map<std::uint64_t, Frame> frames_;
    std::deque<std::uint16_t> bid_to_return_;
    std::vector<std::uint32_t> bid_refcount_;
};

} // namespace zportal
