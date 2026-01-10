#include <crc32intrin.h>
#include <span>

#include <cstddef>
#include <cstdint>

#include <cpuid.h>
#include <nmmintrin.h>

#include <zportal/crc.hpp>

bool zportal::is_sse4_supported() noexcept {
    static const bool supported = ([]() -> bool {
        unsigned eax, ebx, ecx, edx;
        if (!__get_cpuid(1, &eax, &ebx, &ecx, &edx))
            return false;

        return (ecx & bit_SSE4_2) != 0;
    })();

    return supported;
}

constexpr auto crc32c_software = [](std::span<const std::byte> data) -> std::uint32_t {
    constexpr std::uint32_t poly = 0x82F63B78u; // reflected Castagnoli
    std::uint32_t crc = 0xFFFFFFFFu;

    for (const std::byte b : data) {
        crc ^= static_cast<std::uint8_t>(b);
        for (int i = 0; i < 8; ++i) {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (poly & mask);
        }
    }

    return ~crc;
};

#if defined(__x86_64__) || defined(__i386__)
#    include <nmmintrin.h>

__attribute__((target("sse4.2"))) static inline std::uint32_t crc32c_hardware(std::span<const std::byte> data) {
    const auto* ptr = reinterpret_cast<const std::uint8_t*>(data.data());
    std::size_t size = data.size();

    std::uint32_t crc = 0xFFFFFFFFu;

#    if defined(__x86_64__)
    std::uint64_t c64 = crc;

    while (size && (reinterpret_cast<std::uintptr_t>(ptr) & 7u)) {
        crc = _mm_crc32_u8(crc, *ptr++);
        --size;
    }
    c64 = crc;

    while (size >= 8) {
        std::uint64_t v;
        __builtin_memcpy(&v, ptr, sizeof(v));
        c64 = _mm_crc32_u64(c64, v);
        ptr += 8;
        size -= 8;
    }
    crc = static_cast<std::uint32_t>(c64);
#    endif

    while (size >= 4) {
        std::uint32_t v;
        __builtin_memcpy(&v, ptr, sizeof(v));
        crc = _mm_crc32_u32(crc, v);
        ptr += 4;
        size -= 4;
    }
    while (size--) {
        crc = _mm_crc32_u8(crc, *ptr++);
    }

    return ~crc;
}

#endif

std::uint32_t zportal::crc32c(std::span<const std::byte> data) noexcept {
#if defined(__x86_64__) || defined(__i386__)
    return is_sse4_supported() ? crc32c_hardware(data) : crc32c_software(data);
#else
    return crc32c_software(data);
#endif
}