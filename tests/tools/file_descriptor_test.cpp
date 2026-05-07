#include <array>
#include <system_error>

#include <cerrno>

#include <sys/socket.h>

#include <gtest/gtest.h>

#include <zportal/tools/file_descriptor.hpp>

using namespace zportal;

static void create_socketpair(std::array<int, 2>& sv) noexcept {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv.data()) < 0) {
        GTEST_SKIP() << std::system_category().message(errno);
    }
}

static constexpr std::array<int, 2> invalid_fds = {FileDescriptor::invalid_fd, FileDescriptor::invalid_fd};

TEST(FileDescriptor, Default) {
    FileDescriptor fd;

    EXPECT_FALSE(fd);
    EXPECT_EQ(fd.get(), FileDescriptor::invalid_fd);
}

TEST(FileDescriptor, Constructor) {
    auto sv{invalid_fds};
    create_socketpair(sv);

    FileDescriptor fd(sv[0]);
    FileDescriptor peer(sv[1]);

    EXPECT_TRUE(fd);

    EXPECT_EQ(fd.get(), sv[0]);
}

TEST(FileDescriptor, Swap) {
    auto sv{invalid_fds};
    create_socketpair(sv);

    FileDescriptor fd1(sv[0]);
    FileDescriptor fd2(sv[1]);

    swap(fd1, fd2);

    EXPECT_TRUE(fd1);
    EXPECT_TRUE(fd2);
    EXPECT_EQ(fd1.get(), sv[1]);
    EXPECT_EQ(fd2.get(), sv[0]);
}

TEST(FileDescriptor, Close) {
    auto sv{invalid_fds};
    create_socketpair(sv);

    FileDescriptor fd(sv[0]);
    FileDescriptor peer(sv[1]);

    fd.close();

    EXPECT_FALSE(fd);

    EXPECT_EQ(fd.get(), FileDescriptor::invalid_fd);
}

TEST(FileDescriptor, Reset) {
    auto sv{invalid_fds};
    create_socketpair(sv);

    FileDescriptor fd(sv[0]);

    fd.reset(sv[1]);

    EXPECT_TRUE(fd);
    EXPECT_EQ(fd.get(), sv[1]);
}

TEST(FileDescriptor, Release) {
    auto sv{invalid_fds};
    create_socketpair(sv);

    FileDescriptor fd(sv[0]);
    FileDescriptor peer(sv[1]);

    const auto unowned_fd = fd.release();

    EXPECT_FALSE(fd);
    EXPECT_EQ(fd.get(), FileDescriptor::invalid_fd);

    EXPECT_EQ(unowned_fd, sv[0]);

    FileDescriptor owner(unowned_fd);
}

TEST(FileDescriptor, MoveConstructor) {
    auto sv{invalid_fds};
    create_socketpair(sv);

    FileDescriptor fd(sv[0]);
    FileDescriptor peer(sv[1]);

    FileDescriptor new_fd(std::move(fd));

    EXPECT_FALSE(fd);
    EXPECT_EQ(fd.get(), FileDescriptor::invalid_fd);

    EXPECT_TRUE(new_fd);
    EXPECT_EQ(new_fd.get(), sv[0]);
}

TEST(FileDescriptor, MoveOperator) {
    auto sv{invalid_fds};
    create_socketpair(sv);

    FileDescriptor fd1(sv[0]);
    FileDescriptor fd2(sv[1]);

    fd2 = std::move(fd1);

    EXPECT_FALSE(fd1);
    EXPECT_EQ(fd1.get(), FileDescriptor::invalid_fd);

    EXPECT_TRUE(fd2);
    EXPECT_EQ(fd2.get(), sv[0]);
}
