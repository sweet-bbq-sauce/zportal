#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <utility>

#include <csignal>
#include <cstdlib>

#include <liburing.h>

#include <zportal/net/address.hpp>
#include <zportal/net/connection.hpp>
#include <zportal/net/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tunnel/tunnel.hpp>

std::sig_atomic_t running = 1;

extern "C" void handle_signal(int) {
    running = 0;
}

int main(int argn, char* argv[]) {

    // std::signal(SIGTERM, handle_signal);
    // std::signal(SIGINT, handle_signal);

    zportal::Config config;
    zportal::parse_arguments(config, argn, argv);

    zportal::TUNInterface tun(config.interface_name);
    tun.set_mtu(config.mtu);
    tun.set_cidr(config.inner_address);
    tun.set_up();

    zportal::IOUring ring(config.io_uring_entries);

    constexpr auto handle_connection = [](zportal::IOUring& ring, zportal::Socket& sock,
                                          zportal::TUNInterface& interface) {
        bool exited = false;
        zportal::Tunnel tunnel(ring, std::move(sock), &interface, &exited);
        while (!exited) {
            io_uring_cqe* cqe = ring.get_cqe();

            auto* operation = reinterpret_cast<zportal::Operation*>(::io_uring_cqe_get_data(cqe));
            if (operation == nullptr)
                throw std::runtime_error("operation is null");

            tunnel.handle_cqe(cqe);

            delete operation;
            ring.seen(cqe);
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
        std::size_t errors{};
        while (running) {

            if (errors >= config.error_threshold) {
                std::cout << "Exiting." << std::endl;
                return EXIT_FAILURE;
            }

            zportal::Socket sock;
            try {
                sock = zportal::connect_to(*config.connect_address, config.proxies);

                errors = 0;
                std::cout << "Connected to " << zportal::to_string(*config.connect_address) << std::endl;
            } catch (const std::exception& e) {
                errors++;
                std::cout << "Can't connect to " << zportal::to_string(*config.connect_address) << ": " << e.what()
                          << std::endl;
                std::cout << "Reconnecting in " << config.reconnect_duration << " seconds." << std::endl;
                std::this_thread::sleep_for(config.reconnect_duration);
                continue;
            }

            try {
                handle_connection(ring, sock, tun);
            } catch (const std::exception& e) {
                errors++;
                std::cout << "Disconnected from " << zportal::to_string(*config.connect_address) << ": " << e.what()
                          << std::endl;
                std::cout << "Reconnecting in " << config.reconnect_duration << " seconds." << std::endl;
                std::this_thread::sleep_for(config.reconnect_duration);
                continue;
            }
        }
    }

    return EXIT_SUCCESS;
}
