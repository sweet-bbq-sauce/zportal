#pragma once

#include <sys/socket.h>
#include <sys/uio.h>
#include <utility>
#include <vector>

#include <cstddef>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/iouring/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tunnel/frame/header.hpp>
#include <zportal/tunnel/frame/parser.hpp>

namespace zportal {

class OutFrame {
  public:
    explicit OutFrame(std::vector<std::byte>&& payload) noexcept : payload_(std::move(payload)) {
        header_.clean();
        header_.set_size(payload_.size());
        vec_[0] = {.iov_base = header_.data().data(), .iov_len = FrameHeader::wire_size};
        vec_[1] = {.iov_base = payload_.data(), .iov_len = payload_.size()};
        hdr_.msg_iov = vec_;
        hdr_.msg_iovlen = 2;
    }

    const msghdr& get() const noexcept {
        return hdr_;
    }

  private:
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