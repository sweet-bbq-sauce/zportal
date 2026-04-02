#pragma once

#include <exception>
#include <expected>
#include <source_location>
#include <string>

#include <cstdint>

namespace zportal {

enum class ErrorDomain { None, Protocol, Socket, Tun, IoUring, Resource, Internal, Socks, Resolve };

enum class ErrorCode : std::uint32_t {
    None = 0x000,
    Shutdown,

    // Protocol errors
    InvalidMagic = 0x100,
    InvalidSize,
    CrcMismatch,

    // Socket errors
    PeerClosed = 0x200,
    InvalidSocket,
    ConnectFailed,
    SendFailed,
    RecvFailed,
    SocketInterrupted,
    BindFailed,
    ListenFailed,
    SetSockOptFailed,
    AcceptFailed,

    // TUN errors
    TunOpenFailed = 0x300,
    TunReadFailed,
    TunWriteFailed,
    TunInterrupted,

    // IOUring errors
    RingSubmitFailed = 0x400,
    RingWaitFailed,

    // Resource errors
    NotEnoughMemory = 0x500,
    MissingSqe,

    // Internal errors
    Exception = 0x600,
    RecvParserError,
    RecvCqeMissingBid,
    SendCqeWithoutFrameId,
    WriteUnknownFrameId,

    // SOCKS5 errors
    SocksPeerClosed = 0x700,
    SocksHostnameTooLong,
    SocksRecvTimeout,
    SocksSendTimeout,
    SocksConnectFailed,
    SocksUnsupportedFamily,
    SocksAuthMethodUnsupported,
    SocksErrno,

    // DNS resolving errors
    ResolveNotFound = 0x800,
    ResolveInvalidRequest,
    ResolveSystemError,
    ResolveTemporaryFailure,
    ResolveFailed
};

class Error {
  public:
    Error() = default;
    explicit Error(ErrorCode code, const char* message = nullptr, int sys_errno = 0,
                   std::exception_ptr exception = nullptr,
                   std::source_location where = std::source_location::current());

    ErrorDomain get_domain() const noexcept;
    ErrorCode get_code() const noexcept;

    const char* get_message() const noexcept;
    int get_errno() const noexcept;
    std::exception_ptr get_exception() const noexcept;
    std::source_location where() const;

    explicit operator bool() const noexcept;
    bool operator==(const Error& e) const noexcept;
    bool operator==(ErrorCode code) const noexcept;

    std::string to_string() const;

  private:
    ErrorCode code_{ErrorCode::None};
    const char* message_{};
    int sys_errno_{};
    std::exception_ptr exception_{};
    std::source_location where_{std::source_location::current()};
};

template <typename T> using Result = std::expected<T, Error>;
using Fail = std::unexpected<Error>;

} // namespace zportal