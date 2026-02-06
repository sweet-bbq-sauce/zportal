#include <bit>
#include <stdexcept>
#include <system_error>
#include <utility>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <liburing.h>

#include <zportal/iouring/buffer_ring.hpp>
#include <zportal/iouring/ring.hpp>
#include <zportal/tools/debug.hpp>

zportal::BufferRing::BufferRing(BufferRing&& other) noexcept
    : ring_(std::exchange(other.ring_, nullptr)), br_(std::exchange(other.br_, nullptr)),
      bgid_(std::exchange(other.bgid_, 0)), threshold_(std::exchange(other.threshold_, 0)),
      to_return_queue_(std::move(other.to_return_queue_)), data_(std::exchange(other.data_, nullptr)),
      size_(std::exchange(other.size_, 0)), count_(std::exchange(other.count_, 0)),
      mask_(std::exchange(other.mask_, 0)) {}

zportal::BufferRing& zportal::BufferRing::operator=(BufferRing&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    ring_ = std::exchange(other.ring_, nullptr);
    br_ = std::exchange(other.br_, nullptr);
    bgid_ = std::exchange(other.bgid_, 0);
    threshold_ = std::exchange(other.threshold_, 0);
    to_return_queue_ = std::move(other.to_return_queue_);
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    count_ = std::exchange(other.count_, 0);
    mask_ = std::exchange(other.mask_, 0);

    return *this;
}

zportal::BufferRing::~BufferRing() noexcept {
    close();
}

void zportal::BufferRing::close() noexcept {
    flush_returns();

    if (ring_ && br_) {
#if HAVE_IO_URING_SETUP_BUF_RING
        if (const int result = ::io_uring_free_buf_ring(ring_->get(), br_, count_, bgid_); result < 0)
            DEBUG_ERRNO(-result, "io_uring_free_buf_ring");
#else
        if (const int result = ::io_uring_unregister_buf_ring(ring_->get(), bgid_); result < 0)
            DEBUG_ERRNO(-result, "io_uring_unregister_buf_ring");
        std::free(br_);
#endif
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
    size_ = 0;
    count_ = 0;
    mask_ = 0;
}

bool zportal::BufferRing::is_valid() const noexcept {
    return ring_ && br_ && data_ && count_ && size_;
}

zportal::BufferRing::operator bool() const noexcept {
    return is_valid();
}

std::uint16_t zportal::BufferRing::get_bgid() const noexcept {
    return bgid_;
}

std::uint16_t zportal::BufferRing::get_count() const noexcept {
    return count_;
}

void zportal::BufferRing::return_buffer(std::uint16_t bid) noexcept {
    if (!is_valid())
        return;
    if (!is_bid_valid(bid))
        return;

    if (threshold_ == 0) {
        ::io_uring_buf_ring_add(br_, buffer_ptr(bid), size_, bid, mask_, 0);
        ::io_uring_buf_ring_advance(br_, 1);
    } else {
        if (to_return_queue_.size() >= threshold_)
            flush_returns();

        to_return_queue_.push_back(bid);
    }
}

void zportal::BufferRing::flush_returns() noexcept {
    if (!is_valid() || to_return_queue_.empty())
        return;

    std::uint16_t to_commit{};
    for (std::uint16_t bid : to_return_queue_) {
        if (!is_bid_valid(bid))
            continue;

        ::io_uring_buf_ring_add(br_, buffer_ptr(bid), size_, bid, mask_, to_commit);
        to_commit++;
    }

    if (to_commit > 0)
        ::io_uring_buf_ring_advance(br_, to_commit);

    to_return_queue_.clear();
}

bool zportal::BufferRing::is_bid_valid(std::uint16_t bid) const noexcept {
    return bid < count_;
}

std::size_t zportal::BufferRing::buffers_size() const noexcept {
    return std::size_t(count_) * std::size_t(size_);
}

std::byte* zportal::BufferRing::buffer_ptr(std::uint16_t bid) const noexcept {
    return is_valid() && is_bid_valid(bid) ? data_ + std::size_t(size_) * std::size_t(bid) : nullptr;
}

zportal::BufferRing::BufferRing(IOUring& ring, std::uint16_t bgid, std::uint16_t count, std::uint32_t size,
                                std::uint16_t threshold)
    : ring_(&ring), bgid_(bgid), br_(nullptr), count_(count), size_(size), threshold_(threshold), mask_(0) {

    if (!std::has_single_bit(count_) || count_ < 2)
        throw std::invalid_argument("count must be power of 2 and >= 2");
    if (size_ == 0)
        throw std::invalid_argument("size must be > 0");

    if (threshold_ > 0)
        to_return_queue_.reserve(threshold_);

    mask_ = count_ - 1;

    void* new_mem{};
    if (const int result = ::posix_memalign(&new_mem, 4096, std::size_t(count_) * std::size_t(size_)); result != 0)
        throw std::system_error(result, std::system_category(), "posix_memalign");
    data_ = static_cast<std::byte*>(new_mem);

#if HAVE_IO_URING_SETUP_BUF_RING
    int err{};
    br_ = ::io_uring_setup_buf_ring(ring_->get(), count_, bgid_, 0, &err);
    if (!br_) {
        std::free(data_);
        data_ = nullptr;

        if (err == 0)
            err = EIO;
        else
            err = err > 0 ? err : -err;
        throw std::system_error(err, std::system_category(), "io_uring_setup_buf_ring");
    }
#else
    const std::size_t ring_bytes_raw = std::size_t(count_) * sizeof(io_uring_buf);
    const std::size_t ring_bytes = (ring_bytes_raw + 4095) & ~std::size_t(4095);

    void* ring_mem{};
    if (const int result = ::posix_memalign(&ring_mem, 4096, ring_bytes); result != 0) {
        std::free(data_);
        data_ = nullptr;
        throw std::system_error(result, std::system_category(), "posix_memalign");
    }

    br_ = reinterpret_cast<io_uring_buf_ring*>(ring_mem);
#    if HAVE_IO_URING_BUF_RING_INIT
    ::io_uring_buf_ring_init(br_);
#    else
    std::memset(br_, 0, ring_bytes);
#    endif

    io_uring_buf_reg reg{};
    reg.ring_addr = reinterpret_cast<unsigned long>(br_);
    reg.ring_entries = count_;
    reg.bgid = bgid_;

    if (const int result = ::io_uring_register_buf_ring(ring_->get(), &reg, 0); result < 0) {
        std::free(br_);
        br_ = nullptr;

        std::free(data_);
        data_ = nullptr;

        throw std::system_error(-result, std::system_category(), "io_uring_register_buf_ring");
    }
#endif

    for (std::uint16_t i = 0; i < count_; i++)
        ::io_uring_buf_ring_add(br_, data_ + std::size_t(i) * std::size_t(size_), size_, i, mask_, i);

    ::io_uring_buf_ring_advance(br_, count_);
}
