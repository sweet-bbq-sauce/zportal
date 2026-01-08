#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <cstdint>

#include "address.hpp"

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
    std::chrono::seconds reconnect_duration{5};
    std::chrono::seconds connect_timeout{5};
    std::size_t error_threshold{10};

    unsigned io_uring_entries{32};
};

void parse_arguments(zportal::Config& config, int argn, char* argv[]);

} // namespace zportal