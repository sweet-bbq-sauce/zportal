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

struct InputDatagram {
    std::vector<std::byte> data;
};

class FrameParser;
class Frame {
  public:
    friend class FrameParser;
    std::vector<iovec> get_segments() const;

  private:
    std::vector<std::pair<std::uint16_t, std::span<const std::byte>>> segments_;
};

} // namespace zportal

#include "frame.inl"