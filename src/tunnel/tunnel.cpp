#include <atomic>
#include <exception>
#include <future>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

#include <cerrno>
#include <cstddef>

#include <liburing.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <zportal/iouring/cqe.hpp>
#include <zportal/tools/config.hpp>
#include <zportal/tools/debug.hpp>
#include <zportal/tunnel/frame/header.hpp>
#include <zportal/tunnel/frame/parser.hpp>
#include <zportal/tunnel/operation.hpp>
#include <zportal/tunnel/peer.hpp>
#include <zportal/tunnel/submission.hpp>
#include <zportal/tunnel/tunnel.hpp>

namespace {

std::size_t frame_size_bytes(const zportal::Frame& frame) noexcept {
    std::size_t total = 0;
    for (const auto& segment : frame.get_segments())
        total += segment.iov_len;
    return total;
}

} // namespace

zportal::Tunnel::Tunnel(IOUring&& ring, TUNInterface&& tun, Socket&& sock, const Config& cfg) noexcept
    : ring_(std::move(ring)), tun_(std::move(tun)),
      peer_({.br{ring_.create_buf_ring(1024, 4096)}, .socket{std::move(sock)}}),
      tun_br_(ring_.create_buf_ring(1024, 2048)), cfg_(&cfg) {
    peer_.parser = zportal::FrameParser{peer_.br, cfg};
    thread_ = std::async(std::launch::async, &Tunnel::loop_, this);
}

void zportal::Tunnel::loop_() {
    try {
        auto read_sqe = ring_.get_sqe();
        if (!read_sqe)
            throw std::bad_alloc();

        zportal::Operation read_op;
        read_op.set_type(Operation::Type::READ);
        prepare_read(read_sqe, read_op, tun_.get_fd(), tun_br_.get_bgid());

        auto recv_sqe = ring_.get_sqe();
        if (!recv_sqe)
            throw std::bad_alloc();

        zportal::Operation recv_op;
        recv_op.set_type(Operation::Type::RECV);
        prepare_recv(recv_sqe, recv_op, peer_.socket.get(), peer_.br.get_bgid());

        ring_.submit();

        while (running_.load(std::memory_order_relaxed)) {
            const Cqe cqe{ring_.wait_cqe()};
            const Operation op{cqe.get_data64()};
            const int result = cqe.get_result();

            if (result < 0) {
                DEBUG_ERRNO(-result, "CQE result");

                if (-result == ECONNRESET)
                    running_.store(false, std::memory_order_relaxed);

                continue;
            }

            switch (op.get_type()) {
            case zportal::Operation::Type::RECV: {
                if (result == 0) {
                    running_.store(false, std::memory_order_relaxed);
                    break;
                }

                if (!support_check::recv_multishot()) {
                    recv_sqe = ring_.get_sqe();
                    if (!recv_sqe)
                        throw std::bad_alloc();

                    prepare_recv(recv_sqe, recv_op, peer_.socket.get(), peer_.br.get_bgid());
                }

                if (peer_.parser.push_buffer(*cqe.get_buffer_id(), static_cast<std::size_t>(result)) !=
                    FrameParser::ParserError::OK) {
                    running_.store(false, std::memory_order_relaxed);
                    throw std::runtime_error("Stream parse error");
                }

                while (const auto frame = peer_.parser.get_frame()) {
                    tun_write_queue_.push_back(*frame);
                }
                kick_write_();
            } break;

            case zportal::Operation::Type::WRITE: {
                Frame* frame = peer_.parser.get_frame_by_fd(op.get_bid());
                if (!frame)
                    throw std::runtime_error("Unknown frame id in WRITE completion");

                const std::size_t expected = frame_size_bytes(*frame);
                if (static_cast<std::size_t>(result) != expected) {
                    running_.store(false, std::memory_order_relaxed);
                    throw std::runtime_error("Short write to TUN");
                }

                peer_.parser.free_frame(op.get_bid());
                write_inprogress = false;
                kick_write_();
            } break;

            case zportal::Operation::Type::READ: {
                const std::byte* ptr = tun_br_.buffer_ptr(*cqe.get_buffer_id());
                std::vector<std::byte> buffer{ptr, ptr + static_cast<std::size_t>(result)};
                tun_br_.return_buffer(*cqe.get_buffer_id());

                if (!support_check::read_multishot()) {
                    read_sqe = ring_.get_sqe();
                    prepare_read(read_sqe, read_op, tun_.get_fd(), tun_br_.get_bgid());
                }

                peer_.out_queue.emplace_back(std::move(buffer));
                kick_send_();
            } break;

            case zportal::Operation::Type::SEND: {
                if (peer_.out_queue.empty())
                    throw std::runtime_error("SEND completion without queued frame");

                auto& frame = peer_.out_queue.front();
                frame.advance(static_cast<std::size_t>(result));
                if (frame.complete())
                    peer_.out_queue.pop_front();

                send_inprogress = false;
                kick_send_();
            } break;

            default:
                continue;
            }

            if (!running_.load(std::memory_order_relaxed))
                break;
        }
    } catch (const std::exception& e) {
        running_.store(false, std::memory_order_relaxed);
        throw;
    }

    running_.store(false, std::memory_order_relaxed);
}

void zportal::Tunnel::kick_send_() {
    if (peer_.out_queue.empty() || send_inprogress)
        return;

    send_inprogress = true;
    auto& frame = peer_.out_queue.front();

    const msghdr& hdr = frame.get();

    auto sqe = ring_.get_sqe();
    if (!sqe)
        throw std::bad_alloc();

    ::io_uring_prep_sendmsg(sqe, peer_.socket.get(), &hdr, 0);

    Operation op;
    op.set_type(Operation::Type::SEND);
    ::io_uring_sqe_set_data64(sqe, op.serialize());
    ring_.submit();
}

void zportal::Tunnel::kick_write_() {
    if (tun_write_queue_.empty() || write_inprogress)
        return;

    Frame* frame = peer_.parser.get_frame_by_fd(tun_write_queue_.front());
    if (!frame)
        throw std::runtime_error("Queued frame is missing");

    auto sqe = ring_.get_sqe();
    if (!sqe)
        throw std::bad_alloc();

    const auto& segments = frame->get_segments();
    ::io_uring_prep_writev(sqe, tun_.get_fd(), segments.data(), segments.size(), 0);

    Operation op;
    op.set_type(Operation::Type::WRITE);
    op.set_bid(tun_write_queue_.front());
    ::io_uring_sqe_set_data64(sqe, op.serialize());

    write_inprogress = true;
    tun_write_queue_.pop_front();
    ring_.submit();
}
