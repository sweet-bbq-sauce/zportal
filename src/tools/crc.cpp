#include <span>
#include <vector>

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(__x86_64__) || defined(__i386__)
    #include <cpuid.h>
    #include <nmmintrin.h>
#endif

#include <zportal/tools/crc.hpp>
#include <zportal/tools/support_check.hpp>

static constexpr auto crc32c_software = [](std::uint32_t& crc, std::span<const std::byte> data) -> void {
    constexpr std::uint32_t poly = 0x82F63B78u; // reflected Castagnoli
    // std::uint32_t crc = 0xFFFFFFFFu;

    for (const std::byte b : data) {
        crc ^= static_cast<std::uint8_t>(b);
        for (int i = 0; i < 8; ++i) {
            const std::uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (poly & mask);
        }
    }
};

#if defined(__x86_64__) || defined(__i386__)
__attribute__((target("sse4.2"))) static inline void crc32c_hardware(std::uint32_t& crc,
                                                                     std::span<const std::byte> data) {
    const auto* ptr = reinterpret_cast<const std::uint8_t*>(data.data());
    std::size_t size = data.size();

    // std::uint32_t crc = 0xFFFFFFFFu;

    #if defined(__x86_64__)
    std::uint64_t c64 = crc;

    while (size && (reinterpret_cast<std::uintptr_t>(ptr) & 7u)) {
        crc = _mm_crc32_u8(crc, *ptr++);
        --size;
    }
    c64 = crc;

    while (size >= 8) {
        std::uint64_t v;
        std::memcpy(&v, ptr, sizeof(v));
        c64 = _mm_crc32_u64(c64, v);
        ptr += 8;
        size -= 8;
    }
    crc = static_cast<std::uint32_t>(c64);
    #endif

    while (size >= 4) {
        std::uint32_t v;
        std::memcpy(&v, ptr, sizeof(v));
        crc = _mm_crc32_u32(crc, v);
        ptr += 4;
        size -= 4;
    }
    while (size--) {
        crc = _mm_crc32_u8(crc, *ptr++);
    }
}

#endif

std::uint32_t zportal::crc32c(std::span<const std::byte> data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;

#if defined(__x86_64__) || defined(__i386__)
    support_check::sse4() ? crc32c_hardware(crc, data) : crc32c_software(crc, data);
#else
    crc32c_software(crc, data);
#endif

    return crc ^ 0xFFFFFFFFu;
}

std::uint32_t zportal::crc32c(const std::vector<std::span<const std::byte>>& data) noexcept {
    std::uint32_t crc = 0xFFFFFFFFu;

    for (const auto& segment : data) {
#if defined(__x86_64__) || defined(__i386__)
        support_check::sse4() ? crc32c_hardware(crc, segment) : crc32c_software(crc, segment);
#else
        crc32c_software(crc, segment);
#endif
    }

    return crc ^ 0xFFFFFFFFu;
}

std::uint32_t zportal::crc32c(const std::vector<iovec>& data) noexcept {
    std::vector<std::span<const std::byte>> tmp;
    tmp.reserve(data.size());
    for (const auto& vec : data)
        tmp.emplace_back(reinterpret_cast<const std::byte*>(vec.iov_base), static_cast<std::size_t>(vec.iov_len));

    return zportal::crc32c(tmp);
}
