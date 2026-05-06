#pragma once

#include <expected>
#include <source_location>
#include <string>

#include <cstdint>

namespace zportal {

enum class ErrorDomain : std::uint8_t { None, Protocol, Socket, Tun, IoUring, Resource, Internal, Socks, Resolve };

enum class ErrorCode : std::uint16_t {
    None = 0x000,

    // Protocol errors
    InvalidMagic = 0x100,
    InvalidSize = 257,
    FrameCrcMismatch = 258,

    // Socket errors
    PeerClosed = 0x200,
    SocketCreateFailed = 513,
    InvalidSocketFamily = 514,
    ConnectFailed = 515,
    BindFailed = 516,
    ListenFailed = 517,
    AcceptFailed = 518,
    SendFailed = 519,
    RecvFailed = 520,
    SetSockOptFailed = 521,
    GetSockNameFailed = 522,
    GetPeerNameFailed = 523,
    SendReturnedZero = 524,
    SocketPairFailed = 525,
    InetPtonFailed = 526,
    InetNtopFailed = 527,

    // TUN errors
    TunOpenFailed = 0x300,
    TunReadFailed = 769,
    TunWriteFailed = 770,
    TunPartialWrite = 771,
    TunIoctlFailed = 772,
    TunNameToIndexFailed = 773,
    TunIpConfigFailed = 774,
    NetlinkError = 775,

    // io_uring errors
    RingSubmitFailed = 0x400,
    RingWaitFailed = 1025,
    RingCreateQueueFailed = 1026,
    RingBufferRingSetupFailed = 1027,
    RingProbeNotSupported = 1028,
    RingRegisterBufRingFailed = 1029,

    // Resource errors
    NotEnoughMemory = 0x500,
    NotEnoughSqe = 1281,
    PosixMemalignFailed = 1282,
    SysConfFailed = 1283,

    // Internal errors
    RecvParserError = 0x600,
    RecvCqeMissingBid = 1537,
    ReadCqeMissingBid = 1538,
    SendCqeWithoutFrame = 1539,
    WriteUnknownFrame = 1540,
    AddressParseFailed = 1541,
    RingInvalid = 1542,
    InvalidSocket = 1543,
    InvalidBid = 1544,
    InvalidBgid = 1545,
    InvalidArgument = 1546,
    InvalidBufferGroup = 1547,
    WrongOperationType = 1548,
    InvalidTransmitter = 1549,
    InvalidReceiver = 1550,
    InvalidState = 1551,
    InvalidEnumValue = 1552,

    // SOCKS5 errors
    SocksHostnameTooLong = 0x700,
    SocksConnectFailed = 1793,
    SocksUnsupportedTargetFamily = 1794,
    SocksAuthMethodUnsupported = 1795,

    // DNS resolving errors
    ResolveNotFound = 0x800,
    ResolveInvalidRequest = 2049,
    ResolveSystemError = 2050,
    ResolveTemporaryFailure = 2051,
    ResolveFailed = 2052
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