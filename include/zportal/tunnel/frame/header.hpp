#pragma once

#include <array>
#include <span>

#include <cstddef>
#include <cstdint>

namespace zportal {

class FrameHeader {
  public:
    static constexpr std::uint32_t magic = 0x5A505254;
    static constexpr std::size_t wire_size = 16;

    void clean() noexcept;

    std::uint32_t get_magic() const noexcept;

    std::uint32_t get_flags() const noexcept;
    void set_flags(std::uint32_t flags) noexcept;

    std::uint32_t get_size() const noexcept;
    void set_size(std::uint32_t size) noexcept;

    std::uint32_t get_crc() const noexcept;
    void set_crc(std::uint32_t crc) noexcept;

    std::span<std::byte, wire_size> data() noexcept;
    std::span<const std::byte, wire_size> data() const noexcept;

  private:
    std::uint32_t get_u32_(std::size_t offset) const noexcept;
    void set_u32_(std::size_t offset, std::uint32_t value) noexcept;

    std::array<std::byte, wire_size> data_;
};

} // namespace zportal

#include "header.inl"