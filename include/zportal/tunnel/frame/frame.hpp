#pragma once

#include <span>
#include <vector>
#include <utility>

#include <cstddef>
#include <cstdint>

#include <sys/uio.h>

namespace zportal {

struct Segment {
    std::uint16_t bid;
    std::size_t offset, length;
};

class FrameParser;
class Frame {
  public:
    friend class FrameParser;
    const std::vector<iovec>& get_segments() const;

  private:
    std::vector<std::uint16_t> bids_;
    std::vector<iovec> segments_;
};

} // namespace zportal

#include "frame.inl"