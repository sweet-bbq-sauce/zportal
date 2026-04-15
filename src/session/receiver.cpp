#include <iostream>
#include <limits>
#include <new>
#include <utility>

#include <cassert>
#include <cerrno>
#include <cstdint>

#include <liburing.h>

#include <zportal/session/frame_header.hpp>
#include <zportal/session/operation.hpp>
#include <zportal/session/receiver.hpp>
#include <zportal/tools/crc.hpp>
#include <zportal/tools/error.hpp>

zportal::Result<zportal::Receiver> zportal::Receiver::create_receiver(IoUring& ring, TunDevice& tun, Socket& socket,
                                                                      std::size_t queue_length,
                                                                      std::size_t buffer_size) noexcept {
    Receiver receiver;
    receiver.ring_ = &ring;
    receiver.tun_ = &tun;
    receiver.socket_ = &socket;

    if (buffer_size > std::numeric_limits<std::uint32_t>::max())
        return fail(ErrorCode::InvalidArgument);

    auto bg = receiver.ring_->create_buffer_group(queue_length, static_cast<std::uint32_t>(buffer_size));
    if (!bg)
        return fail(bg.error());
    receiver.bg_ = *bg;

    try {
        receiver.buffer_refcounts_.resize(queue_length, 0);
    } catch (const std::bad_alloc&) {
        // TODO: delete buffer group
        return fail(ErrorCode::NotEnoughMemory);
    }

    return receiver;
}

zportal::Receiver::Receiver(Receiver&& other) noexcept
    : ring_(std::exchange(other.ring_, nullptr)), tun_(std::exchange(other.tun_, nullptr)),
      bg_(std::exchange(other.bg_, nullptr)), socket_(std::exchange(other.socket_, nullptr)),
      cooling_down_(std::exchange(other.cooling_down_, false)), used_buffers_(std::exchange(other.used_buffers_, 0)),
      input_buffer_queue_(std::move(other.input_buffer_queue_)), buffer_refcounts_(std::move(other.buffer_refcounts_)),
      state_(std::exchange(other.state_, {})), header_(std::exchange(other.header_, {})),
      header_progress_(std::exchange(other.header_progress_, 0)), frame_(std::move(other.frame_)),
      payload_progress_(std::exchange(other.payload_progress_, 0)),
      output_frame_queue_(std::move(other.output_frame_queue_)),
      write_in_progress_(std::exchange(other.write_in_progress_, false)) {}

zportal::Receiver& zportal::Receiver::operator=(Receiver&& other) noexcept {
    if (&other == this)
        return *this;

    ring_ = std::exchange(other.ring_, nullptr);
    tun_ = std::exchange(other.tun_, nullptr);
    bg_ = std::exchange(other.bg_, nullptr);
    socket_ = std::exchange(other.socket_, nullptr);
    cooling_down_ = std::exchange(other.cooling_down_, false);
    used_buffers_ = std::exchange(other.used_buffers_, 0);
    input_buffer_queue_ = std::move(other.input_buffer_queue_);
    buffer_refcounts_ = std::move(other.buffer_refcounts_);
    state_ = std::exchange(other.state_, {});
    header_ = std::exchange(other.header_, {});
    header_progress_ = std::exchange(other.header_progress_, 0);
    frame_ = std::move(other.frame_);
    payload_progress_ = std::exchange(other.payload_progress_, 0);
    output_frame_queue_ = std::move(other.output_frame_queue_);
    write_in_progress_ = std::exchange(other.write_in_progress_, false);

    return *this;
}

zportal::Result<void> zportal::Receiver::arm_recv() noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidReceiver);

    auto sqe = ring_->get_sqe();
    if (!sqe)
        return fail(sqe.error());

    Operation operation;
    operation.set_type(OperationType::RECV);

    ::io_uring_prep_recv_multishot(*sqe, socket_->get(), nullptr, 0, 0);
    ::io_uring_sqe_set_flags(*sqe, IOSQE_BUFFER_SELECT);
    (*sqe)->buf_group = bg_->get_bgid();
    ::io_uring_sqe_set_data64(*sqe, operation.serialize());

    const auto submit_result = ring_->submit();
    if (!submit_result)
        return fail(submit_result.error());

    return {};
}

zportal::Result<void> zportal::Receiver::handle_cqe(const Cqe& cqe) noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidReceiver);

    const auto type = cqe.operation().get_type();
    if (type != OperationType::RECV && type != OperationType::WRITE)
        return fail(ErrorCode::WrongOperationType);

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
        return fail(ErrorCode::InvalidReceiver);

    if (cqe.operation().get_type() != OperationType::WRITE)
        return fail(ErrorCode::WrongOperationType);

    write_in_progress_ = false;

    if (output_frame_queue_.empty())
        return fail(ErrorCode::WriteUnknownFrame);

    if (!cqe.ok())
        return fail({ErrorCode::TunWriteFailed, cqe.error()});

    const auto& frame = output_frame_queue_.front();
    const std::size_t frame_size = ([&]() {
        std::size_t size{};
        for (const auto& segment : frame.segments)
            size += static_cast<std::size_t>(segment.iov_len);

        return size;
    })();

    const std::size_t written = static_cast<std::size_t>(cqe.result());
    if (written != frame_size)
        return fail(ErrorCode::TunPartialWrite);

    for (std::uint16_t bid : frame.bid) {
        buffer_refcounts_[bid]--;
        if (buffer_refcounts_[bid] <= 0) {
            assert(buffer_refcounts_[bid] == 0);

            if (const auto result = bg_->return_buffer(bid); !result)
                return fail(result.error());

            used_buffers_--;
        }
    }

    output_frame_queue_.pop();

    if (cooling_down_ && (used_buffers_ < bg_->get_buffer_count() / 2)) {
        if (const auto arm_recv_result = arm_recv(); !arm_recv_result)
            return fail(arm_recv_result.error());

        cooling_down_ = false;
        std::cout << "RX backpressure: LOW watermark, rearming RECV" << std::endl;
    }

    return kick_write_();
}

zportal::Result<void> zportal::Receiver::handle_recv_cqe_(const Cqe& cqe) noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidReceiver);

    if (cqe.operation().get_type() != OperationType::RECV)
        return fail(ErrorCode::WrongOperationType);

    if (!cqe.ok()) {
        if (cqe.error() == ENOBUFS && !cqe.more()) {
            cooling_down_ = true;
            std::cout << "RX backpressure: HIGH watermark, stopping RECV" << std::endl;
            return kick_write_();
        }
        return fail({ErrorCode::RecvFailed, cqe.error()});
    }

    used_buffers_++;

    const std::uint32_t readen = cqe.result();
    if (readen == 0)
        return fail(ErrorCode::PeerClosed);

    const auto bid = cqe.bid();
    if (!bid)
        return fail(ErrorCode::RecvCqeMissingBid);

    try {
        input_buffer_queue_.push({.bid = *bid, .size = static_cast<std::size_t>(readen)});
    } catch (const std::bad_alloc&) {
        return fail(ErrorCode::NotEnoughMemory);
    }

    if (const auto kick_parse_result = kick_parse_(); !kick_parse_result)
        return fail(kick_parse_result.error());

    if (!cqe.more() && !cooling_down_) {
        if (const auto arm_recv_result = arm_recv(); !arm_recv_result)
            return fail(arm_recv_result.error());
    }

    return kick_write_();
}

zportal::Result<void> zportal::Receiver::kick_parse_() noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidReceiver);

    while (!input_buffer_queue_.empty()) {
        InputBuffer& input_buffer = input_buffer_queue_.front();

        if (input_buffer.offset >= input_buffer.size)
            return fail(ErrorCode::InvalidState);

        if (state_ == ParseState::PARSING_HEADER) {
            if (header_progress_ >= FrameHeader::wire_size)
                return fail(ErrorCode::InvalidState);

            auto buffer_span = bg_->get_buffer(input_buffer.bid, static_cast<std::uint32_t>(input_buffer.size));
            if (!buffer_span)
                return fail(buffer_span.error());

            const std::size_t take =
                std::min(input_buffer.size - input_buffer.offset, FrameHeader::wire_size - header_progress_);
            std::memcpy(header_.data().data() + header_progress_, buffer_span->data() + input_buffer.offset, take);

            input_buffer.offset += take;
            header_progress_ += take;

            if (header_progress_ == FrameHeader::wire_size) {
                header_progress_ = 0;

                if (!header_.is_magic_valid())
                    return fail(ErrorCode::InvalidMagic);

                if (header_.get_size() == 0 || header_.get_size() > tun_->get_mtu())
                    return fail(ErrorCode::InvalidSize);

                state_ = ParseState::PARSING_PAYLOAD;
            }

        } else if (state_ == ParseState::PARSING_PAYLOAD) {
            const std::size_t payload_size = static_cast<std::size_t>(header_.get_size());
            if (payload_progress_ >= payload_size)
                return fail(ErrorCode::InvalidState);

            auto buffer_span = bg_->get_buffer(input_buffer.bid, static_cast<std::uint32_t>(input_buffer.size));
            if (!buffer_span)
                return fail(buffer_span.error());

            const std::size_t take =
                std::min(input_buffer.size - input_buffer.offset, payload_size - payload_progress_);

            buffer_refcounts_[input_buffer.bid]++;

            try {
                frame_.bid.push_back(input_buffer.bid);
                frame_.segments.push_back({.iov_base = buffer_span->data() + input_buffer.offset, .iov_len = take});
            } catch (const std::bad_alloc&) {
                return fail(ErrorCode::NotEnoughMemory);
            }

            input_buffer.offset += take;
            payload_progress_ += take;

            if (payload_progress_ == payload_size) {
                payload_progress_ = 0;

                if (header_.get_crc() != crc32c(frame_.segments))
                    return fail(ErrorCode::FrameCrcMismatch);

                try {
                    output_frame_queue_.push(frame_);
                } catch (const std::bad_alloc&) {
                    return fail(ErrorCode::NotEnoughMemory);
                }

                frame_ = OutputFrame{};
                state_ = ParseState::PARSING_HEADER;
            }
        } else
            return fail(ErrorCode::InvalidEnumValue);

        if (input_buffer.offset == input_buffer.size) {
            if (buffer_refcounts_[input_buffer.bid] == 0) {
                if (const auto result = bg_->return_buffer(input_buffer.bid); !result)
                    return fail(result.error());

                used_buffers_--;
            }

            input_buffer_queue_.pop();
        }
    }

    return {};
}

zportal::Result<void> zportal::Receiver::kick_write_() noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidReceiver);

    if (write_in_progress_)
        return {};

    if (output_frame_queue_.empty())
        return {};

    const auto& frame = output_frame_queue_.front();

    auto sqe = ring_->get_sqe();
    if (!sqe)
        return fail(sqe.error());

    Operation operation;
    operation.set_type(OperationType::WRITE);

    ::io_uring_prep_writev(*sqe, tun_->get_fd(), frame.segments.data(),
                           static_cast<unsigned int>(frame.segments.size()), 0);
    ::io_uring_sqe_set_data64(*sqe, operation.serialize());

    if (const auto submit_result = ring_->submit(); !submit_result)
        return fail(submit_result.error());

    write_in_progress_ = true;

    return {};
}