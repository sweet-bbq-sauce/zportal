#pragma once

#include <vector>

#include <zportal/net/address.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

Result<Socket> connect_to(const Address& address, const std::vector<Address>& proxies = {}) noexcept;
Result<Socket> create_listener(const Address& address) noexcept;
Result<Socket> accept_from(const Socket& listener) noexcept;

Result<void> socks5_connect(Socket& socket, const Address& address) noexcept;

} // namespace zportal