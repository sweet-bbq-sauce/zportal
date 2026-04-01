#include <exception>
#include <source_location>
#include <sstream>
#include <string>

#include <cstdint>

#include <zportal/tools/error.hpp>

zportal::Error::Error(ErrorCode code, const char* message, int sys_errno, std::exception_ptr exception,
                      std::source_location where)
    : code_(code), message_(message), sys_errno_(sys_errno), exception_(exception), where_(where) {}

zportal::ErrorDomain zportal::Error::get_domain() const noexcept {
    const std::uint32_t value = static_cast<std::uint32_t>(code_);

    if (value >= 0x600)
        return ErrorDomain::Internal;
    else if (value >= 0x500)
        return ErrorDomain::Resource;
    else if (value >= 0x400)
        return ErrorDomain::IoUring;
    else if (value >= 0x300)
        return ErrorDomain::Tun;
    else if (value >= 0x200)
        return ErrorDomain::Socket;
    else if (value >= 0x100)
        return ErrorDomain::Protocol;

    return ErrorDomain::None;
}

zportal::ErrorCode zportal::Error::get_code() const noexcept {
    return code_;
}

const char* zportal::Error::get_message() const noexcept {
    return message_;
}

int zportal::Error::get_errno() const noexcept {
    return sys_errno_;
}

std::exception_ptr zportal::Error::get_exception() const noexcept {
    return exception_;
}

std::source_location zportal::Error::where() const {
    return where_;
}

zportal::Error::operator bool() const noexcept {
    return code_ != ErrorCode::None;
}

bool zportal::Error::operator==(const Error& e) const noexcept {
    return code_ == e.code_;
}

std::string zportal::Error::to_string() const {
    std::ostringstream out;

    const auto domain = get_domain();
    switch (domain) {
    case ErrorDomain::None:
        break;

    case ErrorDomain::Protocol: {
        out << "Protocol";
        break;
    }

    case ErrorDomain::Socket: {
        out << "Socket";
        break;
    }

    case ErrorDomain::Tun: {
        out << "TUN device";
        break;
    }

    case ErrorDomain::IoUring: {
        out << "IOUring";
        break;
    }

    case ErrorDomain::Resource: {
        out << "Resource";
        break;
    }

    case ErrorDomain::Internal:
    default:
        out << "Internal";
    }

    if (domain != ErrorDomain::None)
        out << ": ";

    switch (code_) {
    case ErrorCode::None: {
        out << "OK";
        break;
    }

    // Protocol errors
    case ErrorCode::InvalidMagic: {
        out << "Invalid magic number";
        break;
    }

    case ErrorCode::InvalidSize: {
        out << "Invalid frame size";
        break;
    }

    case ErrorCode::CrcMismatch: {
        out << "Frame CRC mismatch";
        break;
    }

    // Socket errors
    case ErrorCode::PeerClosed: {
        out << "Peer closed";
        break;
    }

    case ErrorCode::ConnectFailed: {
        out << "Connect failed";
        break;
    }

    case ErrorCode::SendFailed: {
        out << "Send failed";
        break;
    }

    case ErrorCode::RecvFailed: {
        out << "Recv failed";
        break;
    }

    case ErrorCode::SocketInterrupted: {
        out << "Interrupted";
        break;
    }

    // TUN errors
    case ErrorCode::TunOpenFailed: {
        out << "Open failed";
        break;
    }

    case ErrorCode::TunReadFailed: {
        out << "Read failed";
        break;
    }

    case ErrorCode::TunWriteFailed: {
        out << "Write failed";
        break;
    }

    case ErrorCode::TunInterrupted: {
        out << "Interrupted";
        break;
    }

    // IOUring errors
    case ErrorCode::RingSubmitFailed: {
        out << "Submit failed";
        break;
    }

    case ErrorCode::RingWaitFailed: {
        out << "Wait failed";
        break;
    }

    // Resource errors
    case ErrorCode::NotEnoughMemory: {
        out << "Not enough memory";
        break;
    }

    case ErrorCode::MissingSqe: {
        out << "Missing SQE";
        break;
    }

    // Internal errors
    case ErrorCode::Exception: {
        const auto e = get_exception();
        if (e) {
            try {
                std::rethrow_exception(e);
            } catch (const std::exception& e) {
                out << e.what();
            } catch (...) {
                out << "Unknown";
            }
        } else {
            out << "Unknown";
        }
        break;
    }

    case ErrorCode::RecvParserError: {
        out << "RX parser error";
        break;
    }

    case ErrorCode::RecvCqeMissingBid: {
        out << "Recv CQE has no BID";
        break;
    }

    case ErrorCode::SendCqeWithoutFrameId: {
        out << "Send with empty TX queue";
        break;
    }

    case ErrorCode::WriteUnknownFrameId: {
        out << "Invalid frame ID to send";
        break;
    }
    }

    if (sys_errno_ > 0)
        out << " errno: " << sys_errno_;

    if (message_)
        out << " \"" << message_ << "\"";

#if !defined(NDEBUG)
    out << " [" << where_.file_name() << "(" << where_.line() << ":" << where_.column() << ") in `"
        << where_.function_name() << "`]";
#endif

    return out.str();
}