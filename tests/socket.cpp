#include <sys/socket.h>

#include <gtest/gtest.h>

#include <zportal/net/socket.hpp>

using namespace zportal;

TEST(Socket, Default) {
    Socket s;
    EXPECT_FALSE(s.is_valid());
    EXPECT_EQ(s.get(), -1);
    EXPECT_EQ(s.get_family(), AF_UNSPEC);
}

TEST(Socket, CreateSocket) {
    Socket s;
    EXPECT_NO_THROW(s = Socket::create_socket(AF_INET));
    EXPECT_TRUE(s.is_valid());
    EXPECT_GE(s.get(), 0);
    EXPECT_EQ(s.get_family(), AF_INET);

    s.close();

    EXPECT_FALSE(s.is_valid());
    EXPECT_EQ(s.get(), -1);
    EXPECT_EQ(s.get_family(), AF_UNSPEC);
}