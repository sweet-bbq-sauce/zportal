#pragma once

#include "frame.hpp"

namespace zportal {

inline std::vector<iovec> zportal::Frame::get_segments() const {
    std::vector<iovec> buffer;
    for(auto segment : segments_)
        buffer.push_back({const_cast<std::byte*>(segment.second.data()), segment.second.size()});
    return buffer;
}

} // namespace zportal