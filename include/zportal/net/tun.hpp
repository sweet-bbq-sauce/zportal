#pragma once

#include <string>
#include <vector>

#include <cstdint>

#include <netinet/in.h>

#include <zportal/net/address.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

class TunDevice {
  public:
    static Result<TunDevice> create_tun_device(const std::string& name, Cidr address, std::uint32_t mtu) noexcept;
    TunDevice() noexcept = default;

    TunDevice(TunDevice&&) noexcept;
    TunDevice& operator=(TunDevice&&) noexcept;

    TunDevice(const TunDevice&) = delete;
    TunDevice& operator=(const TunDevice&) = delete;

    ~TunDevice() noexcept;

    void set_up();
    void set_down();

    int get_fd() const noexcept;
    const std::string& get_name() const noexcept;
    int get_index() const noexcept;
    std::uint32_t get_mtu() const noexcept;

    explicit operator bool() const noexcept;

    void close() noexcept;

  private:
    void run_ip_command(const std::vector<std::string>& args);

    void set_cidr_(Cidr cidr);
    void set_mtu_(std::uint32_t mtu);

    int fd_{-1};
    int index_{};
    std::string name_;
    std::uint32_t mtu_;
};

} // namespace zportal