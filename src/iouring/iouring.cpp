#include <memory>
#include <new>
#include <utility>
#include <vector>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <liburing.h>
#include <unistd.h>

#include <zportal/iouring/buffer_group.hpp>
#include <zportal/iouring/iouring.hpp>
#include <zportal/tools/error.hpp>
#include <zportal/tools/system.hpp>

zportal::Result<zportal::IoUring> zportal::IoUring::create_queue(unsigned entries) noexcept {
    IoUring ring;
    if (const int result = ::io_uring_queue_init(entries, &ring.ring_, 0); result < 0)
        return fail({ErrorCode::RingCreateQueueFailed, -result});

    return ring;
}

zportal::IoUring::IoUring(IoUring&& other) noexcept
    : ring_(std::exchange(other.ring_, invalid_ring_)), buffer_groups_(std::move(other.buffer_groups_)),
      next_bgid_(std::exchange(other.next_bgid_, 0)) {}
zportal::IoUring& zportal::IoUring::operator=(IoUring&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    ring_ = std::exchange(other.ring_, invalid_ring_);
    buffer_groups_ = std::move(other.buffer_groups_);
    next_bgid_ = std::exchange(other.next_bgid_, 0);

    return *this;
}

zportal::IoUring::~IoUring() noexcept {
    close();
}

void zportal::IoUring::close() noexcept {
    if (!is_valid())
        return;

    buffer_groups_.clear();
    next_bgid_ = 0;

    ::io_uring_queue_exit(&ring_);
    ring_ = invalid_ring_;
}

zportal::Result<io_uring_sqe*> zportal::IoUring::get_sqe() noexcept {
    if (!is_valid())
        return fail(ErrorCode::RingInvalid);

    io_uring_sqe* sqe = ::io_uring_get_sqe(&ring_);
    if (!sqe)
        return fail(ErrorCode::NotEnoughSqe);

    return sqe;
}

zportal::Result<unsigned> zportal::IoUring::submit() noexcept {
    if (!is_valid())
        return fail(ErrorCode::RingInvalid);

    const int result = ::io_uring_submit(&ring_);
    if (result < 0)
        return fail({ErrorCode::RingSubmitFailed, -result});

    return static_cast<unsigned>(result);
}

zportal::Result<zportal::Cqe> zportal::IoUring::wait() noexcept {
    if (!is_valid())
        return fail(ErrorCode::RingInvalid);

    io_uring_cqe* cqe = nullptr;
    if (const int result = ::io_uring_wait_cqe(&ring_, &cqe); result < 0)
        return fail({ErrorCode::RingWaitFailed, -result});

    Cqe cqe_copy{*cqe};
    ::io_uring_cqe_seen(&ring_, cqe);

    return cqe_copy;
}

bool zportal::IoUring::is_valid() const noexcept {
    return ring_.ring_fd >= 0;
}

zportal::IoUring::operator bool() const noexcept {
    return is_valid();
}

zportal::Result<zportal::BufferGroup*> zportal::IoUring::create_buffer_group(std::uint16_t length,
                                                                             std::uint32_t buf_size) noexcept {
    if (!is_valid())
        return fail(ErrorCode::RingInvalid);

    if (length == 0 || buf_size == 0)
        return fail(ErrorCode::InvalidArgument);

    auto bg = std::unique_ptr<BufferGroup>(new (std::nothrow) BufferGroup(&ring_));
    if (!bg)
        return fail(ErrorCode::NotEnoughMemory);

    bg->buffer_count_ = length;
    bg->buffer_size_ = buf_size;
    bg->size_ = static_cast<std::size_t>(bg->buffer_count_) * static_cast<std::size_t>(bg->buffer_size_);
    bg->data_ = std::unique_ptr<std::byte[]>(new (std::nothrow) std::byte[bg->size_]);

    if (!bg->data_)
        return fail(ErrorCode::NotEnoughMemory);

    bg->bgid_ = get_next_bgid_();

#if HAVE_IO_URING_SETUP_BUF_RING
    int setup_error{};
    bg->br_ =
        ::io_uring_setup_buf_ring(&ring_, static_cast<unsigned int>(bg->buffer_count_), bg->bgid_, 0, &setup_error);

    if (!bg->br_)
        return fail({ErrorCode::RingBufferRingSetupFailed,
                     setup_error == 0 ? EIO : (setup_error > 0 ? setup_error : -setup_error)});
#else
    const auto page_size = system::get_page_size();
    if (!page_size)
        return fail(page_size.error());

    const std::size_t ring_bytes_raw = std::size_t(bg->buffer_count_) * sizeof(io_uring_buf);
    const std::size_t ring_bytes = ((ring_bytes_raw + *page_size - 1) / *page_size) * *page_size;

    if (const int result = ::posix_memalign(reinterpret_cast<void**>(&bg->br_), *page_size, ring_bytes); result != 0)
        return fail({ErrorCode::PosixMemalignFailed, result});

    #if HAVE_IO_URING_BUF_RING_INIT
    ::io_uring_buf_ring_init(bg->br_);
    #else
    std::memset(bg->br_, 0, ring_bytes);
    #endif

    io_uring_buf_reg reg{};
    reg.ring_addr = reinterpret_cast<unsigned long>(bg->br_);
    reg.ring_entries = static_cast<std::uint32_t>(bg->buffer_count_);
    reg.bgid = bg->bgid_;

    if (const int result = ::io_uring_register_buf_ring(&ring_, &reg, 0); result < 0) {
        std::free(bg->br_);
        bg->br_ = nullptr;

        return fail({ErrorCode::RingRegisterBufRingFailed, -result});
    }
#endif

    bg->mask_ = ::io_uring_buf_ring_mask(static_cast<std::uint32_t>(bg->buffer_count_));
    for (std::uint16_t bid = 0; bid < bg->buffer_count_; bid++) {
        auto buffer = bg->get_buffer(bid);
        if (!buffer)
            return fail(buffer.error());

        ::io_uring_buf_ring_add(bg->br_, buffer->data(), buffer->size(), bid, bg->mask_, static_cast<int>(bid));
    }

    ::io_uring_buf_ring_advance(bg->br_, static_cast<int>(bg->buffer_count_));

    return buffer_groups_.emplace_back(std::move(bg)).get();
}

zportal::Result<zportal::BufferGroup*> zportal::IoUring::get_buffer_group(std::uint16_t bgid) noexcept {
    if (!is_valid())
        return fail(ErrorCode::RingInvalid);

    for (auto& bg : buffer_groups_) {
        if (bg->get_bgid() == bgid)
            return bg.get();
    }

    return fail(ErrorCode::InvalidBgid);
}

std::uint16_t zportal::IoUring::get_next_bgid_() noexcept {
    return next_bgid_++;
}