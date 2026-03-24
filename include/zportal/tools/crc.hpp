#pragma once

#include <span>
#include <vector>

#include <cstddef>
#include <cstdint>

#include <sys/uio.h>

namespace zportal {

bool is_sse4_supported() noexcept;

std::uint32_t crc32c(std::span<const std::byte> data) noexcept;
std::uint32_t crc32c(const std::vector<std::span<const std::byte>>& data) noexcept;
std::uint32_t crc32c(const std::vector<iovec>& data) noexcept;

} // namespace zportal