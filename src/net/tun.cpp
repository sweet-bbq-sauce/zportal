#include <string>
#include <utility>

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <fcntl.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <zportal/net/socket.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tools/file_descriptor.hpp>

zportal::Result<zportal::TunDevice> zportal::TunDevice::create_tun_device(const std::string& name, const Cidr& address,
                                                                          std::uint32_t mtu) noexcept {
    TunDevice tun;
    tun.fd_ = FileDescriptor(::open("/dev/net/tun", O_RDWR));
    if (!tun.fd_)
        return fail({ErrorCode::TunOpenFailed, errno});

    ifreq ifr{};
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

    if (::ioctl(tun.get_fd(), TUNSETIFF, &ifr) < 0) {
        const int err = errno;
        tun.close();
        return fail({ErrorCode::TunIoctlFailed, err});
    }

    tun.name_ = ifr.ifr_name;
    tun.index_ = ::if_nametoindex(tun.name_.c_str());
    if (tun.index_ == 0) {
        const int err = errno;
        tun.close();
        return fail({ErrorCode::TunNameToIndexFailed, err});
    }

    tun.nl_ = Socket(::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE));
    if (!tun.nl_)
        return fail({ErrorCode::SocketCreateFailed, errno});

    sockaddr_nl local{};
    local.nl_family = AF_NETLINK;
    if (::bind(tun.nl_.get(), reinterpret_cast<const sockaddr*>(&local), sizeof(local)) < 0)
        return fail({ErrorCode::BindFailed, errno});

    auto result = tun.set_mtu_(mtu);
    if (!result)
        return fail(result.error());

    result = tun.set_cidr_(address);
    if (!result)
        return fail(result.error());

    return tun;
}

zportal::TunDevice::TunDevice(TunDevice&& other) noexcept
    : fd_(std::move(other.fd_)), mtu_(std::exchange(other.mtu_, 0)), index_(std::exchange(other.index_, 0)),
      name_(std::exchange(other.name_, "")), nl_(std::move(other.nl_)) {}

zportal::TunDevice& zportal::TunDevice::operator=(TunDevice&& other) noexcept {
    if (this == &other)
        return *this;

    close();
    fd_ = std::move(other.fd_);
    mtu_ = std::exchange(other.mtu_, 0);
    index_ = std::exchange(other.index_, 0);
    name_ = std::exchange(other.name_, "");
    nl_ = std::move(other.nl_);

    return *this;
}

zportal::TunDevice::~TunDevice() noexcept {
    close();
}

zportal::Result<void> zportal::TunDevice::set_cidr_(const Cidr& cidr) noexcept {
    struct {
        nlmsghdr nlh;
        ifaddrmsg ifa;
        char buf[256];
    } req{};

    const auto& addr = cidr.get_address();
    sa_family_t family;
    const void* ip;
    socklen_t ip_length;

    if (cidr.is_ip4()) {
        family = AF_INET;
        const sockaddr_in* sa = reinterpret_cast<const sockaddr_in*>(addr.get());
        ip = &sa->sin_addr;
        ip_length = sizeof(in_addr);
    } else {
        family = AF_INET6;
        const sockaddr_in6* sa = reinterpret_cast<const sockaddr_in6*>(addr.get());
        ip = &sa->sin6_addr;
        ip_length = sizeof(in6_addr);
    }

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(ifaddrmsg));
    req.nlh.nlmsg_type = RTM_NEWADDR;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_REPLACE;
    req.nlh.nlmsg_seq = nl_next_seq_();

    req.ifa.ifa_family = family;
    req.ifa.ifa_prefixlen = cidr.get_prefix();
    req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
    req.ifa.ifa_index = index_;

    auto result = nl_add_attr_(req.nlh, sizeof(req), IFA_LOCAL, ip, ip_length);
    if (!result)
        return fail(result.error());
    result = nl_add_attr_(req.nlh, sizeof(req), IFA_ADDRESS, ip, ip_length);
    if (!result)
        return fail(result.error());

    if (cidr.is_ip4()) {
        result = nl_add_attr_(req.nlh, sizeof(req), IFA_LABEL, name_.c_str(), name_.size() + 1);
        if (!result)
            return fail(result.error());
    }

    return nl_send_acked_(req.nlh);
}

zportal::Result<void> zportal::TunDevice::set_mtu_(std::uint32_t mtu) noexcept {
    struct {
        nlmsghdr nlh;
        ifinfomsg ifi;
        char buf[256];
    } req{};

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
    req.nlh.nlmsg_type = RTM_NEWLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    req.nlh.nlmsg_seq = nl_next_seq_();

    req.ifi.ifi_family = AF_UNSPEC;
    req.ifi.ifi_index = index_;
    req.ifi.ifi_change = 0xFFFFFFFFu;

    auto result = nl_add_attr_(req.nlh, sizeof(req), IFLA_MTU, &mtu, sizeof(mtu));
    if (!result)
        return fail(result.error());

    result = nl_send_acked_(req.nlh);
    if (!result)
        return fail(result.error());

    mtu_ = mtu;

    return {};
}

zportal::Result<void> zportal::TunDevice::set_up() noexcept {
    struct {
        nlmsghdr nlh;
        ifinfomsg ifi;
        char buf[128];
    } req{};

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
    req.nlh.nlmsg_type = RTM_NEWLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    req.nlh.nlmsg_seq = nl_next_seq_();

    req.ifi.ifi_family = AF_UNSPEC;
    req.ifi.ifi_index = index_;
    req.ifi.ifi_change = IFF_UP;
    req.ifi.ifi_flags = IFF_UP;

    return nl_send_acked_(req.nlh);
}

zportal::Result<void> zportal::TunDevice::set_down() noexcept {
    struct {
        nlmsghdr nlh;
        ifinfomsg ifi;
        char buf[128];
    } req{};

    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(ifinfomsg));
    req.nlh.nlmsg_type = RTM_NEWLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    req.nlh.nlmsg_seq = nl_next_seq_();

    req.ifi.ifi_family = AF_UNSPEC;
    req.ifi.ifi_index = index_;
    req.ifi.ifi_change = IFF_UP;
    req.ifi.ifi_flags = 0;

    return nl_send_acked_(req.nlh);
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

std::uint32_t zportal::TunDevice::nl_next_seq_() noexcept {
    static std::uint32_t seq = 1;
    return seq++;
}

zportal::Result<void> zportal::TunDevice::nl_add_attr_(nlmsghdr& nlh, std::size_t maxlen, std::uint16_t type,
                                                       const void* data, std::size_t alen) noexcept {
    const std::size_t len = RTA_LENGTH(alen);
    const std::size_t new_len = NLMSG_ALIGN(nlh.nlmsg_len) + RTA_ALIGN(len);

    if (new_len > maxlen)
        return fail({ErrorCode::InvalidArgument, 0, "netlink message too small for attribute"});

    auto* rta = reinterpret_cast<rtattr*>(reinterpret_cast<char*>(&nlh) + NLMSG_ALIGN(nlh.nlmsg_len));

    rta->rta_type = type;
    rta->rta_len = static_cast<unsigned short>(len);
    std::memcpy(RTA_DATA(rta), data, alen);
    nlh.nlmsg_len = static_cast<std::uint32_t>(new_len);

    return {};
}

zportal::Result<void> zportal::TunDevice::nl_send_raw_(const nlmsghdr& nlh) noexcept {
    sockaddr_nl peer{};
    peer.nl_family = AF_NETLINK;

    iovec iov{.iov_base = const_cast<nlmsghdr*>(&nlh), .iov_len = nlh.nlmsg_len};

    msghdr msg{};
    msg.msg_name = &peer;
    msg.msg_namelen = sizeof(peer);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (::sendmsg(nl_.get(), &msg, 0) < 0)
        return fail({ErrorCode::SendFailed, errno});

    return {};
}

zportal::Result<void> zportal::TunDevice::nl_send_acked_(const nlmsghdr& nlh) noexcept {
    const auto result = nl_send_raw_(nlh);
    if (!result)
        return fail(result.error());

    alignas(nlmsghdr) char rxbuf[8192];
    for (;;) {
        ssize_t n = ::recv(nl_.get(), rxbuf, sizeof(rxbuf), 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return fail({ErrorCode::RecvFailed, errno});
        }

        for (nlmsghdr* nh = reinterpret_cast<nlmsghdr*>(rxbuf); NLMSG_OK(nh, static_cast<unsigned>(n));
             nh = NLMSG_NEXT(nh, n)) {

            if (nh->nlmsg_seq != nlh.nlmsg_seq)
                continue;

            if (nh->nlmsg_type == NLMSG_ERROR) {
                auto* err = reinterpret_cast<nlmsgerr*>(NLMSG_DATA(nh));
                if (err->error == 0)
                    return {};

                return fail({ErrorCode::NetlinkError, -err->error});
            }
        }
    }
}