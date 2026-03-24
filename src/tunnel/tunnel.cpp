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
        auto arm_read = [&]() {
            auto sqe = ring_.get_sqe();
            if (!sqe)
                throw std::bad_alloc();

            zportal::Operation op;
            op.set_type(Operation::Type::READ);
            prepare_read(sqe, op, tun_.get_fd(), tun_br_.get_bgid());
        };

        auto arm_recv = [&]() {
            auto sqe = ring_.get_sqe();
            if (!sqe)
                throw std::bad_alloc();

            zportal::Operation op;
            op.set_type(Operation::Type::RECV);
            prepare_recv(sqe, op, peer_.socket.get(), peer_.br.get_bgid());
        };

        auto maybe_rearm_multishot = [&](Operation::Type type, const Cqe& cqe) {
            if ((cqe.get_flags() & IORING_CQE_F_MORE) != 0)
                return false;

            switch (type) {
            case Operation::Type::READ:
                if (support_check::read_multishot()) {
                    arm_read();
                    return true;
                }
                break;

            case Operation::Type::RECV:
                if (support_check::recv_multishot()) {
                    arm_recv();
                    return true;
                }
                break;

            default:
                break;
            }

            return false;
        };

        arm_read();
        arm_recv();

        ring_.submit();

        while (running_.load(std::memory_order_relaxed)) {
            const Cqe cqe{ring_.wait_cqe()};
            const Operation op{cqe.get_data64()};
            const int result = cqe.get_result();

            if (result < 0) {
                DEBUG_ERRNO(-result, "CQE result");

                if (-result == ECONNRESET)
                    running_.store(false, std::memory_order_relaxed);
                else if (-result == ENOBUFS) {
                    peer_.br.flush_returns();
                    tun_br_.flush_returns();
                }

                if (running_.load(std::memory_order_relaxed) && maybe_rearm_multishot(op.get_type(), cqe))
                    ring_.submit();

                continue;
            }

            switch (op.get_type()) {
            case zportal::Operation::Type::RECV: {
                if (result == 0) {
                    running_.store(false, std::memory_order_relaxed);
                    break;
                }

                if (!support_check::recv_multishot())
                    arm_recv();
                else
                    maybe_rearm_multishot(op.get_type(), cqe);

                const auto bid = cqe.get_buffer_id();
                if (!bid)
                    throw std::runtime_error("RECV completion without buffer id");

                if (peer_.parser.push_buffer(*bid, static_cast<std::size_t>(result)) != FrameParser::ParserError::OK) {
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
                peer_.br.flush_returns();
                write_inprogress = false;
                kick_write_();
            } break;

            case zportal::Operation::Type::READ: {
                if (!support_check::read_multishot())
                    arm_read();
                else
                    maybe_rearm_multishot(op.get_type(), cqe);

                const auto bid = cqe.get_buffer_id();
                if (!bid)
                    throw std::runtime_error("READ completion without buffer id");

                const std::byte* ptr = tun_br_.buffer_ptr(*bid);
                std::vector<std::byte> buffer{ptr, ptr + static_cast<std::size_t>(result)};
                tun_br_.return_buffer(*bid);
                tun_br_.flush_returns();

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
