#pragma once

#include "frame.hpp"

namespace zportal {

inline const Frame::SegmentTable& zportal::Frame::get_segments() const noexcept {
    return segments_;
}

inline std::uint64_t Frame::get_fd() const noexcept {
    return frame_fd_;
}

inline bool Frame::is_valid() const noexcept {
    return !segments_.empty();
}

inline Frame::operator bool() const noexcept {
    return is_valid();
}

} // namespace zportal