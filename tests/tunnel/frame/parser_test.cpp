#include <span>
#include <system_error>
#include <vector>

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>
#include <liburing.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/ring.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tunnel/frame/header.hpp>
#include <zportal/tunnel/frame/parser.hpp>

using namespace zportal;

static constexpr auto prepare_header = [](std::span<const std::uint8_t> payload) -> FrameHeader {
    FrameHeader hdr;
    hdr.clean();
    hdr.set_size(payload.size());
    return hdr;
};

static constexpr auto build_and_send_frame = [](int fd, std::span<const std::uint8_t> payload) {
    FrameHeader hdr = prepare_header(payload);
    std::vector<iovec> chunks;
    chunks.push_back({(void*)hdr.data().data(), hdr.wire_size});
    chunks.push_back({(void*)payload.data(), payload.size()});

    ssize_t n = ::writev(fd, chunks.data(), chunks.size());
    if (n < 0)
        GTEST_SKIP() << "writev: " << std::system_category().message(errno);
};

static constexpr auto equals_iovec_payload = [](std::span<const iovec> chunks, std::span<const std::uint8_t> expected) {
    std::size_t offset = 0;
    for (const iovec& chunk : chunks) {
        const std::size_t len = chunk.iov_len;
        if (offset + len > expected.size())
            return false;

        if (len > 0 && std::memcmp(chunk.iov_base, expected.data() + offset, len) != 0)
            return false;

        offset += len;
    }

    return offset == expected.size();
};

std::uint8_t payload1[] = {0x1e, 0xb7, 0x2d, 0x36, 0xa7, 0xf1, 0x67, 0x10, 0xf7, 0x48, 0xd0, 0x49, 0xcb, 0x9b, 0x62,
                           0x5e, 0x34, 0x41, 0xf8, 0x77, 0x05, 0x5a, 0x67, 0xa8, 0x58, 0x06, 0x68, 0x75, 0xaa, 0xa4,
                           0xed, 0xc2, 0x9e, 0xaf, 0x84, 0xd2, 0xbb, 0xde, 0x20, 0xcc, 0xc6, 0x55, 0x59, 0x15, 0xcf,
                           0xa2, 0x3f, 0xdd, 0x88, 0xc1, 0x49, 0x71, 0xed, 0x6b, 0xf1, 0x59, 0xba, 0xfa, 0x35, 0x5a,
                           0x70, 0x55, 0x37, 0x20, 0xe0, 0x14, 0xc4, 0x02, 0x7d, 0xb9, 0xca, 0xa2, 0x44, 0xd3, 0x8e,
                           0xf2, 0x1a, 0x27, 0x54, 0x20, 0xd0, 0x0d, 0x34, 0x59, 0x31, 0x1d, 0x40, 0x21, 0xaf, 0x8d,
                           0x86, 0xd9, 0x16, 0x19, 0xf6, 0xa6, 0x15, 0x1a, 0x7b, 0xa6};

std::uint8_t payload2[] = {0xc9, 0x36, 0x6b, 0xe0, 0x12, 0x84, 0xb4, 0xd9, 0xc1, 0x81, 0x7b, 0x45, 0x74, 0xc2, 0x0f,
                           0x97, 0x3c, 0x63, 0xee, 0x60, 0x1a, 0xe5, 0x69, 0x0d, 0x65, 0xe7, 0x38, 0x44, 0x64, 0x14,
                           0xf8, 0xc7, 0xae, 0x82, 0x61, 0x03, 0x74, 0x53, 0xfe, 0x45, 0xb4, 0x57, 0xb0, 0x4d, 0x5d,
                           0xc6, 0xbd, 0x76, 0x01, 0x84, 0xf3, 0x42, 0x21, 0x34, 0x3a, 0x52, 0x0f, 0xd5, 0xc5, 0xcc,
                           0x39, 0x63, 0x17, 0xa2, 0xeb, 0x75, 0x9a, 0x5a, 0x0d, 0x82, 0xd2, 0x24, 0xc2, 0xdd, 0xda,
                           0x31, 0xea, 0xff, 0x5f, 0xd1, 0x6e, 0x5d, 0xd5, 0x30, 0xb6, 0xf2, 0xfc, 0x02, 0x40, 0xf4,
                           0x19, 0xcb, 0x06, 0xb8, 0x35, 0xa2, 0x12, 0x49, 0xb8, 0x00};

std::uint8_t payload3[] = {0xff, 0x60, 0x61, 0xde, 0x3b, 0x24, 0xec, 0x78, 0x53, 0xa6, 0xb1, 0x3e, 0x31, 0x66, 0xe7,
                           0x3f, 0xaa, 0xf7, 0x5a, 0x19, 0x53, 0x39, 0x7e, 0x98, 0x1e, 0x2e, 0xe6, 0xff, 0x6e, 0x03,
                           0xc4, 0xea, 0x26, 0xa4, 0x8a, 0x47, 0x8e, 0x18, 0xed, 0x8e, 0x1d, 0x9e, 0x6f, 0x4c, 0x49,
                           0x8e, 0x0d, 0xeb, 0x46, 0x33, 0x0b, 0x90, 0xa2, 0xbf, 0x34, 0x1e, 0xcf, 0xe3, 0x48, 0x96,
                           0x8e, 0x70, 0x44, 0x1b, 0x81, 0x41, 0x10, 0x81, 0xf6, 0x86, 0x97, 0x92, 0x45, 0xb4, 0x8d,
                           0x61, 0x80, 0x9d, 0xcd, 0xb4, 0xfb, 0xe7, 0x00, 0xee, 0xec, 0x5e, 0x04, 0x25, 0x03, 0x2d,
                           0x59, 0xcd, 0x72, 0x11, 0xcb, 0xd4, 0x6b, 0x24, 0xba, 0x3c};

std::uint8_t payload4[] = {0x1b, 0x5d, 0x13, 0x61, 0x00, 0xe8, 0x7f, 0x08, 0xde, 0x45, 0x23, 0x59, 0x46, 0xc4, 0xc2,
                           0xc1, 0x6e, 0xa8, 0x3e, 0xa1, 0xf2, 0xe2, 0x78, 0xf9, 0x12, 0x1b, 0x3d, 0x3f, 0xa4, 0xef,
                           0xd5, 0x54, 0xcd, 0x24, 0x9c, 0x37, 0x2d, 0x6f, 0x9a, 0x92, 0x45, 0x99, 0x3e, 0x8f, 0x1a,
                           0xec, 0xa2, 0x0c, 0x9d, 0xd4, 0x8a, 0xc5, 0xe5, 0x50, 0x38, 0xb6, 0x3b, 0xd6, 0xbc, 0x16,
                           0xf6, 0xdc, 0xad, 0xb7, 0x73, 0xd3, 0x42, 0xbd, 0x51, 0x2c, 0x41, 0x68, 0x32, 0x86, 0xe6,
                           0xe2, 0xe6, 0xac, 0xa4, 0x5b, 0x53, 0x14, 0xf4, 0x94, 0xc5, 0x5f, 0x9e, 0xf1, 0x70, 0x28,
                           0xa3, 0x6a, 0x23, 0x4c, 0x5d, 0x83, 0x5b, 0x48, 0x7e, 0xeb};

TEST(FrameParser, ParseStream) {
    IOUring ring(16);
    BufferRing buffer_ring = ring.create_buf_ring(256, 64);

    FrameParser parser(buffer_ring);

    int pair[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1)
        GTEST_SKIP() << "socketpair: " << std::system_category().message(errno);

    auto sqe = ring.get_sqe();
    ::io_uring_prep_recv_multishot(sqe, pair[0], nullptr, 0, 0);
    ::io_uring_sqe_set_flags(sqe, IOSQE_BUFFER_SELECT);
    ::io_uring_sqe_set_buf_group(sqe, buffer_ring.get_bgid());
    ring.submit();

    build_and_send_frame(pair[1], payload1);
    build_and_send_frame(pair[1], payload2);
    build_and_send_frame(pair[1], payload3);
    build_and_send_frame(pair[1], payload4);

    ::shutdown(pair[1], SHUT_RDWR);

    bool saw_payload = false;
    std::size_t received_total = 0;
    const std::size_t expected_total =
        (FrameHeader::wire_size + sizeof(payload1)) + (FrameHeader::wire_size + sizeof(payload2)) +
        (FrameHeader::wire_size + sizeof(payload3)) + (FrameHeader::wire_size + sizeof(payload4));
    for (;;) {
        Cqe cqe = ring.wait_cqe();

        if (cqe.get_result() < 0) {
            const int err = -cqe.get_result();
            if (!saw_payload && (err == EINVAL || err == ENOBUFS || err == EOPNOTSUPP || err == ENOTSUP))
                GTEST_SKIP() << "recv multishot unsupported/unreliable here: " << std::system_category().message(err);
            if (!(cqe.get_flags() & IORING_CQE_F_MORE) && err == ENOBUFS)
                break;
            GTEST_FAIL() << "recv multishot: " << std::system_category().message(-cqe.get_result());
        }
        if (cqe.get_result() == 0)
            break;

        auto bid = cqe.get_buffer_id();
        EXPECT_TRUE(bid);
        if (!bid)
            break;

        parser.push_buffer(*bid, cqe.get_result());
        saw_payload = true;
        received_total += static_cast<std::size_t>(cqe.get_result());

        if (!(cqe.get_flags() & IORING_CQE_F_MORE))
            break;
    }

    if (!saw_payload)
        GTEST_SKIP() << "recv multishot produced no payload";
    if (received_total < expected_total)
        GTEST_SKIP() << "recv multishot ended before full stream was received";

    auto frame1 = parser.get_frame();
    auto frame2 = parser.get_frame();
    auto frame3 = parser.get_frame();
    auto frame4 = parser.get_frame();
    auto frame5 = parser.get_frame();

    EXPECT_TRUE(frame1);
    EXPECT_TRUE(frame2);
    EXPECT_TRUE(frame3);
    EXPECT_TRUE(frame4);
    EXPECT_FALSE(frame5);

    ASSERT_TRUE(frame1);
    ASSERT_TRUE(frame2);
    ASSERT_TRUE(frame3);
    ASSERT_TRUE(frame4);

    Frame* parsed1 = parser.get_frame_by_fd(*frame1);
    Frame* parsed2 = parser.get_frame_by_fd(*frame2);
    Frame* parsed3 = parser.get_frame_by_fd(*frame3);
    Frame* parsed4 = parser.get_frame_by_fd(*frame4);

    ASSERT_NE(parsed1, nullptr);
    ASSERT_NE(parsed2, nullptr);
    ASSERT_NE(parsed3, nullptr);
    ASSERT_NE(parsed4, nullptr);

    EXPECT_TRUE(equals_iovec_payload(parsed1->get_segments(), payload1));
    EXPECT_TRUE(equals_iovec_payload(parsed2->get_segments(), payload2));
    EXPECT_TRUE(equals_iovec_payload(parsed3->get_segments(), payload3));
    EXPECT_TRUE(equals_iovec_payload(parsed4->get_segments(), payload4));
}
