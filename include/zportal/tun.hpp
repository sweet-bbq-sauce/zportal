#pragma once

#include <string>
#include <vector>

#include <cstdint>

#include <netinet/in.h>

namespace zportal {

class TUNInterface {
  public:
    TUNInterface(const std::string& name);

    TUNInterface(TUNInterface&&) noexcept;
    TUNInterface& operator=(TUNInterface&&) noexcept;

    TUNInterface(const TUNInterface&) = delete;
    TUNInterface& operator=(const TUNInterface&) = delete;

    ~TUNInterface() noexcept;

    void set_cidr(in_addr_t address, int prefix);
    void set_mtu(std::uint32_t mtu);
    void set_up();
    void set_down();

    int get_fd() const noexcept;
    const std::string& get_name() const noexcept;
    int get_index() const noexcept;

    void close() noexcept;

  private:
    void run_ip_command(const std::vector<std::string>& args);

    int fd_{-1};
    int index_{};
    std::string name_;
};

} // namespace zportal