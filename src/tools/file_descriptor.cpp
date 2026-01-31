#include <utility>

#include <cerrno>

#include <unistd.h>

#include <zportal/tools/debug.hpp>
#include <zportal/tools/file_descriptor.hpp>

zportal::FileDescriptor::FileDescriptor(int fd) noexcept : fd_(fd) {}

zportal::FileDescriptor::FileDescriptor(FileDescriptor&& other) noexcept : fd_(std::exchange(other.fd_, invalid_fd)) {}

zportal::FileDescriptor& zportal::FileDescriptor::operator=(FileDescriptor&& other) noexcept {
    if (&other == this)
        return *this;

    close();
    fd_ = std::exchange(other.fd_, invalid_fd);

    return *this;
}

zportal::FileDescriptor::~FileDescriptor() noexcept {
    close();
}

void zportal::FileDescriptor::reset(int fd) noexcept {
    if (fd == fd_)
        return;
    if (is_valid())
        if (::close(fd_) != 0)
            DEBUG_ERRNO(errno, "close");
    fd_ = fd;
}

void zportal::FileDescriptor::close() noexcept {
    reset();
}

int zportal::FileDescriptor::get() const noexcept {
    return fd_;
}

int zportal::FileDescriptor::release() noexcept {
    return std::exchange(fd_, invalid_fd);
}

bool zportal::FileDescriptor::is_valid() const noexcept {
    return fd_ >= 0;
}

zportal::FileDescriptor::operator bool() const noexcept {
    return is_valid();
}

void zportal::FileDescriptor::swap(FileDescriptor& with) noexcept {
    std::swap(fd_, with.fd_);
}

void zportal::swap(FileDescriptor& a, FileDescriptor& b) noexcept {
    a.swap(b);
}