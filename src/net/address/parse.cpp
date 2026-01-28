#include <limits>
#include <regex>
#include <stdexcept>
#include <string>

#include <cstdint>

#include <zportal/net/address.hpp>

zportal::Address zportal::parse_address(const std::string& address) {
    static const std::regex re_unix(R"(^unix:(.+)$)");
    static const std::regex re_unixa(R"(^unixa:(.+)$)");
    static const std::regex re_ip4(
        R"(^((
        (?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.
        (?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.
        (?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)\.
        (?:25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)
    )):(\d{1,5})$)");
    static const std::regex re_ip6(R"(^\[([^\]]+)\]:(\d{1,5})$)");
    static const std::regex re_host(R"(^([^\s:\[\]]+):(\d{1,5})$)");

    constexpr auto parse_port = [](const std::string& port_str) {
        const auto unverified_port = std::stoll(port_str);
        if (unverified_port < 0 || unverified_port > std::numeric_limits<std::uint16_t>::max())
            throw std::invalid_argument("port must be 0-65535");

        return static_cast<std::uint16_t>(unverified_port);
    };

    std::smatch m;
    if (std::regex_match(address, m, re_unix))
        return SockAddress::unix_path(m[1].str());

    else if (std::regex_match(address, m, re_unixa))
        return SockAddress::unix_abstract(m[1].str());

    else if (std::regex_match(address, m, re_ip4))
        return SockAddress::ip4_numeric(m[1].str(), parse_port(m[2].str()));

    else if (std::regex_match(address, m, re_ip6))
        return SockAddress::ip6_numeric(m[1].str(), parse_port(m[2].str()));

    else if (std::regex_match(address, m, re_host))
        return HostPair{m[1].str(), parse_port(m[2].str())};

    throw std::invalid_argument("unknown address format");
}

zportal::Cidr zportal::parse_cidr(const std::string& cidr) {
    const auto slash_position = cidr.find('/');
    if (slash_position == std::string::npos)
        throw std::invalid_argument("'/' character is not present");

    const std::string address = cidr.substr(0, slash_position);
    SockAddress sa;
    try {
        sa = SockAddress::ip4_numeric(address, 0);
    } catch (...) {
        try {
            sa = SockAddress::ip6_numeric(address, 0);
        } catch (...) {
            throw;
        }
    }

    const auto unverified_prefix = std::stoul(cidr.substr(slash_position + 1));
    if (unverified_prefix > 128)
        throw std::invalid_argument("invalid prefix");

    return Cidr{sa, static_cast<std::uint8_t>(unverified_prefix)};
}