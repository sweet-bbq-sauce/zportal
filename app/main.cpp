#include <exception>
#include <iostream>

#include <csignal>
#include <cstdlib>

#include <stdexcept>
#include <zportal/net/connection.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tunnel/tunnel.hpp>

static zportal::Tunnel* tunnel_ptr = nullptr;

extern "C" void on_interrupt(int) {
    if (tunnel_ptr)
        tunnel_ptr->close();
}

int main(int argn, char* argv[]) {
    std::signal(SIGINT, on_interrupt);

    zportal::Config cfg{};
    bool end{};
    std::exception_ptr result = zportal::parse_cli_arguments(cfg, argn, argv, end);
    if (result) {
        try {
            std::rethrow_exception(result);
        } catch (const std::invalid_argument& e) {
            std::cerr << "Invalid CLI argument: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception in CLI parsing: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in CLI parsing" << std::endl;
        }

        return EXIT_FAILURE;
    }

    if (end)
        return EXIT_SUCCESS;

    zportal::TUNInterface tun(cfg.interface_name);
    tun.set_cidr(cfg.inner_address);
    tun.set_mtu(cfg.mtu);
    tun.set_up();

    zportal::Socket sock;
    if (cfg.connect_address) {
        sock = zportal::connect_to(*cfg.connect_address, cfg.proxies);
        std::cout << "Connected" << std::endl;
    } else {
        sock = zportal::accept_from(zportal::create_listener(*cfg.bind_address));
        std::cout << "Accepted" << std::endl;
    }

    zportal::IOUring ring(1024);
    zportal::Tunnel tunnel(std::move(ring), std::move(tun), std::move(sock), cfg);
    tunnel_ptr = &tunnel;

    const auto run_result = tunnel.run();
    if (run_result) {
        std::cout << run_result.to_string() << std::endl;
    }

    std::cout << "Exiting ..." << std::endl;
    return result ? EXIT_FAILURE : EXIT_SUCCESS;
}
