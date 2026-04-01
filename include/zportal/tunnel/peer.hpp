#pragma once

#include <sys/socket.h>
#include <sys/uio.h>
#include <utility>
#include <vector>

#include <cstddef>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/iouring/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/crc.hpp>
#include <zportal/tunnel/frame/header.hpp>
#include <zportal/tunnel/frame/parser.hpp>

namespace zportal {

class OutFrame {
  public:
    explicit OutFrame(std::vector<std::byte>&& payload) noexcept : payload_(std::move(payload)) {
        header_.clean();
        header_.set_size(payload_.size());
        header_.set_crc(zportal::crc32c(payload_));
        vec_[0] = {.iov_base = header_.data().data(), .iov_len = FrameHeader::wire_size};
        vec_[1] = {.iov_base = payload_.data(), .iov_len = payload_.size()};
        init_iov_();
    }

    OutFrame(OutFrame&& other) noexcept
        : header_(other.header_), payload_(std::move(other.payload_)), hdr_(other.hdr_) {
        const std::size_t sent = other.sent_bytes_();
        init_iov_();
        advance(sent);
    }

    OutFrame& operator=(OutFrame&& other) noexcept {
        if (this == &other)
            return *this;

        const std::size_t sent = other.sent_bytes_();
        header_ = other.header_;
        payload_ = std::move(other.payload_);
        hdr_ = other.hdr_;
        init_iov_();
        advance(sent);

        return *this;
    }

    OutFrame(const OutFrame&) = delete;
    OutFrame& operator=(const OutFrame&) = delete;

    const msghdr& get() const noexcept {
        return hdr_;
    }

    std::size_t remaining() const noexcept {
        std::size_t total = 0;
        for (std::size_t i = 0; i < hdr_.msg_iovlen; ++i)
            total += hdr_.msg_iov[i].iov_len;
        return total;
    }

    bool complete() const noexcept {
        return remaining() == 0;
    }

    void advance(std::size_t written) noexcept {
        while (written > 0 && hdr_.msg_iovlen > 0) {
            iovec& current = hdr_.msg_iov[0];
            if (written >= current.iov_len) {
                written -= current.iov_len;
                current.iov_len = 0;
                ++hdr_.msg_iov;
                --hdr_.msg_iovlen;
                continue;
            }

            current.iov_base = static_cast<std::byte*>(current.iov_base) + written;
            current.iov_len -= written;
            written = 0;
        }
    }

  private:
    std::size_t total_size_() const noexcept {
        return FrameHeader::wire_size + payload_.size();
    }

    std::size_t sent_bytes_() const noexcept {
        return total_size_() - remaining();
    }

    void init_iov_() noexcept {
        vec_[0] = {.iov_base = header_.data().data(), .iov_len = FrameHeader::wire_size};
        vec_[1] = {.iov_base = payload_.data(), .iov_len = payload_.size()};
        hdr_.msg_iov = vec_;
        hdr_.msg_iovlen = 2;
    }

    FrameHeader header_;
    std::vector<std::byte> payload_;
    msghdr hdr_{};
    iovec vec_[2];
};

struct Peer {
    // RX
    BufferRing br;
    FrameParser parser;

    // TX
    std::deque<OutFrame> out_queue;
    Socket socket;
};

} // namespace zportal
