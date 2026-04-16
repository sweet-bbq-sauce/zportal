#pragma once

#include <cstddef>

#include <zportal/tools/error.hpp>

namespace zportal {
namespace system {

Result<std::size_t> get_page_size() noexcept;

}
} // namespace zportal