#pragma once

#include <chrono>

#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/iouring.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/tools/error.hpp>

namespace zportal {

class Monitor {
  public:
    static Result<void> print() noexcept;

    static Result<void> arm_timeout(IoUring& ring, std::chrono::milliseconds timeout) noexcept;
    static Result<void> handle_cqe(IoUring& ring, const Cqe& cqe) noexcept;

    static void set_tun_device(const TunDevice& tun_device) noexcept;

  private:
    static const TunDevice* tun_device_;
};

} // namespace zportal