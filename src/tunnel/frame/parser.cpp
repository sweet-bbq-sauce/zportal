#include <algorithm>
#include <deque>
#include <iostream>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <zportal/tools/config.hpp>
#include <zportal/tools/crc.hpp>
#include <zportal/tools/debug.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tunnel/frame/frame.hpp>
#include <zportal/tunnel/frame/header.hpp>
#include <zportal/tunnel/frame/parser.hpp>

zportal::FrameParser::FrameParser(BufferRing& br, const Config& cfg) : br_(&br), cfg_(&cfg) {
    bid_refcount_.resize(br_->get_count());
}

zportal::Error zportal::FrameParser::push_buffer(std::uint16_t bid, std::size_t size) {
    assert(bid < bid_refcount_.size());

    input_queue_.push_back(Chunk{bid, 0, size});

    for (;;) {
        if (state_ == READING_HEADER) {
            while (read_progress_ < FrameHeader::wire_size && !input_queue_.empty()) {
                Chunk& c = input_queue_.front();

                const std::size_t need = FrameHeader::wire_size - read_progress_;
                const std::size_t take = std::min(need, c.length);

                std::memcpy(header_.data().data() + read_progress_, br_->buffer_ptr(c.bid) + c.offset, take);

                c.offset += take;
                c.length -= take;
                read_progress_ += take;

                if (c.length == 0) {
                    const std::uint16_t done_bid = c.bid;
                    input_queue_.pop_front();
                    bid_to_return_.push_back(done_bid);
                }
            }

            if (read_progress_ < FrameHeader::wire_size)
                break;

            assert(read_progress_ == FrameHeader::wire_size);

            if (header_.get_magic() != FrameHeader::magic)
                return Error(ErrorCode::InvalidMagic);

            const std::size_t frame_size = header_.get_size();
            if (frame_size == 0 || frame_size > static_cast<std::size_t>(cfg_->mtu))
                return Error(ErrorCode::InvalidSize);

            // Ensure we start from a known-empty frame after previous move to `frames_`.
            frame_ = Frame{};

            state_ = READING_PAYLOAD;
            read_progress_ = 0;

            continue;
        }

        if (state_ == READING_PAYLOAD) {
            const std::size_t whole = header_.get_size();

            while (read_progress_ < whole && !input_queue_.empty()) {
                Chunk& c = input_queue_.front();

                const std::size_t need = whole - read_progress_;
                const std::size_t take = std::min(need, c.length);

                std::byte* ptr = br_->buffer_ptr(c.bid) + c.offset;
                frame_.bids_.push_back(c.bid);
                frame_.segments_.push_back(iovec{.iov_base = ptr, .iov_len = take});

                ++bid_refcount_[c.bid];

                c.offset += take;
                c.length -= take;
                read_progress_ += take;

                if (c.length == 0) {
                    const std::uint16_t done_bid = c.bid;
                    input_queue_.pop_front();
                    bid_to_return_.push_back(done_bid);
                }
            }

            if (read_progress_ < whole)
                break;

            assert(read_progress_ == whole);

            const std::uint32_t payload_crc = zportal::crc32c(frame_.get_segments());
            std::cout << "Local: " << payload_crc << std::endl;
            std::cout << "Remote: " << header_.get_crc() << std::endl;
            if (header_.get_crc() != payload_crc)
                return Error(ErrorCode::CrcMismatch);

            ready_frames_.push_back(next_frame_id_);
            frames_.emplace(next_frame_id_++, std::move(frame_));

            state_ = READING_HEADER;
            read_progress_ = 0;

            continue;
        }

        return Error(ErrorCode::RecvParserError);
    }

    return {};
}

std::optional<std::uint16_t> zportal::FrameParser::get_frame() noexcept {
    if (ready_frames_.empty())
        return std::nullopt;

    std::uint64_t out = ready_frames_.front();
    ready_frames_.pop_front();
    return out;
}

void zportal::FrameParser::free_frame(std::uint16_t fd) noexcept {
    auto it = frames_.find(fd);
    if (it == frames_.end())
        return;

    Frame& frame = it->second;
    for (const std::uint16_t bid : frame.bids_) {

        if (bid >= bid_refcount_.size())
            continue;

        assert(bid_refcount_[bid] > 0);

        if (bid_refcount_[bid] > 0)
            --bid_refcount_[bid];
    }

    while (!bid_to_return_.empty()) {
        const std::uint16_t bid = bid_to_return_.front();

        if (bid >= bid_refcount_.size() || bid_refcount_[bid] != 0)
            break;

        br_->return_buffer(bid);
        bid_to_return_.pop_front();
    }

    frames_.erase(it);
}

zportal::Frame* zportal::FrameParser::get_frame_by_fd(std::uint16_t fd) noexcept {
    auto it = frames_.find(fd);
    if (it == frames_.end())
        return nullptr;

    return &it->second;
}

const zportal::Frame* zportal::FrameParser::get_frame_by_fd(std::uint16_t fd) const noexcept {
    auto it = frames_.find(fd);
    if (it == frames_.end())
        return nullptr;

    return &it->second;
}
