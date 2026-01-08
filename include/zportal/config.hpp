#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <cstdint>

#include "address.hpp"

namespace zportal {

struct Config {
    std::string interface_name;
    zportal::Cidr inner_address;
    std::uint16_t mtu{};
    std::optional<zportal::Address> bind_address{};
    std::optional<zportal::Address> connect_address{};
    std::vector<zportal::Address> proxies;
    std::chrono::seconds reconnect_duration{5};
    unsigned io_uring_entries{32};
};

Config parse_arguments(int argn, char* argv[]);

} // namespace zportal