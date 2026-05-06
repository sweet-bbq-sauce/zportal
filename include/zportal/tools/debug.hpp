#include <print>
#include <system_error>

#include <cstdio>

#if !defined(NDEBUG)
    #define DEBUG_LOG(fmt, ...)                                                                                        \
        std::println("[{}:{} {}] " fmt "\n", __FILE__, __LINE__, __func__ __VA_OPT__(, ) __VA_ARGS__)
#else
    #define DEBUG_LOG(...) ((void)0)
#endif

#if !defined(NDEBUG)
    #define DEBUG_ERRNO(err, context)                                                                                  \
        std::println("[{}:{} {}] {}: {} ({})\n", __FILE__, __LINE__, __func__, context,                                \
                     std::system_category().message(err).c_str(), err)
#else
    #define DEBUG_ERRNO(...) ((void)0)
#endif
