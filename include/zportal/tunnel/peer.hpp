#pragma once

#include <vector>

#include <cstddef>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/iouring/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tunnel/frame/frame.hpp>
#include <zportal/tunnel/frame/parser.hpp>

namespace zportal {

struct Peer {
    // RX
    BufferRing br;
    FrameParser parser;

    // TX
    std::deque<std::vector<const std::byte>> out_queue;
    Socket socket;
};

} // namespace zportal