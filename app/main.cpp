#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <utility>

#include <csignal>
#include <cstdlib>

#include <liburing.h>

#include <zportal/address.hpp>
#include <zportal/config.hpp>
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

    zportal::Config config;
    zportal::parse_arguments(config, argn, argv);

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

    ::io_uring_queue_exit(&ring);

    return EXIT_SUCCESS;
}
