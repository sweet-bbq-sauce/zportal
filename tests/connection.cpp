#include <exception>
#include <stdexcept>
#include <thread>

#include <cerrno>

#include <gtest/gtest.h>

#include <sys/socket.h>
#include <zportal/address.hpp>
#include <zportal/connection.hpp>

using namespace zportal;

TEST(Connection, Bind) {
    HostPair bindable{"localhost", 0};

    EXPECT_NO_THROW(create_listener(bindable));
}

TEST(Connection, ConnectToNonConnectable) {
    HostPair non_connectable{"localhost", 0};

    EXPECT_THROW(connect_to(non_connectable), std::logic_error);
}

TEST(Connection, BindAndConnect) {
    HostPair bindable{"localhost", 0};

    Socket listener;
    EXPECT_NO_THROW(listener = create_listener(bindable));

    sockaddr_storage binded_address{};
    socklen_t len = sizeof(binded_address);

    ASSERT_EQ(::getsockname(listener.get_fd(), reinterpret_cast<sockaddr*>(&binded_address), &len), 0)
        << std::error_code{errno, std::system_category()}.message();

    SockAddress connect_address = SockAddress::from_sockaddr(reinterpret_cast<sockaddr*>(&binded_address), len);

    std::exception_ptr accept_ex, connect_ex;
    Socket accepted_socket, client_socket;
    auto accept_thread = std::thread([&listener, &accepted_socket, &accept_ex]() {
        try {
            accepted_socket = accept_from(listener);
        } catch (...) {
            accept_ex = std::current_exception();
        }
    });
    auto connect_thread = std::thread([&client_socket, connect_address, &connect_ex]() {
        try {
            client_socket = connect_to(connect_address);
        } catch (...) {
            connect_ex = std::current_exception();
        }
    });

    accept_thread.join();
    connect_thread.join();

    EXPECT_EQ(accept_ex, nullptr);
    EXPECT_EQ(connect_ex, nullptr);

    EXPECT_TRUE(accepted_socket.is_valid());
    EXPECT_TRUE(client_socket.is_valid());
}