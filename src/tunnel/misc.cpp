#include <atomic>

#include <zportal/tunnel/tunnel.hpp>

void zportal::Tunnel::wait() {
    if (thread_.valid())
        thread_.get();
}

void zportal::Tunnel::stop() noexcept {
    running_.store(false, std::memory_order_relaxed);
}

bool zportal::Tunnel::running() const noexcept {
    return running_.load(std::memory_order_relaxed);
}