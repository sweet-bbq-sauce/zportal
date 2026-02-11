#pragma once

#include "frame.hpp"

namespace zportal {

inline const std::vector<iovec>& zportal::Frame::get_segments() const {
    return segments_;
}

} // namespace zportal