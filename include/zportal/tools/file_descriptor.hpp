#pragma once

namespace zportal {

class FileDescriptor {
  public:
    FileDescriptor() noexcept = default;
    explicit FileDescriptor(int fd) noexcept;

    FileDescriptor(FileDescriptor&&) noexcept;
    FileDescriptor& operator=(FileDescriptor&&) noexcept;

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    ~FileDescriptor() noexcept;

    void reset(int fd = invalid_fd) noexcept;
    void close() noexcept;

    [[nodiscard]] int get() const noexcept;
    [[nodiscard]] int release() noexcept;

    [[nodiscard]] bool is_valid() const noexcept;
    [[nodiscard]] explicit operator bool() const noexcept;

    void swap(FileDescriptor& with) noexcept;

    static constexpr int invalid_fd{-1};

  private:
    int fd_{invalid_fd};
};

void swap(FileDescriptor& a, FileDescriptor& b) noexcept;

} // namespace zportal