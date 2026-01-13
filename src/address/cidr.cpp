#include <stdexcept>
#include <string>

#include <sys/socket.h>

#include <zportal/address.hpp>

zportal::Cidr::Cidr(zportal::SockAddress address, std::uint8_t prefix) : address_(address), prefix_(prefix) {
    if (address.family() == AF_INET) {
        if (prefix > 32)
            throw std::invalid_argument("prefix for IPv4 must be 0-32");
    } else if (address.family() == AF_INET6) {
        if (prefix > 128)
            throw std::invalid_argument("prefix for IPv6 must be 0-128");
    } else
        throw std::invalid_argument("address must be IPv4 or IPv6");
}

const zportal::SockAddress& zportal::Cidr::get_address() const noexcept {
    return address_;
}

const std::uint8_t zportal::Cidr::get_prefix() const noexcept {
    return prefix_;
}

bool zportal::Cidr::is_ip4() const noexcept {
    return address_.family() == AF_INET;
}

bool zportal::Cidr::is_ip6() const noexcept {
    return address_.family() == AF_INET6;
}

bool zportal::Cidr::is_valid() const noexcept {
    return address_.is_ip();
}

zportal::Cidr::operator bool() const noexcept {
    return is_valid();
}

std::string zportal::Cidr::str() const {
    return address_.str(false) + '/' + std::to_string(prefix_);
}