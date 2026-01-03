#include <array>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/in.h>
#include <spawn.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <zportal/tun.hpp>

zportal::TUNInterface::TUNInterface(const std::string& name) : name_(name) {
    fd_ = ::open("/dev/net/tun", O_RDWR);
    if (fd_ < 0)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    ifreq ifr{};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    std::strncpy(ifr.ifr_name, name_.c_str(), IFNAMSIZ - 1);

    if (::ioctl(fd_, TUNSETIFF, &ifr) < 0) {
        close();
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());
    }

    name_ = ifr.ifr_name;
    index_ = ::if_nametoindex(name_.c_str());
    if (index_ == 0) {
        close();
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());
    }
}

zportal::TUNInterface::TUNInterface(TUNInterface&& other) noexcept
    : fd_(std::exchange(other.fd_, -1)), index_(std::exchange(other.index_, 0)), name_(std::exchange(other.name_, "")) {
}

zportal::TUNInterface& zportal::TUNInterface::operator=(TUNInterface&& other) noexcept {
    if (this == &other)
        return *this;

    close();
    fd_ = std::exchange(other.fd_, -1);
    index_ = std::exchange(other.index_, 0);
    name_ = std::exchange(other.name_, "");

    return *this;
}

zportal::TUNInterface::~TUNInterface() noexcept {
    close();
}

void zportal::TUNInterface::set_cidr(in_addr_t address, int prefix) {
    if (prefix > 32)
        throw std::invalid_argument("prefix is too big");

    std::string buffer;
    std::array<char, INET_ADDRSTRLEN> ip4{};
    if (::inet_ntop(AF_INET, &address, ip4.data(), ip4.size()) == nullptr)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    buffer.append(ip4.data());
    buffer.append("/");
    buffer.append(std::to_string(prefix));

    run_ip_command({"addr", "add", buffer, "dev", name_});
}

void zportal::TUNInterface::set_mtu(std::uint32_t mtu) {
    run_ip_command({"link", "set", "dev", name_, "mtu", std::to_string(mtu)});
}

void zportal::TUNInterface::set_up() {
    run_ip_command({"link", "set", "dev", name_, "up"});
}

void zportal::TUNInterface::set_down() {
    run_ip_command({"link", "set", "dev", name_, "down"});
}

int zportal::TUNInterface::get_fd() const noexcept {
    return fd_;
}

const std::string& zportal::TUNInterface::get_name() const noexcept {
    return name_;
}

int zportal::TUNInterface::get_index() const noexcept {
    return index_;
}

zportal::TUNInterface::operator bool() const noexcept {
    return get_fd() >= 0;
}

void zportal::TUNInterface::close() noexcept {
    if (fd_ < 0)
        return;

    ::close(fd_);
    fd_ = -1;
}

extern char** environ;
void zportal::TUNInterface::run_ip_command(const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("ip"));
    for (const auto& arg : args)
        argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);

    pid_t pid;
    const int rc = ::posix_spawnp(&pid, "ip", nullptr, nullptr, argv.data(), environ);
    if (rc != 0)
        throw std::runtime_error(std::error_code{rc, std::system_category()}.message());

    int st;
    if (::waitpid(pid, &st, 0) < 0)
        throw std::runtime_error(std::error_code{errno, std::system_category()}.message());

    if (!(WIFEXITED(st) && WEXITSTATUS(st) == 0))
        throw std::runtime_error("command failed");
}