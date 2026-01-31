#include <cstdio>

#if !defined(NDEBUG)
#    define DEBUG_LOG(fmt, ...)                                                                                        \
        std::fprintf(stderr, "[%s:%d %s] " fmt "\n", __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__)
#else
#    define DEBUG_LOG(...) ((void)0)
#endif