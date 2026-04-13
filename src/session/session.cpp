#include "zportal/net/socket.hpp"
#include <utility>

#include <zportal/session/operation.hpp>
#include <zportal/session/receiver.hpp>
#include <zportal/session/session.hpp>
#include <zportal/session/transmitter.hpp>
#include <zportal/tools/error.hpp>

zportal::Result<zportal::Session> zportal::Session::create_session(IoUring&& ring, TunDevice&& tun, Socket&& socket,
                                                                   std::size_t tx_queue_length,
                                                                   std::size_t rx_queue_length,
                                                                   std::size_t rx_buffer_size) noexcept {
    Session session;
    session.ring_ = std::move(ring);
    session.tun_ = std::move(tun);
    session.socket_ = std::move(socket);

    auto receiver =
        Receiver::create_receiver(session.ring_, session.tun_, session.socket_, rx_queue_length, rx_buffer_size);
    if (!receiver)
        return Fail(receiver.error());
    session.receiver_ = std::move(*receiver);

    auto transmitter = Transmitter::create_transmitter(session.ring_, session.tun_, session.socket_, tx_queue_length);
    if (!transmitter)
        return Fail(transmitter.error());
    session.transmitter_ = std::move(*transmitter);

    return session;
}

zportal::Session::Session(Session&& other) noexcept
    : ring_(std::move(other.ring_)), tun_(std::move(other.tun_)), socket_(std::move(other.socket_)),
      receiver_(std::move(other.receiver_)), transmitter_(std::move(other.transmitter_)) {}

zportal::Session& zportal::Session::operator=(Session&& other) noexcept {
    if (&other == this)
        return *this;

    ring_ = std::move(other.ring_);
    tun_ = std::move(other.tun_);
    socket_ = std::move(other.socket_);
    receiver_ = std::move(other.receiver_);
    transmitter_ = std::move(other.transmitter_);

    return *this;
}

zportal::Result<void> zportal::Session::run() noexcept {
    if (const auto arm_recv_result = receiver_.arm_recv(); !arm_recv_result)
        return Fail(arm_recv_result.error());

    if (const auto arm_read_result = transmitter_.arm_read(); !arm_read_result)
        return Fail(arm_read_result.error());

    for (;;) {
        const auto cqe = ring_.wait();
        if (!cqe)
            return Fail(cqe.error());

        const auto type = cqe->operation().get_type();
        if (type == OperationType::NONE)
            continue;
        else if (type == OperationType::READ || type == OperationType::SEND) {
            if (const auto handle_cqe_result = transmitter_.handle_cqe(*cqe); !handle_cqe_result)
                return Fail(handle_cqe_result.error());
        } else if (type == OperationType::RECV || type == OperationType::WRITE) {
            if (const auto handle_cqe_result = receiver_.handle_cqe(*cqe); !handle_cqe_result)
                return Fail(handle_cqe_result.error());
        } else
            return Fail(ErrorCode::InvalidEnumValue);
    }

    return {};
}