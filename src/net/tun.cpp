#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <spawn.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <zportal/net/tun.hpp>
#include <zportal/tools/debug.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tools/file_descriptor.hpp>

zportal::Result<zportal::TunDevice> zportal::TunDevice::create_tun_device(const std::string& name, Cidr address,
                                                                          std::uint32_t mtu) noexcept {
    TunDevice tun;
    tun.fd_ = FileDescriptor(::open("/dev/net/tun", O_RDWR));
    if (!tun.fd_)
        return Fail({ErrorCode::TunOpenFailed, errno});

    ifreq ifr{};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

    if (::ioctl(tun.get_fd(), TUNSETIFF, &ifr) < 0) {
        const int err = errno;
        tun.close();
        return Fail({ErrorCode::TunIoctlFailed, err});
    }

    tun.name_ = ifr.ifr_name;
    tun.index_ = ::if_nametoindex(tun.name_.c_str());
    if (tun.index_ == 0) {
        const int err = errno;
        tun.close();
        return Fail({ErrorCode::TunNameToIndexFailed, err});
    }

    try {
        tun.set_cidr_(address);
        tun.set_mtu_(mtu);
    } catch (...) {
        return Fail(ErrorCode::TunIpConfigFailed);
    }

    return tun;
}

zportal::TunDevice::TunDevice(TunDevice&& other) noexcept
    : fd_(std::move(other.fd_)), mtu_(std::exchange(other.mtu_, 0)), index_(std::exchange(other.index_, 0)),
      name_(std::exchange(other.name_, "")) {}

zportal::TunDevice& zportal::TunDevice::operator=(TunDevice&& other) noexcept {
    if (this == &other)
        return *this;

    close();
    fd_ = std::move(other.fd_);
    mtu_ = std::exchange(other.mtu_, 0);
    index_ = std::exchange(other.index_, 0);
    name_ = std::exchange(other.name_, "");

    return *this;
}

zportal::TunDevice::~TunDevice() noexcept {
    close();
}

void zportal::TunDevice::set_cidr_(Cidr cidr) {
    run_ip_command({"addr", "add", cidr.str(), "dev", name_});
}

void zportal::TunDevice::set_mtu_(std::uint32_t mtu) {
    mtu_ = mtu;
    run_ip_command({"link", "set", "dev", name_, "mtu", std::to_string(mtu)});
}

void zportal::TunDevice::set_up() {
    run_ip_command({"link", "set", "dev", name_, "up"});
}

void zportal::TunDevice::set_down() {
    run_ip_command({"link", "set", "dev", name_, "down"});
}

int zportal::TunDevice::get_fd() const noexcept {
    return fd_.get();
}

const std::string& zportal::TunDevice::get_name() const noexcept {
    return name_;
}

int zportal::TunDevice::get_index() const noexcept {
    return index_;
}

std::uint32_t zportal::TunDevice::get_mtu() const noexcept {
    return mtu_;
}

zportal::TunDevice::operator bool() const noexcept {
    return fd_.is_valid();
}

void zportal::TunDevice::close() noexcept {
    fd_.close();
}

extern char** environ;
void zportal::TunDevice::run_ip_command(const std::vector<std::string>& args) {
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