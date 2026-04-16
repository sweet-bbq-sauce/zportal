#include <iostream>
#include <new>
#include <utility>

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
#include <zportal/tools/support_check.hpp>

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
        return fail(bg.error());
    transmitter.bg_ = *bg;

    return transmitter;
}

zportal::Transmitter::Transmitter(Transmitter&& other) noexcept
    : ring_(std::exchange(other.ring_, nullptr)), tun_(std::exchange(other.tun_, nullptr)),
      bg_(std::exchange(other.bg_, nullptr)), sock_(std::exchange(other.sock_, nullptr)),
      frame_queue_(std::move(other.frame_queue_)), cooling_down_(std::exchange(other.cooling_down_, false)),
      send_in_progress_(std::exchange(other.send_in_progress_, false)),
      current_frame_state_(std::exchange(other.current_frame_state_, std::nullopt)) {}

zportal::Transmitter& zportal::Transmitter::operator=(Transmitter&& other) noexcept {
    if (&other == this)
        return *this;

    ring_ = std::exchange(other.ring_, nullptr);
    tun_ = std::exchange(other.tun_, nullptr);
    bg_ = std::exchange(other.bg_, nullptr);
    sock_ = std::exchange(other.sock_, nullptr);
    frame_queue_ = std::move(other.frame_queue_);
    cooling_down_ = std::exchange(other.cooling_down_, false);
    send_in_progress_ = std::exchange(other.send_in_progress_, false);
    current_frame_state_ = std::exchange(other.current_frame_state_, std::nullopt);

    return *this;
}

zportal::Result<void> zportal::Transmitter::arm_read() noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidTransmitter);

    auto sqe = ring_->get_sqe();
    if (!sqe)
        return fail(sqe.error());

    Operation op;
    op.set_type(OperationType::READ);
    ::io_uring_sqe_set_data64(*sqe, op.serialize());

#if HAVE_IO_URING_PREP_READ_MULTISHOT
    const auto check_result = support_check::read_multishot();
    if (!check_result)
        return fail(check_result.error());

    if (*check_result)
        ::io_uring_prep_read_multishot(*sqe, tun_->get_fd(), 0, 0, bg_->get_bgid());
    else {
        ::io_uring_prep_read(*sqe, tun_->get_fd(), nullptr, 0, 0);
        (*sqe)->flags |= IOSQE_BUFFER_SELECT;
        (*sqe)->buf_group = bg_->get_bgid();
    }
#else
    ::io_uring_prep_read(*sqe, tun_->get_fd(), nullptr, 0, 0);
    (*sqe)->flags |= IOSQE_BUFFER_SELECT;
    (*sqe)->buf_group = bg_->get_bgid();
#endif

    const auto submit_result = ring_->submit();
    if (!submit_result)
        return fail(submit_result.error());

    return {};
}

zportal::Result<void> zportal::Transmitter::handle_cqe(const Cqe& cqe) noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidTransmitter);

    const auto type = cqe.operation().get_type();
    if (type != OperationType::SEND && type != OperationType::READ)
        return fail(ErrorCode::WrongOperationType);

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
        return fail(ErrorCode::InvalidTransmitter);

    if (cqe.operation().get_type() != OperationType::READ)
        return fail(ErrorCode::WrongOperationType);

    if (!cqe.ok()) {
        if (cqe.error() == ENOBUFS && !cqe.more()) {
            cooling_down_ = true;
            std::cout << "TX backpressure: HIGH watermark, stopping READ" << std::endl;
            return kick_send_();
        }

        return fail({ErrorCode::TunReadFailed, cqe.error()});
    }

    const auto bid = cqe.bid();
    if (!bid)
        return fail(ErrorCode::ReadCqeMissingBid);

    const std::uint32_t readen = static_cast<std::size_t>(cqe.result());

    OutFrame out_frame{.bid = *bid, .size = readen};

    try {
        frame_queue_.push(out_frame);
    } catch (const std::bad_alloc&) {
        const auto result = bg_->return_buffer(*bid);
        (void)result;

        return fail(ErrorCode::NotEnoughMemory);
    }

    if (!cqe.more() && !cooling_down_) {
        if (const auto arm_read_result = arm_read(); !arm_read_result)
            return fail(arm_read_result.error());
    }

    return kick_send_();
}

zportal::Result<zportal::FrameHeader> zportal::Transmitter::create_frame_header_(const OutFrame& frame) noexcept {
    const auto buffer = bg_->get_buffer(frame.bid, frame.size);
    if (!buffer)
        return fail(buffer.error());

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
            return fail(header.error());

        CurrentFrameState state;
        state.frame = frame;
        state.header = *header;
        try {
            state.segments.reserve(2);
        } catch (const std::bad_alloc&) {
            return fail(ErrorCode::NotEnoughMemory);
        }

        current_frame_state_ = state;
    }

    auto& state = *current_frame_state_;
    const auto payload = bg_->get_buffer(state.frame.bid, state.frame.size);
    if (!payload)
        return fail(payload.error());

    if (state.bytes_sent >= FrameHeader::wire_size + static_cast<std::size_t>(state.frame.size))
        return fail(ErrorCode::InvalidState);

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
        return fail(sqe.error());

    Operation operation;
    operation.set_type(OperationType::SEND);

    ::io_uring_prep_sendmsg(*sqe, sock_->get(), &state.message_header, MSG_NOSIGNAL);
    ::io_uring_sqe_set_data64(*sqe, operation.serialize());

    if (const auto submit_result = ring_->submit(); !submit_result)
        return fail(submit_result.error());

    send_in_progress_ = true;

    return {};
}

zportal::Result<void> zportal::Transmitter::handle_send_cqe_(const Cqe& cqe) noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidTransmitter);

    if (cqe.operation().get_type() != OperationType::SEND)
        return fail(ErrorCode::WrongOperationType);

    send_in_progress_ = false;

    if (!cqe.ok())
        return fail({ErrorCode::SendFailed, cqe.error()});

    const std::size_t sent = static_cast<std::size_t>(cqe.result());

    if (sent == 0)
        return fail(ErrorCode::SendReturnedZero);

    if (!current_frame_state_)
        return fail(ErrorCode::InvalidState);

    auto& state = *current_frame_state_;

    const std::size_t total = FrameHeader::wire_size + static_cast<std::size_t>(state.frame.size);
    if (state.bytes_sent + sent > total)
        return fail(ErrorCode::InvalidState);

    state.bytes_sent += sent;
    if (state.bytes_sent == total) {
        const auto bid = state.frame.bid;
        current_frame_state_ = std::nullopt;
        frame_queue_.pop();

        if (const auto result = bg_->return_buffer(bid); !result)
            return fail(result.error());
    }

    if (cooling_down_) {
        if (frame_queue_.size() <= bg_->get_buffer_count() / 2) {
            if (const auto result = arm_read(); !result)
                return fail(result.error());

            cooling_down_ = false;
            std::cout << "TX backpressure: LOW watermark, rearming READ" << std::endl;
        }
    }

    return kick_send_();
}