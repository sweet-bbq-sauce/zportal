#include <cstdint>

#include <liburing.h>

#include <gtest/gtest.h>

#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/ring.hpp>

using namespace zportal;

TEST(IOUring, InitDefault) {
    const std::uint32_t entries = 16;
    IOUring ring{entries};

    EXPECT_TRUE(ring);
    EXPECT_TRUE(ring.get()->ring_fd != -1);
    EXPECT_EQ(ring.get_sq_entries(), entries);

    ring.close();
    EXPECT_FALSE(ring);
    EXPECT_TRUE(ring.get()->ring_fd == -1);
    EXPECT_EQ(ring.get_sq_entries(), 0);
}

TEST(IOUring, MoveConstructor) {
    const std::uint32_t entries = 16;
    IOUring source{entries};
    IOUring destination{std::move(source)};

    EXPECT_FALSE(source);
    EXPECT_TRUE(source.get()->ring_fd == -1);
    EXPECT_EQ(source.get_sq_entries(), 0);

    EXPECT_TRUE(destination);
    EXPECT_TRUE(destination.get()->ring_fd != -1);
    EXPECT_EQ(destination.get_sq_entries(), entries);
}

TEST(IOUring, NOPOperation) {
    IOUring ring{16};
    const std::uint64_t user_data = 0x123456789ABCDEF;

    io_uring_sqe* sqe;
    EXPECT_NO_THROW(sqe = ring.get_sqe());

    ::io_uring_sqe_set_data64(sqe, user_data);
    ::io_uring_prep_nop(sqe);
    EXPECT_NO_THROW(ring.submit());

    EXPECT_NO_THROW(Cqe cqe = ring.wait_cqe(); EXPECT_EQ(cqe.get_data64(), user_data););
}