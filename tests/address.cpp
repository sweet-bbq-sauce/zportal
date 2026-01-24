#include <array>
#include <span>
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

TEST(SockAddress, ValidUnix) {
    const std::string unix = "some.socket";
    const std::string unixa = "some";
    const std::array<std::byte, 2> unixa_binary = {std::byte{0x00}, std::byte{0x02}};

    SockAddress address1;
    EXPECT_NO_THROW(address1 = SockAddress::unix_path(unix));
    EXPECT_EQ(address1.family(), AF_UNIX);

    SockAddress address2;
    EXPECT_NO_THROW(address2 = SockAddress::unix_abstract(unixa));
    EXPECT_EQ(address2.family(), AF_UNIX);

    SockAddress address3;
    EXPECT_NO_THROW(address3 = SockAddress::unix_abstract(unixa_binary));
    EXPECT_EQ(address3.family(), AF_UNIX);
}

TEST(SockAddress, UnixStringify) {
    const std::string unix = "some.socket";
    const std::string unix_e = "unix:some.socket";

    const std::string unixa = "some";
    const std::string unixa_e = "unixa:some";

    const std::array<std::byte, 2> unixa_binary_raw = {std::byte{0xAF}, std::byte{0x02}};
    const std::string unixa_binary = "AF02";
    const std::string unixa_binary_e = "unixa:AF02";

    SockAddress address1;
    EXPECT_NO_THROW(address1 = SockAddress::unix_path(unix));
    EXPECT_EQ(address1.str(false), unix);
    EXPECT_EQ(address1.str(true), unix_e);

    SockAddress address2;
    EXPECT_NO_THROW(address2 = SockAddress::unix_abstract(unixa));
    EXPECT_EQ(address2.str(false), unixa);
    EXPECT_EQ(address2.str(true), unixa_e);

    SockAddress address3;
    EXPECT_NO_THROW(address3 = SockAddress::unix_abstract(unixa_binary_raw));
    EXPECT_EQ(address3.str(false), unixa_binary);
    EXPECT_EQ(address3.str(true), unixa_binary_e);
}

TEST(SockAddress, UnixIsValid) {
    const std::string unix = "some.socket";
    const std::string unixa = "some";
    const std::array<std::byte, 2> unixa_binary = {std::byte{0xAF}, std::byte{0x02}};

    SockAddress address1;
    EXPECT_NO_THROW(address1 = SockAddress::unix_path(unix));
    EXPECT_TRUE(address1.is_valid());

    SockAddress address2;
    EXPECT_NO_THROW(address2 = SockAddress::unix_abstract(unixa));
    EXPECT_TRUE(address2.is_valid());

    SockAddress address3;
    EXPECT_NO_THROW(address3 = SockAddress::unix_abstract(unixa_binary));
    EXPECT_TRUE(address3.is_valid());
}

TEST(SockAddress, UnixIsConnectable) {
    const std::string unix = "some.socket";
    const std::string unixa = "some";
    const std::array<std::byte, 2> unixa_binary = {std::byte{0xAF}, std::byte{0x02}};

    SockAddress address1;
    EXPECT_NO_THROW(address1 = SockAddress::unix_path(unix));
    EXPECT_TRUE(address1.is_connectable());

    SockAddress address2;
    EXPECT_NO_THROW(address2 = SockAddress::unix_abstract(unixa));
    EXPECT_TRUE(address2.is_connectable());

    SockAddress address3;
    EXPECT_NO_THROW(address3 = SockAddress::unix_abstract(unixa_binary));
    EXPECT_TRUE(address3.is_connectable());
}

TEST(SockAddress, UnixIsBindable) {
    const std::string unix = "some.socket";
    const std::string unixa = "some";
    const std::array<std::byte, 2> unixa_binary = {std::byte{0xAF}, std::byte{0x02}};

    SockAddress address1;
    EXPECT_NO_THROW(address1 = SockAddress::unix_path(unix));
    EXPECT_TRUE(address1.is_bindable());

    SockAddress address2;
    EXPECT_NO_THROW(address2 = SockAddress::unix_abstract(unixa));
    EXPECT_TRUE(address2.is_bindable());

    SockAddress address3;
    EXPECT_NO_THROW(address3 = SockAddress::unix_abstract(unixa_binary));
    EXPECT_TRUE(address3.is_bindable());
}

TEST(SockAddress, IsIPOrUnix) {
    const std::string ip1 = "::";
    const std::string ip2 = "100.10.0.1";
    const std::string unix = "some.socket";

    SockAddress address1;
    EXPECT_NO_THROW(address1 = SockAddress::ip6_numeric(ip1, 80));
    EXPECT_TRUE(address1.is_ip());
    EXPECT_FALSE(address1.is_unix());

    SockAddress address2;
    EXPECT_NO_THROW(address2 = SockAddress::ip4_numeric(ip2, 80));
    EXPECT_TRUE(address2.is_ip());
    EXPECT_FALSE(address2.is_unix());

    SockAddress address3;
    EXPECT_NO_THROW(address3 = SockAddress::unix_path(unix));
    EXPECT_FALSE(address3.is_ip());
    EXPECT_TRUE(address3.is_unix());
}

TEST(ParseAddressString, ToUnixPath) {
    const std::string unix = "unix:some/socket";
    const std::string unix_short = "some/socket";
    Address addr;

    EXPECT_NO_THROW(addr = parse_address(unix));
    EXPECT_EQ(to_string(addr, true), unix);
    EXPECT_EQ(to_string(addr, false), unix_short);
}

TEST(ParseAddressString, ToUnixAbstract) {
    const std::string unix = "unixa:socket name";
    const std::string unix_short = "socket name";
    Address addr;

    EXPECT_NO_THROW(addr = parse_address(unix));
    EXPECT_EQ(to_string(addr, true), unix);
    EXPECT_EQ(to_string(addr, false), unix_short);
}

TEST(ParseAddressString, ToIP4) {
    const std::string ip = "123.45.67.89:8765";
    const std::string ip_short = "123.45.67.89";
    Address addr;

    EXPECT_NO_THROW(addr = parse_address(ip));
    EXPECT_EQ(to_string(addr, true), ip);
    EXPECT_EQ(to_string(addr, false), ip_short);
}

TEST(ParseAddressString, ToUnixIP6) {
    const std::string ip = "[::1]:23456";
    const std::string ip_short = "::1";
    Address addr;

    EXPECT_NO_THROW(addr = parse_address(ip));
    EXPECT_EQ(to_string(addr, true), ip);
    EXPECT_EQ(to_string(addr, false), ip_short);
}

TEST(ParseAddressString, ToHostPair) {
    const std::string hostname1 = "some.site.com:23456";
    const std::string hostname1_short = "some.site.com";

    Address addr1;
    EXPECT_NO_THROW(addr1 = parse_address(hostname1));
    EXPECT_EQ(to_string(addr1, true), hostname1);
    EXPECT_EQ(to_string(addr1, false), hostname1_short);

    const std::string hostname2 = "localhost:23456";
    const std::string hostname2_short = "localhost";

    Address addr2;
    EXPECT_NO_THROW(addr2 = parse_address(hostname2));
    EXPECT_EQ(to_string(addr2, true), hostname2);
    EXPECT_EQ(to_string(addr2, false), hostname2_short);
}

TEST(ParseCidrString, IP4) {
    const std::string ip = "10.1.0.1/24";
    Cidr cidr;

    EXPECT_NO_THROW(cidr = parse_cidr(ip));
    EXPECT_EQ(cidr.str(), ip);
}

TEST(ParseCidrString, IP4InvalidPrefix) {
    const std::string ip = "10.1.0.1/40";
    Cidr cidr;

    EXPECT_THROW(cidr = parse_cidr(ip), std::invalid_argument);
}

TEST(Cidr, AddressToNetwork) {
    const std::string ip1 = "10.1.0.16/24";
    const std::string network_ip1 = "10.1.0.0/24";
    Cidr cidr1;

    EXPECT_NO_THROW(cidr1 = parse_cidr(ip1));

    Cidr network1;
    EXPECT_NO_THROW(network1 = cidr1.get_network());
    EXPECT_EQ(network1.str(), network_ip1);

    const std::string ip2 = "123.4.5.67/13";
    const std::string network_ip2 = "123.0.0.0/13";
    Cidr cidr2;

    EXPECT_NO_THROW(cidr2 = parse_cidr(ip2));

    Cidr network2;
    EXPECT_NO_THROW(network2 = cidr2.get_network());
    EXPECT_EQ(network2.str(), network_ip2);
}

TEST(Cidr, IsHostInNetwork) {
    const std::string network_ip = "10.1.0.0/24";
    const std::string host_ip1 = "10.1.0.61";
    const std::string host_ip2 = "10.0.0.61";
    Cidr network;
    SockAddress host1, host2;

    EXPECT_NO_THROW(network = parse_cidr(network_ip));
    EXPECT_NO_THROW(host1 = SockAddress::ip4_numeric(host_ip1, 0));

    EXPECT_TRUE(network.is_in_network(host1));
    EXPECT_FALSE(network.is_in_network(host2));
}