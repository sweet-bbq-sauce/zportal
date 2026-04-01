#include <exception>
#include <iostream>
#include <limits>
#include <string>

#include <cstdlib>

#include <unistd.h>

#include <zportal/tools/config.hpp>

constexpr auto help = [](zportal::Config& config, const std::string& program_name) {
    std::cout << "Usage:" << std::endl;
    std::cout << program_name
              << " -n <ifname> -m <MTU> -a <peer address> (-b <bind addr> | -c <connect addr>) [-p <proxy>]"
              << std::endl;
    std::cout << std::endl;
    std::cout << "-n <ifname> \t\tTUN device name. For example 'tun0', 'tun%d'." << std::endl;
    std::cout << "-m <MTU> \t\tDevice MTU. Range 68-" << std::numeric_limits<std::uint16_t>::max() << "." << std::endl;
    std::cout << "-a <inner address> \tInner IP4 cidr. For example 10.0.0.1/24." << std::endl;
    std::cout << "-b <bind address> \tServer mode." << std::endl;
    std::cout << "-c <connect address> \tClient mode." << std::endl;
    std::cout << "-p <proxy> \t\tProxy address." << std::endl;
    std::cout << std::endl;
    std::cout << "-h \tPrint this help info." << std::endl;
    std::cout << "-v \tPrint version." << std::endl;
};

constexpr auto version = []() {
    std::cout << PROJECT_NAME << " " << ZPORTAL_VERSION << std::endl;
    std::cout << "Copyright (C) 2025-2026 Wiktor Sołtys <wiesiekdx@gmail.com>" << std::endl;
    std::cout << "Licensed under MIT license" << std::endl;
    std::cout << PROJECT_HOMEPAGE_URL << std::endl;
};

std::exception_ptr zportal::parse_cli_arguments(zportal::Config& config, int argn, char* argv[], bool& end) {
    end = false;
    try {
        int opt;
        while ((opt = ::getopt(argn, argv, ":n:m:a:c:b:p:hv")) != -1) {
            switch (opt) {
            case 'n': {
                config.interface_name = optarg;
                break;
            }

            case 'm': {
                const auto unvalidated_mtu = std::stoll(optarg);
                if (unvalidated_mtu < 68 || unvalidated_mtu > std::numeric_limits<std::uint16_t>::max())
                    throw std::invalid_argument("MTU must be min 68 max 65535");

                config.mtu = static_cast<std::uint16_t>(unvalidated_mtu);
                break;
            }

            case 'a': {
                config.inner_address = zportal::parse_cidr(optarg);
                break;
            }

            case 'c': {
                config.connect_address = zportal::parse_address(optarg);
                break;
            }

            case 'b': {
                config.bind_address = zportal::parse_address(optarg);
                break;
            }

            case 'p': {
                config.proxies.emplace_back(zportal::parse_address(optarg));
                break;
            }

            case 'h': {
                help(config, argv[0]);
                end = true;
                break;
            }

            case 'v': {
                version();
                end = true;
                break;
            }

            case ':':
                throw std::invalid_argument(std::string("missing argument for '-") + char(optopt) + "'");

            case '?':
                throw std::invalid_argument(std::string("unknown option '-") + char(optopt) + "'");
            }
        }

        if (end)
            return {};

        if (optind != argn)
            throw std::invalid_argument("too many arguments");

        if (config.interface_name.empty())
            throw std::invalid_argument("interface name is not set");

        if (config.mtu == 0)
            throw std::invalid_argument("MTU is not set");

        if (!config.inner_address)
            throw std::invalid_argument("inner address is not set");

        if (bool(config.bind_address) == bool(config.connect_address))
            throw std::invalid_argument("exactly one of '-b' or '-c' must be set");

    } catch (...) {
        return std::current_exception();
    }

    return {};
}