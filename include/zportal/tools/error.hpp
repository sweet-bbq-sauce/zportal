#pragma once

#include <source_location>
#include <variant>

namespace zportal {

enum class ErrorKind { OK, ShutdownRequested, ProtocolError, SocketError, TunError, ResourceExhausted, Internal };
enum class ProtocolErrorCode { InvalidMagic, InvalidSize, CrcMismatch };
enum class SocketErrorCode { PeerClosed, ConnectFailed, SendFailed, RecvFailed };
enum class TunErrorCode { OpenFailed, ReadFailed, WriteFailed };

struct ProtocolErrorInfo {
    ProtocolErrorCode code;
};

struct SocketErrorInfo {
    SocketErrorCode code;
    int fd{-1};
};

struct TunErrorInfo {
    TunErrorCode code;
};

using ErrorInfo = std::variant<std::monostate, ProtocolErrorInfo, SocketErrorInfo, TunErrorInfo>;

struct Error {
    ErrorKind kind{};
    int sys_errno{};
    const char* what{};
    ErrorInfo info{};
    std::source_location where{std::source_location::current()};

    operator bool() const noexcept {
        return kind != ErrorKind::OK;
    }
};

} // namespace zportal