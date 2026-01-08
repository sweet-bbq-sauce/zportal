#include <iostream>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <endian.h>
#include <fcntl.h>
#include <liburing.h>
#include <unistd.h>

#include <zportal/socket.hpp>
#include <zportal/tun.hpp>
#include <zportal/tunnel.hpp>

zportal::Tunnel::Tunnel(io_uring* ring, Socket&& tcp, const TUNInterface* tun, bool* exited)
    : ring_(ring), tcp_(std::move(tcp)), tun_(tun), exited_(exited) {
    if (!ring)
        throw std::invalid_argument("ring is null");

    if (!tcp_)
        throw std::invalid_argument("invalid socket");

    if (!tun_)
        throw std::invalid_argument("TUN interface is null");

    if (!*tun_)
        throw std::invalid_argument("invalid TUN interface");

    const int flags = ::fcntl(tcp_.get_fd(), F_GETFL, 0);
    if (flags < 0)
        throw std::system_error(errno, std::system_category(), "fcntl F_GETFL");

    if (!(flags & O_NONBLOCK))
        if (::fcntl(tcp_.get_fd(), F_SETFL, flags | O_NONBLOCK) < 0)
            throw std::system_error(errno, std::system_category(), "fcntl F_SETFL");

    rx.resize(tun->get_mtu());
    tx.resize(tun->get_mtu());

    tcp_recv_header();
    tun_read();
}

zportal::Tunnel::Tunnel(Tunnel&& other) noexcept
    : ring_(std::exchange(other.ring_, nullptr)), tcp_(std::move(other.tcp_)), tun_(std::exchange(other.tun_, nullptr)),
      exited_(std::exchange(other.exited_, nullptr)) {}

zportal::Tunnel& zportal::Tunnel::operator=(Tunnel&& other) noexcept {
    if (this == &other)
        return *this;

    close();
    ring_ = std::exchange(other.ring_, nullptr);
    tcp_ = std::move(other.tcp_);
    tun_ = std::exchange(other.tun_, nullptr);
    exited_ = std::exchange(other.exited_, nullptr);

    return *this;
}

zportal::Tunnel::~Tunnel() noexcept {
    close();
}

void zportal::Tunnel::close() noexcept {
    tcp_.close();
}

static const auto get_sqe = [](io_uring* ring) {
    auto* sqe = ::io_uring_get_sqe(ring);
    if (!sqe)
        throw std::runtime_error("SQE is null");

    return sqe;
};

static const auto sqe_submit = [](io_uring* ring) {
    if (const int result = ::io_uring_submit(ring); result < 0)
        throw std::runtime_error(std::error_code{-result, std::system_category()}.message());
};

void zportal::Tunnel::tcp_recv_header() {
    auto* sqe = get_sqe(ring_);
    auto* operation = new Operation{};

    void* ptr = reinterpret_cast<std::byte*>(&rx_header) + rx_current_processed;
    const std::size_t size = sizeof(rx_header) - rx_current_processed;
    ::io_uring_prep_recv(sqe, tcp_.get_fd(), ptr, size, 0);

    operation->ring = ring_;
    operation->type = OperationType::TCP_RECV_HEADER;
    ::io_uring_sqe_set_data(sqe, operation);

    sqe_submit(ring_);
}

void zportal::Tunnel::tcp_recv() {
    auto* sqe = get_sqe(ring_);
    auto* operation = new Operation{};

    void* ptr = rx.data() + rx_current_processed;
    const std::size_t size = static_cast<std::size_t>(::be32toh(rx_header.size)) - rx_current_processed;
    ::io_uring_prep_recv(sqe, tcp_.get_fd(), ptr, size, 0);

    operation->ring = ring_;
    operation->type = OperationType::TCP_RECV;
    ::io_uring_sqe_set_data(sqe, operation);

    sqe_submit(ring_);
}

void zportal::Tunnel::tun_write() {
    auto* sqe = get_sqe(ring_);
    auto* operation = new Operation{};

    if (!tun_)
        throw std::logic_error("TUN interface is null");

    ::io_uring_prep_write(sqe, tun_->get_fd(), rx.data(), ::be32toh(rx_header.size), -1);

    operation->ring = ring_;
    operation->type = OperationType::TUN_WRITE;
    ::io_uring_sqe_set_data(sqe, operation);

    sqe_submit(ring_);
}

void zportal::Tunnel::tun_read() {
    auto* sqe = get_sqe(ring_);
    auto* operation = new Operation{};

    if (!tun_)
        throw std::logic_error("TUN interface is null");

    ::io_uring_prep_read(sqe, tun_->get_fd(), tx.data(), tx.size(), -1);

    operation->ring = ring_;
    operation->type = OperationType::TUN_READ;
    ::io_uring_sqe_set_data(sqe, operation);

    sqe_submit(ring_);
}

void zportal::Tunnel::tcp_send_header() {
    auto* sqe = get_sqe(ring_);
    auto* operation = new Operation{};

    const void* ptr = reinterpret_cast<std::byte*>(&tx_header) + tx_current_processed;
    const std::size_t size = sizeof(tx_header) - tx_current_processed;
    ::io_uring_prep_send(sqe, tcp_.get_fd(), ptr, size, 0);

    operation->ring = ring_;
    operation->type = OperationType::TCP_SEND_HEADER;
    ::io_uring_sqe_set_data(sqe, operation);

    sqe_submit(ring_);
}

void zportal::Tunnel::tcp_send() {
    auto* sqe = get_sqe(ring_);
    auto* operation = new Operation{};

    const void* ptr = tx.data() + tx_current_processed;
    const std::size_t size = static_cast<std::size_t>(::be32toh(tx_header.size)) - tx_current_processed;
    ::io_uring_prep_send(sqe, tcp_.get_fd(), ptr, size, 0);

    operation->ring = ring_;
    operation->type = OperationType::TCP_SEND;
    ::io_uring_sqe_set_data(sqe, operation);

    sqe_submit(ring_);
}

constexpr auto log_info = [](zportal::OperationType optype, int result) {
    std::string optype_string;
    switch (optype) {
    case zportal::OperationType::TCP_RECV_HEADER:
        optype_string = "TCP_RECV_HEADER";
        break;
    case zportal::OperationType::TCP_RECV:
        optype_string = "TCP_RECV";
        break;
    case zportal::OperationType::TUN_WRITE:
        optype_string = "TUN_WRITE";
        break;
    case zportal::OperationType::TUN_READ:
        optype_string = "TUN_READ";
        break;
    case zportal::OperationType::TCP_SEND_HEADER:
        optype_string = "TCP_SEND_HEADER";
        break;
    case zportal::OperationType::TCP_SEND:
        optype_string = "TCP_SEND";
        break;

    default:
        optype_string = "UNKNOWN";
    };

    std::cout << optype_string << ": " << std::error_code{-result, std::system_category()}.message() << std::endl;
};

void zportal::Tunnel::handle_cqe(io_uring_cqe* cqe) {
    if (!cqe)
        throw std::runtime_error("cqe is null");

    const auto* operation = reinterpret_cast<const Operation*>(::io_uring_cqe_get_data(cqe));
    const int result = cqe->res;
    const OperationType type = operation->type;

    if (result < 0)
        log_info(type, result);

    switch (type) {
    case zportal::OperationType::TCP_RECV_HEADER: {
        if (result < 0)
            tcp_recv_header();
        else if (result == 0) {
            std::cout << "TCP_RECV_HEADER: peer closed" << std::endl;
            *exited_ = true;
        } else {
            rx_current_processed += static_cast<std::size_t>(result);
            if (rx_current_processed == sizeof(rx_header)) {
                rx_current_processed = 0;
                const std::uint32_t size = ::be32toh(rx_header.size);
                if (size > rx.size()) {
                    std::cout << "TCP_RECV_HEADER: want to receive too big packet(" << size << "B), ignoring ..."
                              << std::endl;
                    tcp_recv_header();
                } else
                    tcp_recv();
            } else
                tcp_recv_header();
        }
    } break;
    case zportal::OperationType::TCP_RECV: {
        if (result < 0)
            tcp_recv();
        else if (result == 0) {
            std::cout << "TCP_RECV: peer closed" << std::endl;
            *exited_ = true;
        } else {
            rx_current_processed += static_cast<std::size_t>(result);
            if (rx_current_processed == ::be32toh(rx_header.size)) {
                rx_current_processed = 0;
                tun_write();
            } else
                tcp_recv();
        }
    } break;
    case zportal::OperationType::TUN_WRITE: {
        if (result < ::be32toh(rx_header.size))
            tun_write();
        else
            tcp_recv_header();
    } break;
    case zportal::OperationType::TUN_READ: {
        if (result < 0)
            tun_read();
        else if (result == 0) {
            std::cout << "TUN_READ: interfce " << tun_->get_name() << " is closed" << std::endl;
            *exited_ = true;
        } else {
            tx_header.size = ::htobe32(static_cast<std::uint32_t>(result));
            tcp_send_header();
        }
    }; break;
    case zportal::OperationType::TCP_SEND_HEADER: {
        if (result <= 0)
            tcp_send_header();
        else {
            tx_current_processed += static_cast<std::size_t>(result);
            if (tx_current_processed == sizeof(tx_header)) {
                tx_current_processed = 0;
                tcp_send();
            } else
                tcp_send_header();
        }
    } break;
    case zportal::OperationType::TCP_SEND: {
        if (result <= 0)
            tcp_send();
        else {
            tx_current_processed += static_cast<std::size_t>(result);
            if (tx_current_processed == ::be32toh(tx_header.size)) {
                tx_current_processed = 0;
                tun_read();
            } else
                tcp_send();
        }
    } break;
    };
}