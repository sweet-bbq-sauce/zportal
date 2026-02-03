#pragma once

#include <span>
#include <utility>
#include <vector>

#include <cstddef>
#include <cstdint>

namespace zportal {

struct Segment {
    std::uint16_t bid;
    std::size_t offset, length;
};

class FrameParser;
class Frame {
  public:
    friend class FrameParser;
    using SegmentTable = std::vector<std::pair<std::uint16_t, std::span<const std::byte>>>;

    const SegmentTable& get_segments() const noexcept;
    std::uint64_t get_fd() const noexcept;

    bool is_valid() const noexcept;
    explicit operator bool() const noexcept;

  private:
    explicit Frame(std::uint64_t frame_fd) noexcept;

    SegmentTable segments_;
    std::uint64_t frame_fd_;
};

} // namespace zportal

#include "frame.inl"