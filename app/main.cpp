#include <exception>
#include <iostream>

#include <cstdlib>

#include <zportal/iouring/iouring.hpp>
#include <zportal/net/address.hpp>
#include <zportal/net/connection.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/session/session.hpp>
#include <zportal/session/transmitter.hpp>
#include <zportal/tools/config.hpp>

int main(int argn, char* argv[]) {
    zportal::Config cfg{};

    bool end = false;
    if (const auto result = zportal::parse_cli_arguments(cfg, argn, argv, end); result) {
        try {
            std::rethrow_exception(result);
        } catch (const std::exception& e) {
            std::cerr << "CLI options error: " << e.what() << std::endl;
        }

        return EXIT_FAILURE;
    }

    if (end)
        return EXIT_SUCCESS;

    auto ring = zportal::IoUring::create_queue(64);
    if (!ring) {
        std::cerr << ring.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    auto tun_device = zportal::TunDevice::create_tun_device(cfg.interface_name, cfg.inner_address, cfg.mtu);
    if (!tun_device) {
        std::cerr << tun_device.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    zportal::Socket socket;
    if (cfg.bind_address) {
        auto listener = zportal::create_listener(*cfg.bind_address);
        if (!listener) {
            std::cerr << listener.error().to_string() << std::endl;
            return EXIT_FAILURE;
        }

        auto peer = zportal::accept_from(*listener);
        if (!peer) {
            std::cerr << peer.error().to_string() << std::endl;
            return EXIT_FAILURE;
        }
        socket = std::move(*peer);

        const auto remote_address = socket.get_remote_address();
        if (!remote_address) {
            std::cerr << remote_address.error().to_string() << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "Accepted connection from " << zportal::to_string(*remote_address) << std::endl;
    } else if (cfg.connect_address) {
        auto peer = zportal::connect_to(*cfg.connect_address, cfg.proxies);
        if (!peer) {
            std::cerr << peer.error().to_string() << std::endl;
            return EXIT_FAILURE;
        }
        socket = std::move(*peer);

        std::cout << "Connected to " << zportal::to_string(*cfg.connect_address) << std::endl;
    }

    tun_device->set_up();

    auto session =
        zportal::Session::create_session(std::move(*ring), std::move(*tun_device), std::move(socket), 4096, 4096, 4096);
    if (!session) {
        std::cerr << session.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    const auto run_result = session->run();
    if (!run_result) {
        std::cerr << run_result.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
