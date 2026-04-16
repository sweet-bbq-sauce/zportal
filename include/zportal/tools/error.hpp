#pragma once

#include <expected>
#include <source_location>
#include <string>

#include <cstdint>

namespace zportal {

enum class ErrorDomain { None, Protocol, Socket, Tun, IoUring, Resource, Internal, Socks, Resolve };

enum class ErrorCode : std::uint32_t {
    None = 0x000,

    // Protocol errors
    InvalidMagic = 0x100,
    InvalidSize,
    FrameCrcMismatch,

    // Socket errors
    PeerClosed = 0x200,
    SocketCreateFailed,
    InvalidSocketFamily,
    ConnectFailed,
    BindFailed,
    ListenFailed,
    AcceptFailed,
    SendFailed,
    RecvFailed,
    SetSockOptFailed,
    GetSockNameFailed,
    GetPeerNameFailed,
    SendReturnedZero,
    SocketPairFailed,

    // TUN errors
    TunOpenFailed = 0x300,
    TunReadFailed,
    TunWriteFailed,
    TunPartialWrite,
    TunIoctlFailed,
    TunNameToIndexFailed,
    TunIpConfigFailed,

    // io_uring errors
    RingSubmitFailed = 0x400,
    RingWaitFailed,
    RingCreateQueueFailed,
    RingBufferRingSetupFailed,
    RingProbeNotSupported,
    RingRegisterBufRingFailed,

    // Resource errors
    NotEnoughMemory = 0x500,
    NotEnoughSqe,
    PosixMemalignFailed,
    SysConfFailed,

    // Internal errors
    RecvParserError = 0x600,
    RecvCqeMissingBid,
    ReadCqeMissingBid,
    SendCqeWithoutFrame,
    WriteUnknownFrame,
    AddressParseFailed,
    RingInvalid,
    InvalidSocket,
    InvalidBid,
    InvalidBgid,
    InvalidArgument,
    InvalidBufferGroup,
    WrongOperationType,
    InvalidTransmitter,
    InvalidReceiver,
    InvalidState,
    InvalidEnumValue,

    // SOCKS5 errors
    SocksHostnameTooLong = 0x700,
    SocksConnectFailed,
    SocksUnsupportedTargetFamily,
    SocksAuthMethodUnsupported,

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
    Error(ErrorCode code, int sys_errno = 0, const char* message = nullptr,
          std::source_location where = std::source_location::current());

    ErrorDomain domain() const noexcept;
    ErrorCode code() const noexcept;

    const char* message() const noexcept;
    int sys_errno() const noexcept;
    std::source_location where() const;

    bool ok() const noexcept;
    explicit operator bool() const noexcept;

    bool operator==(const Error& e) const noexcept;
    bool operator==(ErrorCode code) const noexcept;

    std::string to_string() const;

  private:
    ErrorCode code_{ErrorCode::None};
    const char* message_{};
    int sys_errno_{};
    std::source_location where_{std::source_location::current()};
};

template <typename T> using Result = std::expected<T, Error>;
using Fail = std::unexpected<Error>;

Fail fail(const Error& err) noexcept;

} // namespace zportal