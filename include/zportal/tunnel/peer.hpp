#pragma once

#include <deque>

#include <netinet/in.h>

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
    std::deque<InputDatagram> out_queue;
    Socket socket;
};

} // namespace zportal