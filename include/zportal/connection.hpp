#pragma once

#include "address.hpp"
#include "socket.hpp"

namespace zportal {

Socket connect_to(const Address& address);
Socket create_listener(const Address& address);
Socket accept_from(const Socket& listener);

} // namespace zportal