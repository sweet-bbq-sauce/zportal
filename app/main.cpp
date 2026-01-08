#include <chrono>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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

    std::signal(SIGTERM, handle_signal);
    std::signal(SIGINT, handle_signal);

    constexpr auto help = [](std::string_view program_name) {
        std::cout << "Usage:" << std::endl
                  << program_name << " ifname CIDR MTU --server/--client hostname port" << std::endl;
    };

    static const auto version = []() {
        std::cout << PROJECT_NAME << " " << ZPORTAL_VERSION << std::endl;
        std::cout << "Copyright (C) 2025-2026 Wiktor Sołtys <wiesiekdx@gmail.com>" << std::endl;
        std::cout << "Licensed under MIT license" << std::endl;
        std::cout << PROJECT_HOMEPAGE_URL << std::endl;
    };

    struct Config {
        std::string interface_name;
        zportal::Cidr inner_address;
        std::uint32_t mtu;
        std::optional<zportal::Address> bind_address;
        std::optional<zportal::Address> connect_address;
        std::vector<zportal::Address> proxies;
        std::chrono::seconds reconnect_duration{5};
        unsigned io_uring_entries{32};
    } config;

    zportal::TUNInterface tun(config.interface_name);
    tun.set_mtu(config.mtu);
    tun.set_cidr(config.inner_address);
    tun.set_up();

    io_uring ring{};
    if (const int result = ::io_uring_queue_init(config.io_uring_entries, &ring, 0); result < 0)
        throw std::system_error(-result, std::system_category(), "io_uring_wait_cqe");

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
            handle_connection(ring, client, tun);
        }
    } else if (config.connect_address) {
        while (running) {
            auto sock = zportal::connect_to(*config.connect_address, config.proxies);
            handle_connection(ring, sock, tun);
            std::cout << "Reconnecting in " << config.reconnect_duration << " seconds ..." << std::endl;
            std::this_thread::sleep_for(config.reconnect_duration);
        }
    }

    ::io_uring_queue_exit(&ring);

    return EXIT_SUCCESS;
}
