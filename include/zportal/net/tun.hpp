#pragma once

#include <string>
#include <vector>

#include <cstdint>

#include <netinet/in.h>

#include <zportal/net/address.hpp>

namespace zportal {

class TUNInterface {
  public:
    explicit TUNInterface(const std::string& name);

    TUNInterface(TUNInterface&&) noexcept;
    TUNInterface& operator=(TUNInterface&&) noexcept;

    TUNInterface(const TUNInterface&) = delete;
    TUNInterface& operator=(const TUNInterface&) = delete;

    ~TUNInterface() noexcept;

    void set_cidr(Cidr cidr);
    void set_mtu(std::uint32_t mtu);
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

    int fd_{-1};
    int index_{};
    std::string name_;
    std::uint32_t mtu_;
};

} // namespace zportal