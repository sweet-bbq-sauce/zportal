#include <atomic>
#include <cerrno>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <endian.h>
#include <fcntl.h>
#include <linux/time_types.h>
#include <unistd.h>

#include <liburing.h>

#include <zportal/config.hpp>
#include <zportal/crc.hpp>
#include <zportal/ring.hpp>
#include <zportal/socket.hpp>
#include <zportal/tun.hpp>
#include <zportal/tunnel.hpp>

zportal::Tunnel::Tunnel(zportal::IOUring& ring, Socket&& tcp, const TUNInterface* tun, bool* exited)
    : ring_(&ring), tcp_(std::move(tcp)), tun_(tun), exited_(exited) {
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
    wait_for_refresh();
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

void zportal::Tunnel::tcp_recv_header() {
    auto* sqe = ring_->get_sqe();
    auto* operation = new Operation{};

    void* ptr = reinterpret_cast<std::byte*>(&rx_header) + rx_current_processed;
    const std::size_t size = sizeof(rx_header) - rx_current_processed;
    ::io_uring_prep_recv(sqe, tcp_.get_fd(), ptr, size, 0);

    operation->type = OperationType::TCP_RECV_HEADER;
    ::io_uring_sqe_set_data(sqe, operation);

    ring_->submit();
}

void zportal::Tunnel::tcp_recv() {
    auto* sqe = ring_->get_sqe();
    auto* operation = new Operation{};

    void* ptr = rx.data() + rx_current_processed;
    const std::size_t size = static_cast<std::size_t>(::be32toh(rx_header.size)) - rx_current_processed;
    ::io_uring_prep_recv(sqe, tcp_.get_fd(), ptr, size, 0);

    operation->type = OperationType::TCP_RECV;
    ::io_uring_sqe_set_data(sqe, operation);

    ring_->submit();
}

void zportal::Tunnel::tun_write() {
    auto* sqe = ring_->get_sqe();
    auto* operation = new Operation{};

    if (!tun_)
        throw std::logic_error("TUN interface is null");

    ::io_uring_prep_write(sqe, tun_->get_fd(), rx.data(), ::be32toh(rx_header.size), -1);

    operation->type = OperationType::TUN_WRITE;
    ::io_uring_sqe_set_data(sqe, operation);

    ring_->submit();
}

void zportal::Tunnel::tun_read() {
    auto* sqe = ring_->get_sqe();
    auto* operation = new Operation{};

    if (!tun_)
        throw std::logic_error("TUN interface is null");

    ::io_uring_prep_read(sqe, tun_->get_fd(), tx.data(), tx.size(), -1);

    operation->type = OperationType::TUN_READ;
    ::io_uring_sqe_set_data(sqe, operation);

    ring_->submit();
}

void zportal::Tunnel::tcp_send_header() {
    auto* sqe = ring_->get_sqe();
    auto* operation = new Operation{};

    const void* ptr = reinterpret_cast<std::byte*>(&tx_header) + tx_current_processed;
    const std::size_t size = sizeof(tx_header) - tx_current_processed;
    ::io_uring_prep_send(sqe, tcp_.get_fd(), ptr, size, 0);

    operation->type = OperationType::TCP_SEND_HEADER;
    ::io_uring_sqe_set_data(sqe, operation);

    ring_->submit();
}

void zportal::Tunnel::tcp_send() {
    auto* sqe = ring_->get_sqe();
    auto* operation = new Operation{};

    const void* ptr = tx.data() + tx_current_processed;
    const std::size_t size = static_cast<std::size_t>(::be32toh(tx_header.size)) - tx_current_processed;
    ::io_uring_prep_send(sqe, tcp_.get_fd(), ptr, size, 0);

    operation->type = OperationType::TCP_SEND;
    ::io_uring_sqe_set_data(sqe, operation);

    ring_->submit();
}

void zportal::Tunnel::wait_for_refresh() {
    auto* sqe = ring_->get_sqe();
    auto* operation = new Operation{};

    __kernel_timespec ts{};
    ts.tv_sec = 1;
    ::io_uring_prep_timeout(sqe, &ts, 0, 0);

    operation->type = OperationType::MONITOR_REFRESH;
    ::io_uring_sqe_set_data(sqe, operation);

    ring_->submit();
}

void zportal::Tunnel::refresh_monitor() {
    if (!monitor_mode.load(std::memory_order_relaxed))
        return;

    std::ostringstream oss;
    oss << "\r\x1b[2K" << "RX: " << rx_total << " B | TX: " << tx_total << " B";
    const std::string line = oss.str();
    ::write(STDERR_FILENO, line.data(), line.size());
}

constexpr auto operation_string(zportal::OperationType optype) {
    std::string optype_string;
    switch (optype) {
    case zportal::OperationType::TCP_RECV_HEADER:
        optype_string = "PEER_RCV_HDR";
        break;
    case zportal::OperationType::TCP_RECV:
        optype_string = "PEER_RCV";
        break;
    case zportal::OperationType::TUN_WRITE:
        optype_string = "TUN_WRITE";
        break;
    case zportal::OperationType::TUN_READ:
        optype_string = "TUN_READ";
        break;
    case zportal::OperationType::TCP_SEND_HEADER:
        optype_string = "PEER_SND_HDR";
        break;
    case zportal::OperationType::TCP_SEND:
        optype_string = "PEER_SND";
        break;

    case zportal::OperationType::MONITOR_REFRESH:
        optype_string = "MONITOR_REFRESH";
        break;

    default:
        optype_string = "UNKNOWN";
    };

    return optype_string;
};

constexpr auto verbose_info = [](const zportal::Operation* operation, const io_uring_cqe* cqe) {
    if (!operation)
        return;

    if (!zportal::verbose_mode.load(std::memory_order_relaxed))
        return;

    const int result = cqe->res;

    if (operation->type == zportal::OperationType::MONITOR_REFRESH && result == -ETIME)
        return;

    std::cout << "\r" << operation_string(operation->type) << ":\t";
    switch (operation->type) {
    case zportal::OperationType::TCP_RECV_HEADER:
    case zportal::OperationType::TCP_RECV: {
        if (result == 0)
            std::cout << "Peer closed";
        else if (result > 0)
            std::cout << "Received " << result << "B.";
    } break;
    case zportal::OperationType::TUN_WRITE:
        if (result > 0)
            std::cout << "Writed " << result << "B packet to TUN";
        break;
    case zportal::OperationType::TUN_READ:
        if (result > 0)
            std::cout << "Readen " << result << "B packet from TUN";
        break;
    case zportal::OperationType::TCP_SEND_HEADER:
    case zportal::OperationType::TCP_SEND:
        if (result > 0)
            std::cout << "Sent " << result << "B.";
        break;

    default:
        std::cout << "Result: " << result;
    }

    std::cout << std::endl;
};

void zportal::Tunnel::handle_cqe(io_uring_cqe* cqe) {
    if (!cqe)
        throw std::runtime_error("cqe is null");

    const auto* operation = reinterpret_cast<const Operation*>(::io_uring_cqe_get_data(cqe));
    const int result = cqe->res;
    const OperationType type = operation->type;

    verbose_info(operation, cqe);

    switch (type) {
    case zportal::OperationType::TCP_RECV_HEADER: {
        if (result < 0)
            tcp_recv_header();
        else if (result == 0)
            *exited_ = true;
        else {
            rx_current_processed += static_cast<std::size_t>(result);
            rx_total += static_cast<std::uintmax_t>(result);
            if (rx_current_processed == sizeof(rx_header)) {
                rx_current_processed = 0;
                if (rx_header.magic != ::htobe32(zportal::magic))
                    throw std::runtime_error("connection desynchronized");

                const std::uint32_t size = ::be32toh(rx_header.size);
                if (size > rx.size())
                    tcp_recv_header();
                else
                    tcp_recv();
            } else
                tcp_recv_header();
        }
    } break;
    case zportal::OperationType::TCP_RECV: {
        if (result < 0)
            tcp_recv();
        else if (result == 0)
            *exited_ = true;
        else {
            rx_current_processed += static_cast<std::size_t>(result);
            rx_total += static_cast<std::uintmax_t>(result);
            if (rx_current_processed == ::be32toh(rx_header.size)) {
                rx_current_processed = 0;

                if (crc32c({rx.data(), ::be32toh(rx_header.size)}) != ::be32toh(rx_header.crc))
                    tcp_recv_header();
                else
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
        else if (result == 0)
            *exited_ = true;
        else {
            tx_header.magic = ::htobe32(zportal::magic);
            tx_header.size = ::htobe32(static_cast<std::uint32_t>(result));
            tx_header.crc = ::htobe32(crc32c({tx.data(), ::be32toh(tx_header.size)}));
            tcp_send_header();
        }
    }; break;
    case zportal::OperationType::TCP_SEND_HEADER: {
        if (result <= 0)
            tcp_send_header();
        else {
            tx_current_processed += static_cast<std::size_t>(result);
            tx_total += static_cast<std::uintmax_t>(result);
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
            tx_total += static_cast<std::uintmax_t>(result);
            if (tx_current_processed == ::be32toh(tx_header.size)) {
                tx_current_processed = 0;
                tun_read();
            } else
                tcp_send();
        }
    } break;

    case zportal::OperationType::MONITOR_REFRESH: {
        if (result == -ETIME) {
            refresh_monitor();
            wait_for_refresh();
        } else if (result == -ECANCELED)
            wait_for_refresh();
        else if (result < 0)
            wait_for_refresh();
        else
            wait_for_refresh();
    } break;
    };
}