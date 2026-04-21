#include <chrono>
#include <iostream>

#include <liburing.h>

#include <zportal/iouring/cqe.hpp>
#include <zportal/iouring/iouring.hpp>
#include <zportal/net/tun.hpp>
#include <zportal/session/operation.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tools/monitor.hpp>

const zportal::TunDevice* zportal::Monitor::tun_device_{nullptr};

zportal::Result<void> zportal::Monitor::print() noexcept {
    if (!tun_device_) {
        std::cout << "\r\033[KNo TUN device specified" << std::flush;
        return {};
    }

    const auto stats = tun_device_->get_stats();
    if (!stats)
        return fail(stats.error());

    std::cout << "\r\033[KTotal RX: \033[32m" << stats->rx_bytes << " Bytes\033[0m\t Total TX: \033[31m"
              << stats->tx_bytes << " Bytes\033[0m" << std::flush;

    return {};
}

zportal::Result<void> zportal::Monitor::arm_timeout(zportal::IoUring& ring,
                                                    std::chrono::milliseconds timeout) noexcept {
    auto sqe = ring.get_sqe();
    if (!sqe)
        return fail(sqe.error());

    Operation operation;
    operation.set_type(OperationType::TIMEOUT);

    __kernel_timespec ts{.tv_sec = timeout.count() / 1000, .tv_nsec = (timeout.count() % 1000) * 1000000};

#if defined(IORING_TIMEOUT_MULTISHOT)
    ::io_uring_prep_timeout(*sqe, &ts, 0, IORING_TIMEOUT_MULTISHOT);
#else
    ::io_uring_prep_timeout(*sqe, &ts, 0, 0);
#endif

    ::io_uring_sqe_set_data64(*sqe, operation.serialize());

    const auto submit_result = ring.submit();
    if (!submit_result)
        return fail(submit_result.error());

    return {};
}

zportal::Result<void> zportal::Monitor::handle_cqe(zportal::IoUring& ring, const Cqe& cqe) noexcept {
    if (cqe.operation().get_type() != OperationType::TIMEOUT)
        return fail(ErrorCode::WrongOperationType);

#if defined(IORING_TIMEOUT_MULTISHOT)
    return print();
#else
    const auto print_result = print();
    if (!print_result)
        return fail(print_result.error());

    return arm_timeout(ring, std::chrono::milliseconds(1000));
#endif
}

void zportal::Monitor::set_tun_device(const zportal::TunDevice& tun_device) noexcept {
    tun_device_ = &tun_device;
}