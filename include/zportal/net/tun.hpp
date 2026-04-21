#pragma once

#include <string>

#include <cstdint>

#include <linux/netlink.h>

#include <zportal/net/address.hpp>
#include <zportal/net/socket.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tools/file_descriptor.hpp>

namespace zportal {

struct TunDeviceStats {
    std::uint64_t rx_packets;
    std::uint64_t tx_packets;
    std::uint64_t rx_bytes;
    std::uint64_t tx_bytes;
    std::uint64_t rx_dropped;
    std::uint64_t tx_dropped;
    std::uint64_t rx_errors;
    std::uint64_t tx_errors;
};

class TunDevice {
  public:
    static Result<TunDevice> create_tun_device(const std::string& name, const Cidr& address,
                                               std::uint32_t mtu) noexcept;
    TunDevice() noexcept = default;

    TunDevice(TunDevice&&) noexcept;
    TunDevice& operator=(TunDevice&&) noexcept;

    TunDevice(const TunDevice&) = delete;
    TunDevice& operator=(const TunDevice&) = delete;

    ~TunDevice() noexcept;

    Result<void> set_up() noexcept;
    Result<void> set_down() noexcept;

    Result<TunDeviceStats> get_stats() noexcept;

    int get_fd() const noexcept;
    const std::string& get_name() const noexcept;
    int get_index() const noexcept;
    std::uint32_t get_mtu() const noexcept;

    explicit operator bool() const noexcept;

    void close() noexcept;

  private:
    FileDescriptor fd_;
    Socket nl_;
    int index_{};
    std::string name_;
    std::uint32_t mtu_;

    Result<void> set_mtu_(std::uint32_t mtu) noexcept;
    Result<void> set_cidr_(const Cidr& cidr) noexcept;

    static std::uint32_t nl_next_seq_() noexcept;
    static Result<void> nl_add_attr_(nlmsghdr& nlh, std::size_t maxlen, std::uint16_t type, const void* data,
                                     std::size_t alen) noexcept;
    Result<void> nl_send_raw_(const nlmsghdr& nlh) noexcept;
    Result<void> nl_send_acked_(const nlmsghdr& nlh) noexcept;
};

} // namespace zportal