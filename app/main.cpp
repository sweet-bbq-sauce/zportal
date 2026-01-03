#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

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

constexpr unsigned entries = 32;
std::sig_atomic_t running = 1;

extern "C" void handle_signal(int) {
    running = 0;
}

int main(int argn, char* argv[]) {

    std::signal(SIGTERM, handle_signal);
    std::signal(SIGINT, handle_signal);

    static const auto help = [&]() {
        std::cout << "Usage:" << std::endl
                  << argv[0] << " ifname CIDR MTU --server/--client hostname port" << std::endl;
    };

    static const auto version = [&]() {
        std::cout << PROJECT_NAME << " " << ZPORTAL_VERSION << std::endl;
        std::cout << "Copyright (C) 2025-2026 Wiktor Sołtys <wiesiekdx@gmail.com>" << std::endl;
        std::cout << "Licensed under MIT license" << std::endl;
        std::cout << PROJECT_HOMEPAGE_URL << std::endl;
    };

    if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        help();
        return EXIT_SUCCESS;
    } else if (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v") {
        version();
        return EXIT_SUCCESS;
    }

    if (argn < 7) {
        help();
        return EXIT_FAILURE;
    }

    // TUN interface name
    const std::string ifname = argv[1];

    // CIDR
    const std::pair<in_addr, int> cidr = ([&]() {
        const std::string cidr_string = argv[2];
        const auto slash_pos = cidr_string.find('/');
        if (slash_pos == std::string::npos)
            throw std::invalid_argument("missing '/' in cidr");

        const zportal::SockAddress cidr_addr = zportal::SockAddress::ip4_numeric(cidr_string.substr(0, slash_pos), 0);
        const auto prefix = std::stoll(cidr_string.substr(slash_pos + 1));
        if (prefix < 0 || prefix > 32)
            throw std::runtime_error("cidr prefix must be in range 0-32");

        return std::pair<in_addr, int>{reinterpret_cast<const sockaddr_in*>(cidr_addr.get())->sin_addr,
                                       static_cast<int>(prefix)};
    })();

    // MTU
    const std::uint32_t mtu = ([&]() {
        const auto unverified_mtu = std::stoll(argv[3]);
        if (unverified_mtu < 0 || unverified_mtu > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("MTU must be in range 0-4294967295");

        return static_cast<std::uint32_t>(unverified_mtu);
    })();

    // Mode
    const std::string mode = argv[4];
    if (mode != "--server" && mode != "--client")
        throw std::invalid_argument("mode must be '--server' or '--client'");

    // Target
    const zportal::Address target = ([&]() {
        const auto unverified_port = std::stoll(argv[6]);
        if (unverified_port < 0 || unverified_port > std::numeric_limits<std::uint16_t>::max())
            throw std::runtime_error("port must be in range 0-65535");

        return zportal::HostPair{argv[5], static_cast<std::uint16_t>(unverified_port)};
    })();

    zportal::TUNInterface tun(ifname);
    tun.set_mtu(mtu);
    tun.set_cidr(cidr.first.s_addr, cidr.second);
    tun.set_up();

    io_uring ring{};
    if (const int result = ::io_uring_queue_init(entries, &ring, 0); result < 0)
        throw std::runtime_error(std::error_code{-result, std::system_category()}.message());

    static const auto handle_connection = [&ring](zportal::Socket& sock, zportal::TUNInterface& interface) {
        bool exited = false;
        zportal::Tunnel tunnel(&ring, std::move(sock), &interface, &exited);
        while (!exited) {
            io_uring_cqe* cqe = nullptr;
            if (const int result = ::io_uring_wait_cqe(&ring, &cqe); result < 0)
                throw std::runtime_error(std::error_code{-result, std::system_category()}.message());

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

    if (mode == "--server") {
        auto listener = zportal::create_listener(target);
        while (running) {
            auto client = zportal::accept_from(listener);
            handle_connection(client, tun);
        }
    } else if (mode == "--client") {
        while (running) {
            auto sock = zportal::connect_to(target);
            handle_connection(sock, tun);
            std::cout << "Reconnecting ..." << std::endl;
        }
    }

    ::io_uring_queue_exit(&ring);

    return EXIT_SUCCESS;
}