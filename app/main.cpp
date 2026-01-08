#include <chrono>
#include <exception>
#include <getopt.h>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <csignal>
#include <cstdint>
#include <cstdlib>

#include <arpa/inet.h>
#include <liburing.h>
#include <netinet/in.h>
#include <unistd.h>

#include <zportal/address.hpp>
#include <zportal/connection.hpp>
#include <zportal/socket.hpp>
#include <zportal/tun.hpp>
#include <zportal/tunnel.hpp>

std::sig_atomic_t running = 1;

extern "C" void handle_signal(int) {
    running = 0;
}

int main(int argn, char* argv[]) {

    // std::signal(SIGTERM, handle_signal);
    // std::signal(SIGINT, handle_signal);

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

    struct Config {
        std::string interface_name;
        zportal::Cidr inner_address;
        std::uint16_t mtu{};
        std::optional<zportal::Address> bind_address{};
        std::optional<zportal::Address> connect_address{};
        std::vector<zportal::Address> proxies;
        std::chrono::seconds reconnect_duration{5};
        unsigned io_uring_entries{32};
    } config;

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
                return EXIT_SUCCESS;

            case 'v':
                version();
                return EXIT_SUCCESS;

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
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    zportal::TUNInterface tun(config.interface_name);
    tun.set_mtu(config.mtu);
    tun.set_cidr(config.inner_address);
    tun.set_up();

    io_uring ring{};
    if (const int result = ::io_uring_queue_init(config.io_uring_entries, &ring, 0); result < 0)
        throw std::system_error(-result, std::system_category(), "io_uring_queue_init");

    constexpr auto handle_connection = [](io_uring& ring, zportal::Socket& sock, zportal::TUNInterface& interface) {
        bool exited = false;
        zportal::Tunnel tunnel(&ring, std::move(sock), &interface, &exited);
        while (!exited) {
            io_uring_cqe* cqe = nullptr;
            if (const int result = ::io_uring_wait_cqe(&ring, &cqe); result < 0)
                throw std::system_error(-result, std::system_category(), "io_uring_wait_cqe");

            if (!cqe)
                throw std::runtime_error("cqe is null");

            auto* operation = reinterpret_cast<zportal::Operation*>(::io_uring_cqe_get_data(cqe));
            if (operation == nullptr)
                throw std::runtime_error("operation is null");

            tunnel.handle_cqe(cqe);

            delete operation;
            ::io_uring_cqe_seen(&ring, cqe);
        }
    };

    if (config.bind_address) {
        auto listener = zportal::create_listener(*config.bind_address);
        while (running) {
            auto client = zportal::accept_from(listener);
            std::cout << "New connection from " << client.get_remote_address().str() << std::endl;

            handle_connection(ring, client, tun);

            std::cout << "Client disconnected" << std::endl;
        }
    } else if (config.connect_address) {
        while (running) {
            auto sock = zportal::connect_to(*config.connect_address, config.proxies);
            std::cout << "Connected to " << zportal::to_string(*config.connect_address) << std::endl;

            handle_connection(ring, sock, tun);

            std::cout << "Disconnected. Reconnecting in " << config.reconnect_duration << " seconds ..." << std::endl;
            std::this_thread::sleep_for(config.reconnect_duration);
        }
    }

    ::io_uring_queue_exit(&ring);

    return EXIT_SUCCESS;
}
