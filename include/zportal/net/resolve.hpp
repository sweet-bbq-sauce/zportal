#pragma once

#include <zportal/net/address.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

Result<SockAddress> resolve(const Address& address) noexcept;

}