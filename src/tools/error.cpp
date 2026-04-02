#include <source_location>
#include <sstream>
#include <string>

#include <cstdint>

#include <zportal/tools/error.hpp>

zportal::Error::Error(ErrorCode code, int sys_errno, const char* message, std::source_location where)
    : code_(code), sys_errno_(sys_errno), message_(message), where_(where) {}

zportal::ErrorDomain zportal::Error::domain() const noexcept {
    const auto domain = static_cast<std::uint32_t>(code_) & 0xF00;

    switch (domain) {
    case 0x000:
        return ErrorDomain::None;
    case 0x100:
        return ErrorDomain::Protocol;
    case 0x200:
        return ErrorDomain::Socket;
    case 0x300:
        return ErrorDomain::Tun;
    case 0x400:
        return ErrorDomain::IoUring;
    case 0x500:
        return ErrorDomain::Resource;
    case 0x600:
        return ErrorDomain::Internal;
    case 0x700:
        return ErrorDomain::Socks;
    case 0x800:
        return ErrorDomain::Resolve;

    default:
        return ErrorDomain::Internal;
    }
}

zportal::ErrorCode zportal::Error::code() const noexcept {
    return code_;
}

const char* zportal::Error::message() const noexcept {
    return message_;
}

int zportal::Error::sys_errno() const noexcept {
    return sys_errno_;
}

std::source_location zportal::Error::where() const {
    return where_;
}

bool zportal::Error::ok() const noexcept {
    return code() == ErrorCode::None;
}

zportal::Error::operator bool() const noexcept {
    return !ok();
}

bool zportal::Error::operator==(const Error& e) const noexcept {
    return code_ == e.code_;
}

bool zportal::Error::operator==(ErrorCode code) const noexcept {
    return code_ == code;
}

std::string zportal::Error::to_string() const {
    std::ostringstream out;

    if (ok()) {
        out << "OK";
    } else {
        out << "Domain: ";
        switch (domain()) {
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
            out << "io_uring";
            break;
        }

        case ErrorDomain::Resource: {
            out << "Resources";
            break;
        }

        case ErrorDomain::Internal: {
            out << "Internal";
            break;
        }

        case ErrorDomain::Socks: {
            out << "Socks5";
            break;
        }

        case ErrorDomain::Resolve: {
            out << "DNS resolving";
            break;
        }

        default:
        }

        out << " Code: " << static_cast<std::uint32_t>(code_);

        if (sys_errno_ > 0)
            out << " Errno: " << sys_errno_;
    }

    if (message_ && *message_)
        out << " Message:" << message_;

#if !defined(NDEBUG)
    out << " [" << where_.file_name() << " (" << where_.line() << ":" << where_.column() << ") in "
        << where_.function_name() << "]";
#endif

    return out.str();
}