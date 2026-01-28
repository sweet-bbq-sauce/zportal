#pragma once

#include <vector>

#include <zportal/net/address.hpp>
#include <zportal/net/socket.hpp>

namespace zportal {

Socket connect_to(const Address& address, const std::vector<Address>& proxies = {});
Socket create_listener(const Address& address);
Socket accept_from(const Socket& listener);

} // namespace zportal