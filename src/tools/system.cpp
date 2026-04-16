#include <cerrno>
#include <cstddef>

#include <unistd.h>

#include <zportal/tools/error.hpp>
#include <zportal/tools/system.hpp>

zportal::Result<std::size_t> zportal::system::get_page_size() noexcept {
    const auto result = ::sysconf(_SC_PAGESIZE);
    if (result < 0)
        return fail({ErrorCode::SysConfFailed, errno});

    return static_cast<std::size_t>(result);
}