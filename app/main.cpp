#include <exception>
#include <iostream>
#include <stdexcept>

#include <cstdlib>

#include <zportal/net/connection.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tunnel/tunnel.hpp>

[[noreturn]] void end_with_error(const zportal::Error& err) noexcept {
    std::cerr << "Error: " << err.to_string() << std::endl;
    std::exit(EXIT_FAILURE);
}

int main(int argn, char* argv[]) {
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
        auto connect_result = zportal::connect_to(*cfg.connect_address, cfg.proxies);
        if (!connect_result)
            end_with_error(connect_result.error());

        sock = std::move(*connect_result);
        std::cout << "Connected" << std::endl;
    } else {
        const auto listener_result = zportal::create_listener(*cfg.bind_address);
        if (!listener_result)
            end_with_error(listener_result.error());

        auto accept_result = zportal::accept_from(*listener_result);
        if (!accept_result)
            end_with_error(accept_result.error());

        sock = std::move(*accept_result);
        std::cout << "Accepted" << std::endl;
    }

    zportal::IOUring ring(1024);
    zportal::Tunnel tunnel(std::move(ring), std::move(tun), std::move(sock), cfg);

    const auto run_result = tunnel.run();
    if (!run_result)
        end_with_error(run_result.error());

    std::cout << "Exiting ..." << std::endl;
    return result ? EXIT_FAILURE : EXIT_SUCCESS;
}
