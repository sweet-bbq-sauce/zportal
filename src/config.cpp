#include <iostream>

#include <cstdlib>

#include <unistd.h>

#include <zportal/config.hpp>

constexpr auto help = [](const std::string& program_name) {
    std::cout << "Usage:" << std::endl;
    std::cout << program_name
              << " -n <ifname> -m <MTU> -a <peer address> (-b <bind addr> | -c <connect addr>) [-p <proxy>]"
              << std::endl;
    std::cout << std::endl;
    std::cout << "-n <ifname> \t\tTUN device name. For example 'tun0', 'tun%d'." << std::endl;
    std::cout << "-m <MTU> \t\tDevice MTU. Range 0-4294967295." << std::endl;
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

zportal::Config zportal::parse_arguments(int argn, char* argv[]) {
    Config config;

    try {
        int opt;
        while ((opt = ::getopt(argn, argv, ":n:m:a:c:b:p:hv")) != -1) {
            switch (opt) {
            case 'n':
                if (!config.interface_name.empty())
                    throw std::invalid_argument("'-n' occurred again");

                config.interface_name = optarg;
                break;

            case 'm': {
                if (config.mtu != 0)
                    throw std::invalid_argument("'-m' occurred again");

                const auto unvalidated_mtu = std::stoll(optarg);
                if (unvalidated_mtu < 68 || unvalidated_mtu > std::numeric_limits<std::uint16_t>::max())
                    throw std::invalid_argument("MTU must be min 68 max 65535");

                config.mtu = static_cast<std::uint16_t>(unvalidated_mtu);
            } break;

            case 'a':
                if (config.inner_address)
                    throw std::invalid_argument("'-a' occurred again");

                config.inner_address = zportal::parse_cidr(optarg);
                break;

            case 'c': {
                if (config.connect_address)
                    throw std::invalid_argument("'-c' occurred again");

                config.connect_address = zportal::parse_address(optarg);
            }; break;

            case 'b': {
                if (config.bind_address)
                    throw std::invalid_argument("'-b' occurred again");

                config.bind_address = zportal::parse_address(optarg);
            }; break;

            case 'p':
                config.proxies.emplace_back(zportal::parse_address(optarg));
                break;

            case 'h':
                help(argv[0]);
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
        help(argv[0]);
        std::exit(EXIT_FAILURE);

    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    return config;
}