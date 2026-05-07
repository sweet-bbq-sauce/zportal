#include <array>
#include <string>

#include <cstddef>

#include <gtest/gtest.h>

#include <zportal/tools/crc.hpp>

using namespace zportal;

TEST(Crc, FromString) {
    const std::string data = "zportal";

    EXPECT_EQ(crc32c({reinterpret_cast<const std::byte*>(data.data()), data.size()}), 0xa9d08df5U);
}

TEST(Crc, FromStringSegments) {
    const std::string seg1 = "zp";
    const std::string seg2 = "ort";
    const std::string seg3 = "al";

    EXPECT_EQ(crc32c({{reinterpret_cast<const std::byte*>(seg1.data()), seg1.size()},
                      {reinterpret_cast<const std::byte*>(seg2.data()), seg2.size()},
                      {reinterpret_cast<const std::byte*>(seg3.data()), seg3.size()}}),
              0xa9d08df5U);
}

TEST(Crc, FromEmpty) {
    const std::array<std::byte, 0> empty_data;

    EXPECT_EQ(crc32c(empty_data), 0x00000000U);
}

TEST(Crc, FromEmptySegments) {
    const std::array<std::byte, 0> empty_seg1;
    const std::array<std::byte, 0> empty_seg2;
    const std::array<std::byte, 0> empty_seg3;

    EXPECT_EQ(crc32c({empty_seg1, empty_seg2, empty_seg3}), 0x00000000U);
}

TEST(Crc, FromStringSegmentsWithEmptySegments) {
    const std::string seg1 = "zp";
    const std::array<std::byte, 0> empty;
    const std::string seg2 = "ortal";

    EXPECT_EQ(crc32c({{reinterpret_cast<const std::byte*>(seg1.data()), seg1.size()},
                      empty,
                      {reinterpret_cast<const std::byte*>(seg2.data()), seg2.size()}}),
              0xa9d08df5U);
}