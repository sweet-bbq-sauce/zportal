#include <atomic>
#include <thread>
#include <utility>
#include <vector>

#include <cstddef>

#include <liburing.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <zportal/iouring/cqe.hpp>
#include <zportal/tools/debug.hpp>
#include <zportal/tunnel/frame/header.hpp>
#include <zportal/tunnel/frame/parser.hpp>
#include <zportal/tunnel/operation.hpp>
#include <zportal/tunnel/peer.hpp>
#include <zportal/tunnel/tunnel.hpp>

zportal::Tunnel::Tunnel(IOUring&& ring, TUNInterface&& tun, Socket&& sock) noexcept
    : ring_(std::move(ring)), tun_(std::move(tun)),
      peer_({.br{ring_.create_buf_ring(1024, 4096)}, .socket{std::move(sock)}}),
      tun_br_(ring_.create_buf_ring(1024, 2048)) {
    peer_.parser = zportal::FrameParser{peer_.br};
    jt_ = std::jthread(&Tunnel::loop_, this);
}

void zportal::Tunnel::loop_() {
    auto read_sqe = ring_.get_sqe();
    zportal::Operation read_op;
    read_op.set_type(Operation::Type::READ);
    ::io_uring_prep_read_multishot(read_sqe, tun_.get_fd(), 0, -1, tun_br_.get_bgid());
    ::io_uring_sqe_set_data64(read_sqe, read_op.serialize());

    auto recv_sqe = ring_.get_sqe();
    zportal::Operation recv_op;
    recv_op.set_type(Operation::Type::RECV);
    ::io_uring_prep_recv_multishot(recv_sqe, peer_.socket.get(), nullptr, 0, 0);
    ::io_uring_sqe_set_flags(recv_sqe, IOSQE_BUFFER_SELECT);
    ::io_uring_sqe_set_buf_group(recv_sqe, peer_.br.get_bgid());
    ::io_uring_sqe_set_data64(recv_sqe, recv_op.serialize());

    ring_.submit();

    while (running_.load(std::memory_order_acquire)) {
        const Cqe cqe{ring_.wait_cqe()};
        const Operation op{cqe.get_data64()};
        const int result = cqe.get_result();

        if (result < 0) {
            DEBUG_ERRNO(-result, "CQE result");
            continue;
        }

        switch (op.get_type()) {
        case zportal::Operation::Type::RECV: {
            if (result == 0) {
                closing = true;
                break;
            }

            peer_.parser.push_buffer(*cqe.get_buffer_id(), static_cast<std::size_t>(result));
            while (const auto frame = peer_.parser.get_frame()) {
                auto writev_sqe = ring_.get_sqe();
                const auto& segments = peer_.parser.get_frame_by_fd(*frame)->get_segments();
                ::io_uring_prep_writev(writev_sqe, tun_.get_fd(), segments.data(), segments.size(), 0);

                zportal::Operation writev_op;
                writev_op.set_type(Operation::Type::WRITE);
                writev_op.set_bid(static_cast<std::uint16_t>(*frame));
                ::io_uring_sqe_set_data64(writev_sqe, writev_op.serialize());
            }
            ring_.submit();
        } break;

        case zportal::Operation::Type::WRITE: {
            peer_.parser.free_frame(static_cast<std::uint64_t>(op.get_bid()));
        } break;

        case zportal::Operation::Type::READ: {
            const std::byte* ptr = tun_br_.buffer_ptr(*cqe.get_buffer_id());
            std::vector<std::byte> buffer{ptr, ptr + static_cast<std::size_t>(result)};
            tun_br_.return_buffer(*cqe.get_buffer_id());
            peer_.out_queue.emplace_back(OutFrame{std::move(buffer)});
            kick_send_();
        } break;

        case zportal::Operation::Type::SEND: {
            peer_.out_queue.pop_front();
            send_inprogress = false;
            kick_send_();
        } break;

        default:
            continue;
        }
    }
}

void zportal::Tunnel::kick_send_() {
    if (peer_.out_queue.empty() || send_inprogress)
        return;

    send_inprogress = true;
    auto& frame = peer_.out_queue.front();

    const msghdr& hdr = frame.get();

    auto sqe = ring_.get_sqe();
    ::io_uring_prep_sendmsg(sqe, peer_.socket.get(), &hdr, 0);

    Operation op;
    op.set_type(Operation::Type::SEND);
    ::io_uring_sqe_set_data64(sqe, op.serialize());
    ring_.submit();
}