#pragma once

#include <span>
#include <initializer_list>

#include <cstddef>
#include <cstdint>

namespace zportal {

bool is_sse4_supported() noexcept;

std::uint32_t crc32c(std::span<const std::byte> data) noexcept;
std::uint32_t crc32c(std::initializer_list<std::span<const std::byte>> data) noexcept;

} // namespace zportal