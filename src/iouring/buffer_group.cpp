#include <bit>
#include <new>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <liburing.h>

#include <zportal/iouring/buffer_group.hpp>
#include <zportal/iouring/ring.hpp>

zportal::BufferGroup::BufferGroup(BufferGroup&& other) noexcept
    : ring_(std::exchange(other.ring_, nullptr)), bgid_(std::exchange(other.bgid_, 0)),
      br_(std::exchange(other.br_, nullptr)), threshold_(std::exchange(other.threshold_, 0)),
      to_return_queue_(std::move(other.to_return_queue_)), data_(std::exchange(other.data_, nullptr)),
      count_(std::exchange(other.count_, 0)), size_(std::exchange(other.size_, 0)),
      mask_(std::exchange(other.mask_, 0)) {}

zportal::BufferGroup& zportal::BufferGroup::operator=(BufferGroup&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    ring_ = std::exchange(other.ring_, nullptr);
    bgid_ = std::exchange(other.bgid_, 0);
    br_ = std::exchange(other.br_, nullptr);
    threshold_ = std::exchange(other.threshold_, 0);
    to_return_queue_ = std::move(other.to_return_queue_);
    data_ = std::exchange(other.data_, nullptr);
    count_ = std::exchange(other.count_, 0);
    size_ = std::exchange(other.size_, 0);
    mask_ = std::exchange(other.mask_, 0);

    return *this;
}

zportal::BufferGroup::~BufferGroup() noexcept {
    close();
}

void zportal::BufferGroup::close() noexcept {
    if (!ring_)
        return;

    flush_return();

    if (br_) {
        ::io_uring_unregister_buf_ring(ring_->get(), bgid_);
        ::io_uring_free_buf_ring(ring_->get(), br_, count_, bgid_);
        br_ = nullptr;
    }

    if (data_) {
        std::free(data_);
        data_ = nullptr;
    }

    ring_ = nullptr;
    bgid_ = 0;
    threshold_ = 0;
    to_return_queue_.clear();
    count_ = 0;
    size_ = 0;
    mask_ = 0;
}

bool zportal::BufferGroup::is_valid() const noexcept {
    return ring_ && br_ && data_ && count_ && size_;
}

zportal::BufferGroup::operator bool() const noexcept {
    return is_valid();
}

std::uint16_t zportal::BufferGroup::get_bgid() const noexcept {
    return bgid_;
}

std::uint16_t zportal::BufferGroup::get_count() const noexcept {
    return count_;
}

void zportal::BufferGroup::return_buffer(std::uint16_t bid) noexcept {
    if (!is_valid())
        return;
    if (!is_bid_valid(bid))
        return;

    if (threshold_ == 0) {
        ::io_uring_buf_ring_add(br_, buffer_ptr(bid), size_, bid, mask_, 0);
        ::io_uring_buf_ring_advance(br_, 1);
    } else {
        if (to_return_queue_.size() >= threshold_)
            flush_return();

        to_return_queue_.push_back(bid);
    }
}

void zportal::BufferGroup::flush_return() noexcept {
    if (!is_valid() || to_return_queue_.empty())
        return;

    std::uint16_t to_commit{};
    for (std::uint16_t bid : to_return_queue_) {
        if (!is_bid_valid(bid))
            continue;

        ::io_uring_buf_ring_add(br_, buffer_ptr(bid), size_, bid, mask_, 0);
        to_commit++;
    }
    ::io_uring_buf_ring_advance(br_, to_commit);
    to_return_queue_.clear();
}

bool zportal::BufferGroup::is_bid_valid(std::uint16_t bid) const noexcept {
    return bid < count_;
}

std::size_t zportal::BufferGroup::buffers_size() const noexcept {
    return std::size_t(count_) * std::size_t(size_);
}

std::byte* zportal::BufferGroup::buffer_ptr(std::uint16_t bid) const noexcept {
    return is_valid() && is_bid_valid(bid) ? data_ + std::size_t(size_) * std::size_t(bid) : nullptr;
}

zportal::BufferGroup::BufferGroup(IOUring& ring, std::uint16_t bgid, std::uint16_t count, std::uint32_t size,
                                  std::uint16_t threshold)
    : ring_(&ring), bgid_(bgid), br_(nullptr), count_(count), size_(size), threshold_(threshold), mask_(0) {

    if (std::popcount(count_) != 1 || count_ < 2)
        throw std::invalid_argument("count must be power of 2 and >= 2");
    if (size_ == 0)
        throw std::invalid_argument("size must be > 0");

    if (threshold_ > 0)
        to_return_queue_.reserve(threshold_);

    mask_ = count_ - 1;

    if (::posix_memalign(reinterpret_cast<void**>(&data_), 4096, std::size_t(count_) * std::size_t(size_)) != 0)
        throw std::bad_alloc();

    int err{};
    br_ = ::io_uring_setup_buf_ring(ring_->get(), count_, bgid_, 0, &err);
    if (!br_) {
        std::free(data_);
        data_ = nullptr;
        throw std::system_error(err, std::generic_category(), "io_uring_setup_buf_ring");
    }

    io_uring_buf_reg reg{};
    reg.bgid = bgid_;
    if (const int result = ::io_uring_register_buf_ring(ring_->get(), &reg, 0); result < 0) {
        ::io_uring_free_buf_ring(ring_->get(), br_, count_, bgid_);
        br_ = nullptr;
        std::free(data_);
        data_ = nullptr;
        throw std::system_error(-result, std::generic_category(), "io_uring_register_buf_ring");
    }

    for (std::uint16_t i = 0; i < count_; i++)
        ::io_uring_buf_ring_add(br_, buffer_ptr(i), size_, i, mask_, 0);

    ::io_uring_buf_ring_advance(br_, count_);
}