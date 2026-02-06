#pragma once

#include <array>
#include <span>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <endian.h>

#include "header.hpp"

namespace zportal {

inline void FrameHeader::clean() noexcept {
    std::memset(data_.data(), 0x00, data_.size());
    set_u32_(0, magic);
}

inline std::uint32_t FrameHeader::get_magic() const noexcept {
    return get_u32_(0);
}

inline std::uint32_t FrameHeader::get_flags() const noexcept {
    return get_u32_(4);
}

inline void FrameHeader::set_flags(std::uint32_t flags) noexcept {
    set_u32_(4, flags);
}

inline std::uint32_t FrameHeader::get_size() const noexcept {
    return get_u32_(8);
}

inline void FrameHeader::set_size(std::uint32_t size) noexcept {
    set_u32_(8, size);
}

inline std::uint32_t FrameHeader::get_crc() const noexcept {
    return get_u32_(12);
}

inline void FrameHeader::set_crc(std::uint32_t crc) noexcept {
    set_u32_(12, crc);
}

inline std::span<std::byte, FrameHeader::wire_size> FrameHeader::data() noexcept {
    return data_;
}

inline std::span<const std::byte, FrameHeader::wire_size> FrameHeader::data() const noexcept {
    return data_;
}

inline std::uint32_t FrameHeader::get_u32_(std::size_t offset) const noexcept {
    assert(offset + 4 <= wire_size);

    std::uint32_t value;
    std::memcpy(&value, data_.data() + offset, 4);
    return ::be32toh(value);
}

inline void FrameHeader::set_u32_(std::size_t offset, std::uint32_t value) noexcept {
    assert(offset + 4 <= wire_size);

    value = ::htobe32(value);
    std::memcpy(data_.data() + offset, &value, 4);
}

} // namespace zportal