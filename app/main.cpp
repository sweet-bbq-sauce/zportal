#include <iostream>

#include <cstdlib>

#include <zportal/iouring/iouring.hpp>
#include <zportal/net/address.hpp>
#include <zportal/net/connection.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/session/transmitter.hpp>

// Use with 'nc 127.0.0.1 3333 > packets.bin'
int main(int argn, char* argv[]) {
    const zportal::HostPair host{.hostname = "127.0.0.1", .port = 3333};
    const auto listener = zportal::create_listener(host);
    if (!listener) {
        std::cerr << listener.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    auto ring = zportal::IoUring::create_queue(8);
    if (!ring) {
        std::cerr << ring.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    zportal::Cidr cidr(zportal::SockAddress::ip4_numeric("10.0.1.1", 0), 24);
    auto tun = zportal::TunDevice::create_tun_device("zp0", cidr, 1000);
    if (!tun) {
        std::cerr << tun.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    tun->set_up();

    auto client = zportal::accept_from(*listener);
    if (!client) {
        std::cerr << client.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Client connected" << std::endl;

    auto transmitter = zportal::Transmitter::create_transmitter(*ring, *tun, *client, 16);
    if (!transmitter) {
        std::cerr << transmitter.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    if (const auto result = transmitter->arm_read(); !result) {
        std::cerr << result.error().to_string() << std::endl;
        return EXIT_FAILURE;
    }

    while (true) {
        auto cqe = ring->wait();
        if (!cqe) {
            std::cerr << cqe.error().to_string() << std::endl;
            return EXIT_FAILURE;
        }
        if (const auto result = transmitter->handle_cqe(*cqe); !result) {
            std::cerr << result.error().to_string() << std::endl;
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
