#include "zportal/iouring/ring.hpp"
#include <iostream>

#include <cstdlib>

#include <zportal/net/connection.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tunnel/tunnel.hpp>

int main(int argn, char* argv[]) {
    zportal::Config cfg{};
    zportal::parse_arguments(cfg, argn, argv);

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
    zportal::Tunnel tunnel(std::move(ring), std::move(tun), std::move(sock));

    tunnel.wait();

    return EXIT_SUCCESS;
}
