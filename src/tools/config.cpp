#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <string>

#include <cstdlib>

#include <unistd.h>

#include <zportal/tools/config.hpp>

std::atomic_bool zportal::verbose_mode{false};
std::atomic_bool zportal::monitor_mode{false};

constexpr auto help = [](zportal::Config& config, const std::string& program_name) {
    std::cout << "Usage:" << std::endl;
    std::cout << program_name
              << " -n <ifname> -m <MTU> -a <peer address> (-b <bind addr> | -c <connect addr>) [-p <proxy>]"
                 "-r <seconds> -e <count> -VM"
              << std::endl;
    std::cout << std::endl;
    std::cout << "-n <ifname> \t\tTUN device name. For example 'tun0', 'tun%d'." << std::endl;
    std::cout << "-m <MTU> \t\tDevice MTU. Range 68-" << std::numeric_limits<std::uint16_t>::max() << "." << std::endl;
    std::cout << "-a <inner address> \tInner IP4 cidr. For example 10.0.0.1/24." << std::endl;
    std::cout << "-b <bind address> \tServer mode." << std::endl;
    std::cout << "-c <connect address> \tClient mode." << std::endl;
    std::cout << "-r <seconds> \t\tReconnect duration. Has no effect with '-b'. Defaults: " << config.reconnect_duration
              << "." << std::endl;
    std::cout << "-e <count> \t\tError threshold. Has no effect with '-b'. Defaults: " << config.error_threshold << "."
              << std::endl;
    std::cout << "-p <proxy> \t\tProxy address." << std::endl;
    std::cout << "-V \t\t\tVerbose mode." << std::endl;
    std::cout << "-M \t\t\tMonitor mode." << std::endl;
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

void zportal::parse_arguments(zportal::Config& config, int argn, char* argv[]) {

    try {
        int opt;
        while ((opt = ::getopt(argn, argv, ":n:m:a:c:b:r:e:p:VhvM")) != -1) {
            switch (opt) {
            case 'n':
                config.interface_name = optarg;
                break;

            case 'm': {
                const auto unvalidated_mtu = std::stoll(optarg);
                if (unvalidated_mtu < 68 || unvalidated_mtu > std::numeric_limits<std::uint16_t>::max())
                    throw std::invalid_argument("MTU must be min 68 max 65535");

                config.mtu = static_cast<std::uint16_t>(unvalidated_mtu);
            } break;

            case 'a':
                config.inner_address = zportal::parse_cidr(optarg);
                break;

            case 'c':
                config.connect_address = zportal::parse_address(optarg);
                break;

            case 'b':
                config.bind_address = zportal::parse_address(optarg);
                break;

            case 'r':
                config.reconnect_duration = std::chrono::seconds(std::stoull(optarg));
                break;

            case 'e':
                config.error_threshold = std::stoull(optarg);
                break;

            case 'p':
                config.proxies.emplace_back(zportal::parse_address(optarg));
                break;

            case 'V':
                verbose_mode.store(true, std::memory_order_relaxed);
                break;

            case 'M':
                monitor_mode.store(true, std::memory_order_relaxed);
                break;

            case 'h':
                help(config, argv[0]);
                std::exit(EXIT_SUCCESS);

            case 'v':
                version();
                std::exit(EXIT_SUCCESS);

            case ':':
                throw std::invalid_argument(std::string("missing argument for '-") + char(optopt) + "'");

            case '?':
                throw std::invalid_argument(std::string("unknown option '-") + char(optopt) + "'");
            }
        }

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

    } catch (const std::invalid_argument& e) {
        std::cout << "Wrong argument: " << e.what() << std::endl;
        help(config, argv[0]);
        std::exit(EXIT_FAILURE);

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }
}