#pragma once

#include <zportal/tools/error.hpp>

namespace zportal {

namespace support_check {

Result<bool> recv_multishot() noexcept;
Result<bool> read_multishot() noexcept;

} // namespace support_check

} // namespace zportal