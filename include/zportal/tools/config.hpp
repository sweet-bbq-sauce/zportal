#pragma once

#include <exception>
#include <optional>
#include <string>
#include <vector>

#include <cstdint>

#include <zportal/net/address.hpp>

namespace zportal {

struct Config {

    // TUN interface
    std::string interface_name;
    zportal::Cidr inner_address;
    std::uint16_t mtu{};

    // Mode
    std::optional<zportal::Address> bind_address{};
    std::optional<zportal::Address> connect_address{};

    // Client config
    std::vector<zportal::Address> proxies;

    unsigned io_uring_entries{32};
};

std::exception_ptr parse_cli_arguments(zportal::Config& config, int argn, char* argv[], bool& end);

} // namespace zportal