#include <vector>

#include <cstdint>
#include <cstring>

#include <endian.h>

#include <gtest/gtest.h>

#include <zportal/tunnel/frame_header.hpp>

using namespace zportal;

TEST(FrameHeader, Decode) {
    const std::uint32_t magic = FrameHeader::magic;
    const std::uint32_t flags = 5346;
    const std::uint32_t size = 2345346;
    const std::uint32_t crc = 54373;

    const std::uint32_t magic_be = ::htobe32(magic);
    const std::uint32_t flags_be = ::htobe32(flags);
    const std::uint32_t size_be = ::htobe32(size);
    const std::uint32_t crc_be = ::htobe32(crc);

    std::vector<std::uint32_t> buffer{magic_be, flags_be, size_be, crc_be};

    if (buffer.size() * sizeof(std::uint32_t) != FrameHeader::wire_size)
        GTEST_SKIP() << "test implementation error";

    FrameHeader header;
    auto header_data = header.data();
    std::memcpy(header_data.data(), buffer.data(), FrameHeader::wire_size);

    EXPECT_EQ(header.get_magic(), magic);
    EXPECT_EQ(header.get_flags(), flags);
    EXPECT_EQ(header.get_size(), size);
    EXPECT_EQ(header.get_crc(), crc);
}