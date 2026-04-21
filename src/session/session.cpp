#include <chrono>
#include <utility>

#include <zportal/net/socket.hpp>
#include <zportal/session/operation.hpp>
#include <zportal/session/receiver.hpp>
#include <zportal/session/session.hpp>
#include <zportal/session/transmitter.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tools/monitor.hpp>

zportal::Result<zportal::Session>
zportal::Session::create_session(IoUring&& ring, TunDevice&& tun, Socket&& socket, std::size_t tx_queue_length,
                                 std::size_t rx_queue_length, std::size_t rx_buffer_size, const Config& cfg) noexcept {
    Session session;
    session.ring_ = std::move(ring);
    session.tun_ = std::move(tun);
    session.socket_ = std::move(socket);
    session.cfg_ = &cfg;

    auto receiver =
        Receiver::create_receiver(session.ring_, session.tun_, session.socket_, rx_queue_length, rx_buffer_size);
    if (!receiver)
        return fail(receiver.error());
    session.receiver_ = std::move(*receiver);

    auto transmitter = Transmitter::create_transmitter(session.ring_, session.tun_, session.socket_, tx_queue_length);
    if (!transmitter)
        return fail(transmitter.error());
    session.transmitter_ = std::move(*transmitter);

    return session;
}

zportal::Session::Session(Session&& other) noexcept
    : ring_(std::move(other.ring_)), tun_(std::move(other.tun_)), socket_(std::move(other.socket_)),
      receiver_(std::move(other.receiver_)), transmitter_(std::move(other.transmitter_)),
      cfg_(std::exchange(other.cfg_, nullptr)) {
    receiver_.ring_ = &ring_;
    receiver_.tun_ = &tun_;
    receiver_.socket_ = &socket_;
    transmitter_.ring_ = &ring_;
    transmitter_.tun_ = &tun_;
    transmitter_.sock_ = &socket_;
}

zportal::Session& zportal::Session::operator=(Session&& other) noexcept {
    if (&other == this)
        return *this;

    ring_ = std::move(other.ring_);
    tun_ = std::move(other.tun_);
    socket_ = std::move(other.socket_);
    receiver_ = std::move(other.receiver_);
    transmitter_ = std::move(other.transmitter_);
    cfg_ = std::exchange(other.cfg_, nullptr);

    receiver_.ring_ = &ring_;
    receiver_.tun_ = &tun_;
    receiver_.socket_ = &socket_;
    transmitter_.ring_ = &ring_;
    transmitter_.tun_ = &tun_;
    transmitter_.sock_ = &socket_;

    return *this;
}

zportal::Result<void> zportal::Session::run() noexcept {
    if (const auto arm_recv_result = receiver_.arm_recv(); !arm_recv_result)
        return fail(arm_recv_result.error());

    if (const auto arm_read_result = transmitter_.arm_read(); !arm_read_result)
        return fail(arm_read_result.error());

    Monitor::set_tun_device(tun_);
    if (const auto first_print_result = Monitor::print(); !first_print_result)
        return fail(first_print_result.error());

    if (cfg_->monitor_mode) {
        if (const auto arm_timeout_result = Monitor::arm_timeout(ring_, std::chrono::milliseconds(1000));
            !arm_timeout_result)
            return fail(arm_timeout_result.error());
    }

    for (;;) {
        const auto cqe = ring_.wait();
        if (!cqe)
            return fail(cqe.error());

        const auto type = cqe->operation().get_type();
        if (type == OperationType::NONE)
            continue;
        else if (type == OperationType::READ || type == OperationType::SEND) {
            if (const auto handle_cqe_result = transmitter_.handle_cqe(*cqe); !handle_cqe_result)
                return fail(handle_cqe_result.error());
        } else if (type == OperationType::RECV || type == OperationType::WRITE) {
            if (const auto handle_cqe_result = receiver_.handle_cqe(*cqe); !handle_cqe_result)
                return fail(handle_cqe_result.error());
        } else if (type == OperationType::TIMEOUT && cfg_->monitor_mode) {
            if (const auto handle_cqe_result = Monitor::handle_cqe(ring_, *cqe); !handle_cqe_result)
                return fail(handle_cqe_result.error());
        } else
            return fail(ErrorCode::InvalidEnumValue);
    }

    return {};
}