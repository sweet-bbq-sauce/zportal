#include <exception>
#include <variant>

#include <netdb.h>
#include <sys/socket.h>

#include <zportal/net/address.hpp>
#include <zportal/net/resolve.hpp>
#include <zportal/tools/error.hpp>

zportal::Result<zportal::SockAddress> zportal::resolve(const Address& address) noexcept {
    if (std::holds_alternative<SockAddress>(address))
        return std::get<SockAddress>(address);

    const auto& hostpair = std::get<HostPair>(address);

    addrinfo request{}, *result = nullptr;
    request.ai_family = AF_UNSPEC;
    request.ai_socktype = SOCK_STREAM;

    if (const int code =
            ::getaddrinfo(hostpair.hostname.c_str(), std::to_string(hostpair.port).c_str(), &request, &result);
        code != 0) {
        switch (code) {
        case EAI_NONAME:
            return Fail(ErrorCode::ResolveNotFound);

        case EAI_MEMORY:
            return Fail(ErrorCode::NotEnoughMemory);

        case EAI_AGAIN:
            return Fail(ErrorCode::ResolveTemporaryFailure);

        case EAI_SYSTEM:
            return Fail(Error{ErrorCode::ResolveSystemError, ::gai_strerror(code), code});

        case EAI_BADFLAGS:
        case EAI_FAMILY:
        case EAI_SOCKTYPE:
        case EAI_SERVICE:
#if defined(EAI_ADDRFAMILY)
        case EAI_ADDRFAMILY:
#endif
            return Fail(ErrorCode::ResolveInvalidRequest);

        case EAI_FAIL:
        default:
            return Fail(ErrorCode::ResolveFailed);
        }
    }

    SockAddress resolved;
    try {
        resolved = SockAddress::from_sockaddr(result->ai_addr, result->ai_addrlen);
    } catch (...) {
        ::freeaddrinfo(result);
        return Fail(Error{ErrorCode::Exception, "", 0, std::current_exception()});
    }

    ::freeaddrinfo(result);
    return resolved;
}