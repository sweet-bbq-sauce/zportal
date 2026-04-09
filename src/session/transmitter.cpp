#include <new>

#include <cerrno>

#include <liburing.h>
#include <sys/socket.h>

#include <zportal/iouring/iouring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/session/frame_header.hpp>
#include <zportal/session/operation.hpp>
#include <zportal/session/transmitter.hpp>
#include <zportal/tools/crc.hpp>
#include <zportal/tools/error.hpp>

zportal::Result<zportal::Transmitter> zportal::Transmitter::create_transmitter(zportal::IoUring& ring,
                                                                               zportal::TunDevice& tun,
                                                                               zportal::Socket& sock,
                                                                               std::uint16_t queue_length) noexcept {
    Transmitter transmitter;
    transmitter.ring_ = &ring;
    transmitter.tun_ = &tun;
    transmitter.sock_ = &sock;

    auto bg = transmitter.ring_->create_buffer_group(queue_length, transmitter.tun_->get_mtu());
    if (!bg)
        return Fail(bg.error());
    transmitter.bg_ = *bg;

    return transmitter;
}

zportal::Result<void> zportal::Transmitter::arm_read() noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidTransmitter);

    auto sqe = ring_->get_sqe();
    if (!sqe)
        return Fail(sqe.error());

    Operation op;
    op.set_type(OperationType::READ);

    ::io_uring_prep_read_multishot(*sqe, tun_->get_fd(), 0, 0, bg_->get_bgid());
    ::io_uring_sqe_set_data64(*sqe, op.serialize());

    const auto submit_result = ring_->submit();
    if (!submit_result)
        return Fail(submit_result.error());

    return {};
}

zportal::Result<void> zportal::Transmitter::handle_cqe(const Cqe& cqe) noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidTransmitter);

    const auto type = cqe.operation().get_type();
    if (type != OperationType::SEND && type != OperationType::READ)
        return Fail(ErrorCode::WrongOperationType);

    if (type == OperationType::SEND)
        return handle_send_cqe_(cqe);
    else
        return handle_read_cqe_(cqe);
}

bool zportal::Transmitter::is_valid() const noexcept {
    return ring_ && tun_ && sock_ && bg_;
}

zportal::Transmitter::operator bool() const noexcept {
    return is_valid();
}

zportal::Result<void> zportal::Transmitter::handle_read_cqe_(const Cqe& cqe) noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidTransmitter);

    if (cqe.operation().get_type() != OperationType::READ)
        return Fail(ErrorCode::WrongOperationType);

    if (!cqe.ok()) {
        if (cqe.error() == ENOBUFS && !cqe.more()) {
            cooling_down_ = true;
            return kick_send_();
        }

        return Fail({ErrorCode::TunReadFailed, cqe.error()});
    }

    const auto bid = cqe.bid();
    if (!bid)
        return Fail(ErrorCode::ReadCqeMissingBid);
    
    const std::uint32_t readen = static_cast<std::size_t>(cqe.result());

    OutFrame out_frame{.bid = *bid, .size = readen};

    try {
        frame_queue_.push(out_frame);
    } catch (const std::bad_alloc&) {
        const auto result /*unused*/ = bg_->return_buffer(*bid);
        return Fail(ErrorCode::NotEnoughMemory);
    }

    return kick_send_();
}

zportal::Result<zportal::FrameHeader> zportal::Transmitter::create_frame_header_(const OutFrame& frame) noexcept {
    const auto buffer = bg_->get_buffer(frame.bid, frame.size);
    if (!buffer)
        return Fail(buffer.error());

    FrameHeader header;
    header.set_size(frame.size);
    header.set_crc(crc32c(*buffer));

    return header;
}

zportal::Result<void> zportal::Transmitter::kick_send_() noexcept {
    if (send_in_progress_)
        return {};

    if (frame_queue_.empty())
        return {};

    if (!current_frame_state_) {
        const auto frame = frame_queue_.front();
        const auto header = create_frame_header_(frame);
        if (!header)
            return Fail(header.error());

        CurrentFrameState state;
        state.frame = frame;
        state.header = *header;
        try {
            state.segments.reserve(2);
        } catch (const std::bad_alloc&) {
            return Fail(ErrorCode::NotEnoughMemory);
        }

        current_frame_state_ = state;
    }

    auto& state = *current_frame_state_;
    const auto payload = bg_->get_buffer(state.frame.bid, state.frame.size);
    if (!payload)
        return Fail(payload.error());

    if (state.bytes_sent >= FrameHeader::wire_size + static_cast<std::size_t>(state.frame.size))
        return Fail(ErrorCode::InvalidState);

    state.segments.clear();
    if (state.bytes_sent < FrameHeader::wire_size) {
        state.segments.push_back({.iov_base = state.header.data().data() + state.bytes_sent,
                                  .iov_len = FrameHeader::wire_size - state.bytes_sent});
        state.segments.push_back({.iov_base = payload->data(), .iov_len = payload->size()});
    } else
        state.segments.push_back({.iov_base = payload->data() + (state.bytes_sent - FrameHeader::wire_size),
                                  .iov_len = payload->size() - (state.bytes_sent - FrameHeader::wire_size)});

    state.message_header = msghdr{};
    state.message_header.msg_iov = state.segments.data();
    state.message_header.msg_iovlen = state.segments.size();

    auto sqe = ring_->get_sqe();
    if (!sqe)
        return Fail(sqe.error());

    Operation operation;
    operation.set_type(OperationType::SEND);

    ::io_uring_prep_sendmsg(*sqe, sock_->get(), &state.message_header, MSG_NOSIGNAL);
    ::io_uring_sqe_set_data64(*sqe, operation.serialize());

    const auto submit_result = ring_->submit();
    if (!submit_result)
        return Fail(submit_result.error());

    send_in_progress_ = true;

    return {};
}

zportal::Result<void> zportal::Transmitter::handle_send_cqe_(const Cqe& cqe) noexcept {
    if (!is_valid())
        return Fail(ErrorCode::InvalidTransmitter);

    if (cqe.operation().get_type() != OperationType::SEND)
        return Fail(ErrorCode::WrongOperationType);

    send_in_progress_ = false;

    if (!cqe.ok())
        return Fail({ErrorCode::SendFailed, cqe.error()});

    const std::size_t sent = static_cast<std::size_t>(cqe.result());

    if (sent == 0)
        return Fail(ErrorCode::SendReturnedZero);

    if (!current_frame_state_)
        return Fail(ErrorCode::InvalidState);

    auto& state = *current_frame_state_;

    const std::size_t total = FrameHeader::wire_size + static_cast<std::size_t>(state.frame.size);
    if (state.bytes_sent + sent > total)
        return Fail(ErrorCode::InvalidState);

    state.bytes_sent += sent;
    if (state.bytes_sent == total) {
        const auto bid = state.frame.bid;
        current_frame_state_ = std::nullopt;
        frame_queue_.pop();

        if (const auto result = bg_->return_buffer(bid); !result)
            return Fail(result.error());
    }

    if (cooling_down_) {
        if (frame_queue_.size() <= bg_->get_buffer_count() / 2) {
            if (const auto result = arm_read(); !result)
                return Fail(result.error());

            cooling_down_ = false;
        }
    }

    return kick_send_();
}