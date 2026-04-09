#include "zportal/session/operation.hpp"
#include "zportal/tools/error.hpp"
#include <cerrno>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <zportal/session/receiver.hpp>

zportal::Result<zportal::Receiver> zportal::Receiver::create_receiver(IoUring& ring, TunDevice& tun, Socket& socket,
                                                                      std::size_t queue_length) noexcept {
    Receiver receiver;
    receiver.ring_ = &ring;
    receiver.tun_ = &tun;
    receiver.socket_ = &socket;

    auto bg = receiver.ring_->create_buffer_group(queue_length, receiver.tun_->get_mtu());
    if (!bg)
        return Fail(bg.error());
    receiver.bg_ = *bg;

    return receiver;
}

zportal::Result<void> zportal::Receiver::arm_recv() noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidReceiver);

    auto sqe = ring_->get_sqe();
    if (!sqe)
        return Fail(sqe.error());

    Operation operation;
    operation.set_type(OperationType::RECV);

    ::io_uring_prep_recv_multishot(*sqe, socket_->get(), nullptr, 0, 0);
    ::io_uring_sqe_set_flags(*sqe, IOSQE_BUFFER_SELECT);
    (*sqe)->buf_group = bg_->get_bgid();
    ::io_uring_sqe_set_data64(*sqe, operation.serialize());

    const auto submit_result = ring_->submit();
    if (!submit_result)
        return Fail(submit_result.error());

    return {};
}

zportal::Result<void> zportal::Receiver::handle_cqe(const Cqe& cqe) noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidReceiver);

    const auto type = cqe.operation().get_type();
    if (type != OperationType::RECV && type != OperationType::WRITE)
        return Fail(ErrorCode::WrongOperationType);

    if (type == OperationType::RECV)
        return handle_recv_cqe_(cqe);
    else
        return handle_write_cqe_(cqe);
}

bool zportal::Receiver::is_valid() const noexcept {
    return ring_ && tun_ && socket_ && bg_;
}

zportal::Receiver::operator bool() const noexcept {
    return is_valid();
}

zportal::Result<void> zportal::Receiver::handle_write_cqe_(const Cqe& cqe) noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidReceiver);

    if (cqe.operation().get_type() != OperationType::WRITE)
        return Fail(ErrorCode::WrongOperationType);

    return {};
}

zportal::Result<void> zportal::Receiver::handle_recv_cqe_(const Cqe& cqe) noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidReceiver);

    if (cqe.operation().get_type() != OperationType::RECV)
        return Fail(ErrorCode::WrongOperationType);

    if (!cqe.ok())
        return Fail({ErrorCode::RecvFailed, cqe.error()});

    const std::uint32_t readen = cqe.result();
    if (readen == 0)
        return Fail(ErrorCode::PeerClosed);

    const auto bid = cqe.bid();
    if (!bid)
        return Fail(ErrorCode::RecvCqeMissingBid);

    segment_queue_.push({.bid = *bid, .size = readen});

    return {};
}