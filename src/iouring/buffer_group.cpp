#include <optional>

#include <cstddef>

#include <liburing.h>

#include <zportal/iouring/buffer_group.hpp>
#include <zportal/tools/debug.hpp>
#include <zportal/tools/error.hpp>

zportal::BufferGroup::~BufferGroup() noexcept {
    if (!is_valid())
        return;

    if (const int result =
            ::io_uring_free_buf_ring(ring_, br_, static_cast<unsigned int>(buffer_count_), static_cast<int>(bgid_));
        result < 0)
        DEBUG_ERRNO(-result, "io_uring_free_buf_ring()");

    br_ = nullptr;
}

zportal::Result<std::span<std::byte>> zportal::BufferGroup::get_buffer(std::uint16_t bid,
                                                                       std::optional<std::uint32_t> size) noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidBufferGroup);

    if (bid >= buffer_count_)
        return fail(ErrorCode::InvalidBid);

    if (size && *size > buffer_size_)
        return fail(ErrorCode::InvalidSize);

    std::byte* ptr = data_.get();
    const std::size_t offset = static_cast<std::size_t>(bid) * static_cast<std::size_t>(buffer_size_);
    return std::span<std::byte>{ptr + offset, static_cast<std::size_t>(size ? *size : buffer_size_)};
}

zportal::Result<void> zportal::BufferGroup::return_buffer(std::uint16_t bid) noexcept {
    if (!is_valid())
        return fail(ErrorCode::InvalidBufferGroup);

    if (bid >= buffer_count_)
        return fail(ErrorCode::InvalidBid);

    auto buffer = get_buffer(bid);
    if (!buffer)
        return fail(buffer.error());

    ::io_uring_buf_ring_add(br_, buffer->data(), buffer->size(), bid, mask_, 0);
    ::io_uring_buf_ring_advance(br_, 1);

    return {};
}

std::size_t zportal::BufferGroup::get_buffer_count() const noexcept {
    return buffer_count_;
}

std::uint32_t zportal::BufferGroup::get_buffer_size() const noexcept {
    return buffer_size_;
}

std::uint16_t zportal::BufferGroup::get_bgid() const noexcept {
    return bgid_;
}

bool zportal::BufferGroup::is_valid() const noexcept {
    return br_ != nullptr;
}

zportal::BufferGroup::operator bool() const noexcept {
    return is_valid();
}

zportal::BufferGroup::BufferGroup(io_uring* ring) noexcept : ring_(ring) {}