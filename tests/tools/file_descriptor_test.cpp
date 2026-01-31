#include <gtest/gtest.h>

#include <zportal/tools/file_descriptor.hpp>

using namespace zportal;

TEST(FileDescriptor, Default) {
    FileDescriptor fd;

    EXPECT_FALSE(fd);
    EXPECT_FALSE(fd.is_valid());
    EXPECT_EQ(fd.get(), FileDescriptor::invalid_fd);
}

TEST(FileDescriptor, Close) {
    FileDescriptor fd{3};

    EXPECT_TRUE(fd);
    EXPECT_EQ(fd.get(), 3);

    fd.close();

    EXPECT_FALSE(fd);
    EXPECT_EQ(fd.get(), FileDescriptor::invalid_fd);
}

TEST(FileDescriptor, Reset) {
    FileDescriptor fd{3};

    EXPECT_TRUE(fd);
    EXPECT_EQ(fd.get(), 3);

    fd.reset(6);

    EXPECT_TRUE(fd);
    EXPECT_EQ(fd.get(), 6);
}

TEST(FileDescriptor, Swap) {
    FileDescriptor fd1{3}, fd2{5};

    fd1.swap(fd2);

    EXPECT_EQ(fd1.get(), 5);
    EXPECT_EQ(fd2.get(), 3);
}

TEST(FileDescriptor, MoveConstructor) {
    FileDescriptor fd1{3};
    FileDescriptor fd2{std::move(fd1)};

    EXPECT_FALSE(fd1);
    EXPECT_TRUE(fd2);
    EXPECT_EQ(fd2.get(), 3);
}

TEST(FileDescriptor, MoveOperator) {
    FileDescriptor fd1{3}, fd2{5};

    fd2 = std::move(fd1);

    EXPECT_FALSE(fd1);
    EXPECT_TRUE(fd2);
    EXPECT_EQ(fd2.get(), 3);
}