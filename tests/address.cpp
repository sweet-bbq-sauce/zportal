#include <stdexcept>
#include <string>

#include <sys/socket.h>

#include <gtest/gtest.h>

#include <zportal/address.hpp>

using namespace zportal;

TEST(SockAddress, Empty) {
    SockAddress address;

    EXPECT_EQ(address.family(), AF_UNSPEC);
    EXPECT_EQ(address.length(), 0);

    EXPECT_FALSE(address.is_valid());
    EXPECT_FALSE(address.is_connectable());
    EXPECT_FALSE(address.is_bindable());

    EXPECT_THROW(address.str(), std::logic_error);
}

TEST(SockAddress, ValidIP4) {
    const std::string ip = "100.10.0.1";

    SockAddress address;
    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(ip, 0));
    EXPECT_EQ(address.family(), AF_INET);
}

TEST(SockAddress, InvalidIP4TooBigSegment) {
    const std::string ip = "100.10.300.1";
    EXPECT_THROW(SockAddress::ip4_numeric(ip, 0), std::runtime_error);
}

TEST(SockAddress, InvalidIP4MissingSegment) {
    const std::string ip = "100.10.1";
    EXPECT_THROW(SockAddress::ip4_numeric(ip, 0), std::runtime_error);
}

TEST(SockAddress, IP4Stringify) {
    const std::string ip = "100.10.0.1";
    const std::string ip_with_port = "100.10.0.1:80";

    SockAddress address;
    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(ip, 80));
    EXPECT_EQ(address.str(false), ip);
    EXPECT_EQ(address.str(true), ip_with_port);
}

TEST(SockAddress, IP4IsValid) {
    const std::string ip = "100.10.0.1";

    SockAddress address;
    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(ip, 80));
    EXPECT_TRUE(address.is_valid());
}

TEST(SockAddress, IP4IsConnectable) {
    const std::string connectable = "100.10.0.1";
    const std::string non_connectable = "0.0.0.0";

    SockAddress address;

    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(connectable, 80));
    EXPECT_TRUE(address.is_connectable());

    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(connectable, 0));
    EXPECT_FALSE(address.is_connectable());

    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(non_connectable, 80));
    EXPECT_FALSE(address.is_connectable());

    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(non_connectable, 0));
    EXPECT_FALSE(address.is_connectable());
}

TEST(SockAddress, IP4IsBindable) {
    const std::string connectable = "100.10.0.1";
    const std::string non_connectable = "0.0.0.0";

    SockAddress address;

    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(connectable, 80));
    EXPECT_TRUE(address.is_bindable());

    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(connectable, 0));
    EXPECT_TRUE(address.is_bindable());

    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(non_connectable, 80));
    EXPECT_TRUE(address.is_bindable());

    EXPECT_NO_THROW(address = SockAddress::ip4_numeric(non_connectable, 0));
    EXPECT_TRUE(address.is_bindable());
}

TEST(SockAddress, ValidIP6) {
    const std::string ip = "::";

    SockAddress address;
    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(ip, 0));
    EXPECT_EQ(address.family(), AF_INET6);
}

TEST(SockAddress, IP6Stringify) {
    const std::string ip = "2001:db8::1";
    const std::string ip_with_port = "[2001:db8::1]:80";

    SockAddress address;
    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(ip, 80));
    EXPECT_EQ(address.str(false), ip);
    EXPECT_EQ(address.str(true), ip_with_port);
}

TEST(SockAddress, IP6IsValid) {
    const std::string ip = "2001:db8::1";

    SockAddress address;
    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(ip, 80));
    EXPECT_TRUE(address.is_valid());
}

TEST(SockAddress, IP6IsConnectable) {
    const std::string connectable = "2001:db8::1";
    const std::string non_connectable = "::";

    SockAddress address;

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(connectable, 80));
    EXPECT_TRUE(address.is_connectable());

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(connectable, 0));
    EXPECT_FALSE(address.is_connectable());

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(non_connectable, 80));
    EXPECT_FALSE(address.is_connectable());

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(non_connectable, 0));
    EXPECT_FALSE(address.is_connectable());
}

TEST(SockAddress, IP6IsBindable) {
    const std::string connectable = "2001:db8::1";
    const std::string non_connectable = "::";

    SockAddress address;

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(connectable, 80));
    EXPECT_TRUE(address.is_bindable());

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(connectable, 0));
    EXPECT_TRUE(address.is_bindable());

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(non_connectable, 80));
    EXPECT_TRUE(address.is_bindable());

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(non_connectable, 0));
    EXPECT_TRUE(address.is_bindable());
}

TEST(SockAddress, IP6LinkLocal) {
    const std::string ip = "fe80::1%lo";
    const std::string ip_with_port = "[fe80::1%lo]:80";

    SockAddress address;

    EXPECT_NO_THROW(address = SockAddress::ip6_numeric(ip, 80));
    EXPECT_TRUE(address.is_connectable());

    const auto str1 = address.str(false);
    EXPECT_EQ(str1, ip);

    const auto str2 = address.str(true);
    EXPECT_EQ(str2, ip_with_port);
}